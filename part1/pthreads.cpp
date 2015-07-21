#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include "hrtime.h"

using namespace std;


static volatile int counter = 0;
int iterations;
pthread_mutex_t myMutex;

void *mutex_testing(void *threadid)
{
	long tid;
   	tid = (long)threadid;
        int i;
        for(i = 1; i <= iterations; i++) {
                pthread_mutex_lock(&myMutex);
                counter++; 
		cout << "Thread : " << tid << ", Counter : "<< counter << endl;
                pthread_mutex_unlock(&myMutex);
        }
	pthread_exit(NULL);
}

int main(int argc, char* argv[])
{	
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
	
	pthread_mutex_init(&myMutex,0);

	//creating the threads
  	 for( i=1; i <= NUM_THREADS; i++ ){
    	  	//cout << "main() : creating thread, " << i << endl;
    	  	rc = pthread_create(&threads[i], NULL, mutex_testing, (void *)i );
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
