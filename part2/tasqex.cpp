#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include<assert.h>
#include "atomic_ops.h"
#include "hrtime.h"

using namespace std;

static volatile int counter = 0;
int iterations;
volatile unsigned long flag;
volatile int lock = 0;

const int BACKOFF_BASE          = 20;
const int BACKOFF_FACTOR        = 3;
const int BACKOFF_CAP           = 10000;

typedef struct __node_t {
 	int value;
 	struct __node_t *next;
 } node_t;

 typedef struct __queue_t {
	node_t *head;
 	node_t *tail;

 	pthread_mutex_t headLock;
 	pthread_mutex_t tailLock;
 } queue_t;
queue_t myQ;

void tas_lock() {
	 int delay = BACKOFF_BASE;
	 while (tas(&flag))                                //using tas from atomic_ops.h
	  wait(delay);                                     //wait for certain time
 	  delay =  min(delay*BACKOFF_FACTOR,BACKOFF_CAP);  // setting the dalay value
}

void tas_unlock() {
 flag = 0;
}



 //intialize queue 
 void Queue_Init(queue_t *q) {
 	node_t *tmp = (node_t *)malloc(sizeof(node_t));      // Allocate a free node
 	tmp->next = NULL;                                    // Make it the only node in the linked list
 	q->head = q->tail = tmp;                             // Both Head and Tail point to it

 }
 //adding to queue
 void Queue_Enqueue(queue_t *q, int value) { 
	node_t *tmp = (node_t *)malloc(sizeof(node_t));       // Allocate a new node from the free list
 	assert(tmp != NULL);
 	tmp->value = value;
 	tmp->next = NULL;                                     // Set next pointer of node to NULL
	
	       
	tas_lock();                    			      // Acquire T_lock 
 	q->tail->next = tmp;                                  // Link node at the end of the linked list
	q->tail = tmp;                                        // Swing Tail to node
 	tas_unlock();                  			      //unlock T_lock
 }

 //Dequeue
 int Queue_Dequeue(queue_t *q, int *value) {
 	tas_lock();            // set H_lock in order to access Head
 	node_t *tmp = q->head;                       // Read Head
 	node_t *newHead = tmp->next;                 // Read next pointer
 	if (newHead == NULL) {                       // Is queue empty?
 		tas_unlock();  // Release H_lock before return
 	return -1;                                   // if the queue was empty
 	}
 	*value = newHead->value;                     // Queue not empty.  Read value before release
 	q->head = newHead;                           // Swing Head to next node
 	tas_unlock();          // Release H_lock
 	free(tmp);                                   // Free the tmp node
 	return 0;                                    // Queue was'nt empty, dequeue succeeded
 }

int generateProb()
{
    return rand( ) % 2;
}


void *run_thread(void *threadid)
{
	int tid;
   	tid = (int)threadid;
        int i;
	int prob = generateProb();
 	
 	for(i = 1; i <= iterations; i++) {
		cout << iterations;
		
       		 if(prob == 0)
       		 {
		    cout << "Thread : " << tid << ", Enquing" << iterations << endl;
        	    Queue_Enqueue(&myQ, iterations);
       		 }
      		  else if( prob == 1) 
      		  {
       		     cout << "Thread : " << tid << ", Dequing" << iterations << endl;
 		     Queue_Dequeue(&myQ, &iterations);
      		  }

	}
      
	pthread_exit(NULL);
}



int main(int argc, char* argv[])
{	
	//intialize flag
	flag = 0;
	Queue_Init(&myQ);
	
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
	
	//timers
   	double start, end;
   	start = getElapsedTime();
	//creating the threads
  	 for( i=1; i <= NUM_THREADS; i++ ){
    	  	//cout << "main() : creating thread, " << i << endl;
    	  	rc = pthread_create(&threads[i], NULL, run_thread, (void *)i );
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
        pthread_exit(NULL);
}
