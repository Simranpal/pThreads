# pThreads
POSIX threads and various thread locks implementation

Running the programs
To run the programs, open corrosponding folder. It contains the cpp files as well as executable files. It also contains the header files ”atomic_ops.h” and “hrtime.h” which are necessary to run the programs. The name of programs are same as the different parts of question.
For example, to compile the Pthread lock program, use the command:-
 g++ pthreads.cpp –o pthreads –m32 – lpthread

To execute any program, for example pthreads lock program:- 
 ./pthreads

By default if the arguments are not specified, number of threads = 4 and counter = 10,000.
To set the number of threads and counter values:- 
 ./pthreads –t no_of_threads –i counter_value