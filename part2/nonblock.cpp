#include <stdio.h>
#include<assert.h>
#include<stdlib.h>
#include <iostream>
#include <cstdlib>
#include <inttypes.h>
#include <pthread.h>
#include "hrtime.h"

using namespace std;

typedef bool bool_t;
typedef intptr_t lkey_t;
typedef intptr_t  val_t;

typedef struct _pointer_t {
  intptr_t count;
  struct _node_t *ptr;
}__attribute__((packed)) pointer_t;

typedef struct _node_t {

  pointer_t next;
  val_t val;
}__attribute__((packed)) node_t;


typedef struct _queue_t
{
  pointer_t head;
  pointer_t tail;
} queue_t;

queue_t * init_queue (void);
void free_queue (queue_t *);
bool_t enq (queue_t *, const val_t);
bool_t deq (queue_t *, val_t *);

void show_queue(queue_t *);
static volatile int counter = 0;
int iterations;

pthread_mutex_t myMutex;

static node_t *create_node(const val_t);
static void free_node(node_t *);
bool_t enq (queue_t *, const val_t);
bool_t deq (queue_t *, val_t *);


static inline bool_t
#ifdef _X86_64_
cas(volatile pointer_t * addr, pointer_t oldp, const pointer_t newp)
{
    char result;
  __asm__ __volatile__("lock; cmpxchg16b %0; setz %1":"=m"(*addr),
		       "=q"(result)
		       :"m"(*addr), "a"(oldp.count), "d"(oldp.ptr),
		       "b"(newp.count), "c"(newp.ptr)
		       :"memory");
  return (((int)result == 0) ? false:true);
}
#else
cas(volatile pointer_t * addr, const pointer_t oldp, const pointer_t newp)
{
    char result;
  __asm__ __volatile__("lock; cmpxchg8b %0; setz %1":"=m"(*addr),
		       "=q"(result)
		       :"m"(*addr), "a"(oldp.count), "d"(oldp.ptr),
			"b"(newp.count), "c"(newp.ptr)
		       :"memory");
  return (((int)result == 0) ? false:true);
}
#endif


static node_t *create_node(const val_t val)
{
    node_t *node;

    if ((node = (node_t *) calloc(1, sizeof(node_t))) == NULL) {
	return NULL;
    }

    node->val = val;
    node->next.ptr = NULL;
    node->next.count = 0;

    return node;
}

static void free_node(node_t * node)
{

    free(node);

}

queue_t *init_queue(void)
{
    queue_t *q;
    node_t *node;

    if ((q = (queue_t *) calloc(1, sizeof(queue_t))) == NULL) {
	return NULL;
    }

    if ((node = create_node((val_t)NULL)) == NULL) {
      abort();
    }

    q->head.ptr = node;
    q->tail.ptr = node;

    return q;
}

void free_queue(queue_t * q)
{
  free(q);
}

bool_t enq(queue_t * q, const val_t val)
{
    node_t *newNode;
    pointer_t tail, next, tmp;

    if ((newNode = create_node(val)) == NULL)
	return false;

    while (1) {
	tail = q->tail;
	next = tail.ptr->next;

	if (tail.count == q->tail.count && tail.ptr == q->tail.ptr) {
	  if (next.ptr == NULL) {
	    tmp.ptr = newNode;
	    tmp.count = next.count + 1;
	    if (cas(&tail.ptr->next, next, tmp) == true) {
	      break;
	    }
	  }
	  else {
	    tmp.ptr = next.ptr;
	    tmp.count = tail.count + 1;
	    cas(&q->tail, tail, tmp);
	  }
	}
    }
    tmp.ptr = newNode;    tmp.count = tail.count + 1;
    cas(&q->tail, tail, tmp);

    return true;
}

bool_t deq(queue_t * q, val_t * val)
{
  pointer_t head, tail, next, tmp;
 
    while (1) {
	head = q->head;
	tail = q->tail;
	next = head.ptr->next;

	if (head.count == q->head.count && head.ptr == q->head.ptr) {
	  if (head.ptr == tail.ptr) {
	    if (next.ptr == NULL) {
	      return false;
	    }
	    tmp.ptr = next.ptr;
	    tmp.count = head.count + 1;
	    cas(&q->tail, tail, tmp);
	  }
	  else {
	    *val = next.ptr->val;
	    tmp.ptr = next.ptr;
	    tmp.count = head.count + 1;
	    if (cas(&q->head, head, tmp) == true) {
	      break;
	    }
	  }
	}
    }

    free_node (head.ptr);
    return true;
}


void show_queue(queue_t * q)
{
    node_t *curr;

    curr = q->head.ptr;
    while ((curr = curr->next.ptr) != NULL) {
	printf("[%d]", (int) curr->val);
    }
    printf("\n");
}

queue_t *q;

int generateProb()
{
    return rand( ) % 2;
}


void *my_loop(void *threadid)
{
	int tid;
   	tid = (int)threadid;
        int i;
	int prob = generateProb();
 	val_t val;

 	for(i = 1; i <= iterations; i++) {
		cout << iterations;
		cout << "probability: " << prob << endl;
       		 if(prob == 0)
       		 {
		    cout << "Thread : " << tid << ", Enquing" << iterations << endl;
        	    enq(q,iterations);
       		 }
      		  else if( prob == 1) 
      		  {
       		     cout << "Thread : " << tid << ", Dequing" << iterations << endl;
 		    deq(q, &val);
      		  }

	}
      
	pthread_exit(NULL);
}

int main(int argc, char* argv[])
{	
	q = init_queue();
	
	//default values for number of threads & iterations	
	int NUM_THREADS = 4;
	iterations      = 10000;
		
	if(argc <1) 
	{
		std::cerr << "Usage: " << argv[0] << " -t no_of_threads -i counter -lpthread" <<std::endl;
	return 1;
	}
	//Setting the user defined number of threads & iterations
        if(argc >4) 
	{	
   	NUM_THREADS     = atoi(argv[2]);
	iterations      = atoi(argv[4]);
	}
	pthread_t threads[NUM_THREADS];
   	int rc;
   	int i;
   	pthread_attr_t attr;
	void *status;

	// Setting thread joinable
   	pthread_attr_init(&attr);
  	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	pthread_mutex_init(&myMutex,0);
	//timers
   	double start, end;
   	start = getElapsedTime();

	//creating the threads
  	 for( i=1; i <= NUM_THREADS; i++ ){
    	  	//cout << "main() : creating thread, " << i << endl;
    	  	rc = pthread_create(&threads[i], NULL, my_loop, (void *)i );
    	  	if (rc){
     	  	  cout << "Error:unable to create thread," << rc << endl;
       	  	  exit(-1);
   	  	}
 	  }
	end = getElapsedTime();

        // free attribute and wait for the other threads
 	pthread_attr_destroy(&attr);
   	for( i=1; i <= NUM_THREADS; i++ ){
      		rc = pthread_join(threads[i], &status);
      		if (rc){
        	 cout << "Error:unable to join," << rc << endl;
       		 exit(-1);
      		}
     		//cout << "Main: completed thread id :" << i ;
   	}
	cout << "Execution time =" << (end - start)  <<" nsec\n";
        pthread_mutex_destroy(&myMutex);
        pthread_exit(NULL);
}









