///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2005, 2006, 2007, 2008, 2009
// University of Rochester
// Department of Computer Science
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the University of Rochester nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef ATOMIC_OPS_H__
#define ATOMIC_OPS_H__

#if defined(_MSC_VER)
#define CFENCE /* we don't appear to need compiler fences in Visual C++ */
#define WBR {__asm {mfence} }
#define ISYNC
#define LWSYNC
#define SYNC

// Visual C++ versions of CAS and TAS
static inline unsigned long
cas(volatile unsigned long* addr, unsigned long old, unsigned long new_value)
{
    __asm {
        mov ebx, new_value
        mov edx, [addr]
        mov eax, old
        lock cmpxchg [edx], ebx
        mov new_value, eax
    }
    return new_value;
}

static inline unsigned long tas(volatile unsigned long* addr)
{
    unsigned long result;
    __asm {
        mov edx, [addr]
        mov eax, [edx]
        mov result, eax
        bts [edx], 0
    }
    return result;
}

static inline unsigned long swap(volatile unsigned long* p, unsigned long val)
{
    __asm {
        mov eax, val
        lock xchg [p], eax;
        mov val, eax
    }
    return val;
}

static inline void nop()
{
    __asm nop;
}

static inline bool casX(volatile unsigned long long* addr,
                        unsigned long expected_high,
                        unsigned long expected_low,
                        unsigned long new_high,
                        unsigned long new_low)
{
    bool success;
    __asm {
        mov eax, expected_low
        mov edx, expected_high
        mov ebx, new_low
        mov ecx, new_high
        mov edi, addr
        lock cmpxchg8b QWORD PTR [edi]
        setz [success]
    }
    return success;
}

static inline bool casX(volatile unsigned long long* addr,
                                      const unsigned long long *oldVal,
                                      const unsigned long long *newVal)
{
    unsigned long old_high = *oldVal >> 32, old_low = *(unsigned long*)oldVal;
    unsigned long new_high = *newVal >> 32, new_low = *(unsigned long*)newVal;

    return casX(addr, old_high, old_low, new_high, new_low);
}

static inline void
mvx(const volatile unsigned long long *src, volatile unsigned long long *dest)
{
    __asm {
        mov esi, src
        mov edi, dest

        fld QWORD PTR [esi]
        fstp QWORD PTR [edi]
    }
}

#elif defined(__i386__) && defined(__GNUC__)

/* "compiler fence" for preventing reordering of loads/stores to
   non-volatiles */
#define CFENCE          asm volatile ("":::"memory")
#define WBR             asm volatile("mfence":::"memory")
#define ISYNC
#define LWSYNC
#define SYNC

// gcc x86 CAS and TAS

static inline unsigned long
cas(volatile unsigned long* ptr, unsigned long old, unsigned long _new)
{
#ifdef __llvm__
    return __sync_val_compare_and_swap(ptr, old, _new);
#else
    unsigned long prev;
    asm volatile("lock;"
                 "cmpxchgl %1, %2;"
                 : "=a"(prev)
                 : "q"(_new), "m"(*ptr), "a"(old)
                 : "memory");
    return prev;
#endif
}

static inline unsigned long tas(volatile unsigned long* ptr)
{
#ifdef __llvm__
    return __sync_lock_test_and_set(ptr, 1);
#else
    unsigned long result;
    asm volatile("lock;"
                 "xchgl %0, %1;"
                 : "=r"(result), "=m"(*ptr)
                 : "0"(1), "m"(*ptr)
                 : "memory");
    return result;
#endif
}

static inline unsigned long
swap(volatile unsigned long* ptr, unsigned long val)
{
#ifdef __llvm__
    return __sync_lock_test_and_set(ptr, val);
#else
    asm volatile("lock;"
                 "xchgl %0, %1"
                 : "=r"(val), "=m"(*ptr)
                 : "0"(val), "m"(*ptr)
                 : "memory");
    return val;
#endif
}

static inline void nop()
{
    asm volatile("nop");
}

// Our code will differ based on the status of -fPIC, since using
// -fPIC on x86 will not let us use EBX in inline asm.
//
// Also note that we want to support -fPIC, since it can't be turned
// off on Darwin.
//
// It's important to list a and d as output registers so that gcc knows that
// they're being messed with in the cmpxchg. Can't be inlined otherwise.
static inline bool
casX(volatile unsigned long long* addr,
     unsigned long expected_high, unsigned long expected_low,
     unsigned long new_high, unsigned long new_low)
{
#if defined(__llvm__) || ((__GNUC__ == 4) && (__GNUC_MINOR__ == 2))
    unsigned long long expected = expected_high;
    expected <<= 32;
    expected |= expected_low;

    unsigned long long replacement = new_high;
    replacement <<= 32;
    replacement |= new_low;
    return __sync_bool_compare_and_swap(addr, expected, replacement);
#else
  #ifndef __PIC__
    char success;
    asm volatile("lock; cmpxchg8b (%6);"
                 "setz %7; "
                 : "=a" (expected_low), "=d" (expected_high)
                 : "0" (expected_low), "1" (expected_high),
                   "c" (new_high), "b" (new_low),
                   "r" (addr), "m" (success)
                 : "cc", "memory");
    return success;
  #else
    // %ebx is used oddly when compiling position independent code. All we do
    // is manually save it. It's not really safe to use push and pop in inline
    // assembly because the compiler doesn't know how to deal with the %esp
    // change, and may try and use it for addressing, so we manually allocate
    // space for it, and mov to it instead.
    //
    // There is a version of this code that takes one less instruction (swap
    // new_low with %%ebx before and after using xchgl), but after testing on
    // a core2 duo I've noticed that this version performs consistently
    // better. I think it's because many times "new_low" is equal to zero, and
    // you can't swap with zero... but I'm not sure.
    bool success;
    unsigned long ebx_buffer;
    asm volatile("movl %%ebx, %2;"          // Save %ebx
                 "movl %7, %%ebx;"          // Move the proper value into %ebx
                 "lock cmpxchg8b (%8);"     // Perform the exchange
                 "setz %3;"                 // Grab the Z flag
                 "movl %2, %%ebx"           // Restore %ebx
                 : "=d" (expected_high), "=a" (expected_low),
                   "=m" (ebx_buffer),    "=q" (success)
                 : "d" (expected_high),  "a" (expected_low),
                   "c" (new_high),       "X" (new_low),
                   "r" (addr)
                 : "cc", "memory");
    return success;
  #endif
#endif
}


static inline bool casX(volatile unsigned long long* addr,
                        const unsigned long long *oldVal,
                        const unsigned long long *newVal)
{
#ifdef __llvm__
    return __sync_bool_compare_and_swap(addr, *oldVal, *newVal);
#else
    unsigned long old_high = *oldVal >> 32, old_low = *oldVal;
    unsigned long new_high = *newVal >> 32, new_low = *newVal;

    return casX(addr, old_high, old_low, new_high, new_low);
#endif
}

// atomic load and store of *src into *dest
static inline void
mvx(const volatile unsigned long long *src, volatile unsigned long long *dest)
{
    // Cast into double, since this will be 64-bits and (hopefully) result in
    // atomic code.

    const volatile double *srcd = (const volatile double*)src;
    volatile double *destd = (volatile double*)dest;

    *destd = *srcd;
}

#elif defined(__ia64__) && defined(__GNUC__)
/* "compiler fence" for preventing reordering of loads/stores to
   non-volatiles */
#define CFENCE          asm volatile ("":::"memory")

#define WBR asm volatile ("mf;;":::"memory")
#define ISYNC
#define LWSYNC
#define SYNC

// unsigned long => 64 bit CAS
static inline unsigned long
cas(volatile unsigned long* ptr, unsigned long old, unsigned long _new)
{
    unsigned long _old;
    asm volatile("mf;;mov ar.ccv=%0;;"::"rO"(old));
    asm volatile("cmpxchg8.acq %0=%1,%2,ar.ccv ;mf;;"   // instruction
                 : "=r"(_old), "=m"(*ptr)               // output
                 : "r"(_new)                            // inputs
                 : "memory");                           // side effects
    return _old;
}

static inline unsigned long tas(volatile unsigned long* ptr)
{
    unsigned long result;
    asm volatile("mf;;xchg8 %0=%1,%2;;mf;;"
                 : "=r"(result), "=m"(*ptr)
                 : "r"(1)
                 : "memory");
    return result;
}

static inline unsigned long
swap(volatile unsigned long* ptr, unsigned long val)
{
    unsigned long result;
    asm volatile("mf;;xchg8 %0=%1,%2;;mf;;"
                 : "=r"(result), "=m"(*ptr)
                 : "r"(val)
                 : "memory");
    return result;
}

static inline void nop()
{
    asm volatile("nop.m 0;;");
}

// NB: casX is not yet implemented

#elif defined(__sparc__) && defined(__GNUC__)

/* "compiler fence" for preventing reordering of loads/stores to
   non-volatiles */
#define CFENCE          asm volatile ("":::"memory")

#define WBR asm volatile ("membar #StoreLoad":::"memory")
#define ISYNC
#define LWSYNC
#define SYNC

static inline unsigned long
cas(volatile unsigned long* ptr, unsigned long old, unsigned long _new)
{
    asm volatile("cas [%2], %3, %0"                     // instruction
                 : "=&r"(_new)                          // output
                 : "0"(_new), "r"(ptr), "r"(old)        // inputs
                 : "memory");                           // side effects
    return _new;
}

static inline unsigned long tas(volatile unsigned long* ptr)
{
    unsigned long result;
    asm volatile("ldstub [%1], %0"
                 : "=r"(result)
                 : "r"(ptr)
                 : "memory");
    return result;
}

static inline unsigned long
swap(volatile unsigned long* ptr, unsigned long val)
{
    asm volatile("swap [%2], %0"
                 : "=&r"(val)
                 : "0"(val), "r"(ptr)
                 : "memory");
    return val;
}

// NB: When Solaris is in 32-bit mode, it does not save the top 32 bits of a
// 64-bit local (l) register on context switch, so always use an "o" register
// for 64-bit ops in 32-bit mode

// we can't mov 64 bits directly from c++ to a register, so we must ldx
// pointers to get the data into registers
static inline bool casX(volatile unsigned long long* ptr,
                        const unsigned long long* expected_value,
                        const unsigned long long* new_value)
{
    bool success = false;

    asm volatile("ldx   [%1], %%o4;"
                 "ldx   [%2], %%o5;"
                 "casx  [%3], %%o4, %%o5;"
                 "cmp   %%o4, %%o5;"
                 "mov   %%g0, %0;"
                 "move  %%xcc, 1, %0"   // predicated move... should do this
                                        // for bool_cas too
                 : "=r"(success)
                 : "r"(expected_value), "r"(new_value), "r"(ptr)
                 : "o4", "o5", "memory");
    return success;
}

// When casX is dealing with packed structs, it is convenient to pass each word
// directly
static inline bool volatile casX(volatile unsigned long long* ptr,
                                 unsigned long expected_high,
                                 unsigned long expected_low,
                                 unsigned long new_high,
                                 unsigned long new_low)
{
    bool success = false;
    asm volatile("sllx %1, 32, %%o4;"
                 "or   %%o4, %2, %%o4;"
                 "sllx %3, 32, %%o5;"
                 "or   %%o5, %4, %%o5;"
                 "casx [%5], %%o4, %%o5;"
                 "cmp  %%o4, %%o5;"
                 "be,pt %%xcc,1f;"
                 "mov  1, %0;"
                 "mov  %%g0, %0;"
                 "1:"
                 : "=r"(success)
                 : "r"(expected_high), "r"(expected_low), "r"(new_high),
                   "r"(new_low), "r"(ptr)
                 : "o4", "o5", "memory");
    return success;
}

static inline void
mvx(const volatile unsigned long long* from, volatile unsigned long long* to)
{
    asm volatile("ldx  [%0], %%o4;"
                 "stx  %%o4, [%1];"
                 :
                 : "r"(from), "r"(to)
                 : "o4", "memory");
}

static inline void nop()
{
    asm volatile("nop");
}

#elif defined(_POWER) && defined(__GNUC__)

#define ISYNC       asm volatile ("isync":::"memory")
#define LWSYNC      asm volatile ("lwsync":::"memory")
#define SYNC        asm volatile ("sync":::"memory")
#define WBR         SYNC

/* "compiler fence" for preventing reordering of loads/stores to
   non-volatiles */
#define CFENCE          asm volatile ("":::"memory")

static inline unsigned long
load_link(volatile unsigned long *addr)
{
    unsigned long value;
    asm volatile("lwarx %0, 0, %1"
                 : "=r" (value)
                 : "r" (addr));
    return value;
}

static inline bool
store_conditional(volatile unsigned long *addr, unsigned long value)
{
    unsigned long success;
    asm volatile("stwcx. %1, 0, %2\n\t"
                 "bne- $+8\n\t"
                 "mr %0, %2\n\t"
                 : "=r" (success)
                 : "r" (value), "r" (addr), "0" (0));
    return success ? true : false;
}


static inline unsigned long
cas(volatile unsigned long* addr, unsigned long old, unsigned long new_value)
{
    asm volatile("lwarx %0, 0, %1\n\t"   // Load and reserve addr into %0
                 "cmpw %0, %2\n\t"       // Is this the expected value?
                 "bne- $+12\n\t"         // If not, bail out (%0 has old value)
                 "stwcx. %3, 0, %1\n\t"  // Try to store new value if we can
                 "bne- $-16\n"           // Our reservation expired; try again
                : "=r" (old)
                : "r" (addr), "r" (old), "r" (new_value),
                  "0" (0)
                : "cc", "memory");
    return old;
}

static inline unsigned long
tas(volatile unsigned long* addr)
{
    unsigned long old;
    asm volatile("lwarx %0, 0, %1\n\t"   // Load and reserve addr into %0
                 "stwcx. %2, 0, %1\n\t"  // Try to store new value if we can
                 "bne- $-8\n"            // Our reservation expired; try again
                : "=r" (old)
                : "r" (addr), "r" (1),
                  "0" (0)
                : "cc", "memory");
    return old;
}

static inline unsigned long
swap(volatile unsigned long* p, unsigned long val)
{
    unsigned long old;
    asm volatile("lwarx %0, 0, %1\n\t"   // Load and reserve addr into %0
                 "stwcx. %2, 0, %1\n\t"  // Try to store new value if we can
                 "bne- $-8\n"            // Our reservation expired; try again
                : "=r" (old)
                : "r" (p), "r" (val),
                  "0" (0)
                : "cc", "memory");
    return old;
}

static inline void
mvx(const volatile unsigned long long* from, volatile unsigned long long* to)
{
  *to = *from;
}

static inline void nop()
{
    asm volatile("nop");
}

#else
#error Your CPU/compiler combination is not supported
#endif

static inline bool
bool_cas(volatile unsigned long* ptr, unsigned long old, unsigned long _new)
{
    return cas(ptr, old, _new) == old;
}

static inline unsigned long fai(volatile unsigned long* ptr)
{
    unsigned long found = *ptr;
    unsigned long expected;
    do {
        expected = found;
    } while ((found = cas(ptr, expected, expected + 1)) != expected);
    return found;
}

static inline unsigned long faa(volatile unsigned long* ptr, int amnt)
{
  unsigned long found = *ptr;
  unsigned long expected;
  do {
    expected = found;
  } while ((found = cas(ptr, expected, expected + amnt)) != expected);
  return found;
}


// exponential backoff
static inline void backoff(int *b)
{
    for (int i = *b; i; i--)
        nop();

    if (*b < 4096)
        *b <<= 1;
}

// issue 64 nops to provide a little busy waiting
static inline void spin64()
{
    for (int i = 0; i < 64; i++)
        nop();
}

// issue 128 nops to provide a little busy waiting
static inline void spin128()
{
    for (int i = 0; i < 128; i++)
        nop();
}

////////////////////////////////////////
// tatas lock

typedef volatile unsigned long tatas_lock_t;

static inline void tatas_acquire_slowpath(tatas_lock_t* L)
{
    int b = 64;

    do
    {
        backoff(&b);
    }
    while (tas(L));
}

static inline void tatas_acquire(tatas_lock_t* L)
{
    if (tas(L))
        tatas_acquire_slowpath(L);
    ISYNC;
}

static inline void tatas_release(tatas_lock_t* L)
{
  LWSYNC;
  *L = 0;
}

////////////////////////////////////////
// ticket lock

extern "C"
{
    typedef struct
    {
        volatile unsigned long next_ticket;
        volatile unsigned long now_serving;
    } ticket_lock_t;
}

static inline void ticket_acquire(ticket_lock_t* L)
{
    unsigned long my_ticket = fai(&L->next_ticket);
    while (L->now_serving != my_ticket);
}

static inline void ticket_release(ticket_lock_t* L)
{
    L->now_serving += 1;
}

////////////////////////////////////////
// MCS lock

extern "C"
{
    typedef volatile struct _mcs_qnode_t
    {
        bool flag;
        volatile struct _mcs_qnode_t* next;
    } mcs_qnode_t;
}

static inline void mcs_acquire(mcs_qnode_t** L, mcs_qnode_t* I)
{
    I->next = 0;
    mcs_qnode_t* pred =
        (mcs_qnode_t*)swap((volatile unsigned long*)L, (unsigned long)I);

    if (pred != 0) {
        I->flag = true;
        pred->next = I;
        while (I->flag) { } // spin
    }
}

static inline void mcs_release(mcs_qnode_t** L, mcs_qnode_t* I)
{
    if (I->next == 0) {
        if (bool_cas((volatile unsigned long*)L, (unsigned long)I, 0))
            return;
        while (I->next == 0) { } // spin
    }
    I->next->flag = false;
}

#endif // ATOMIC_OPS_H__
