#include <pthread.h>
#include "threadPool.h"
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>

#define MAX_QUEUE 32
#define MAX_THREAD 32

struct threadpool *threadpool_init(int thread_num, int queue_size){   
	struct threadpool *pool;
	int i;
	//check to be within limits
	if((thread_num < 1 ) || (queue_size < 1) || (thread_num > MAX_THREAD)|| (queue_size > MAX_QUEUE))
		raise_error("beyond max queue size or thread");


	pool = (struct threadpool *) malloc(sizeof(struct threadpool));
	check_ptr(pool);

	//init pool
	pool->thread_count = thread_num;
	pool->workers = (struct thread **) malloc(sizeof(struct thread*) * thread_num);
	check_ptr(pool->workers);
	pool->queue = (struct job_queue *)malloc(sizeof(struct job_queue));
	check_ptr(pool->queue);

	//init job quue
	pool->queue->size = queue_size;
	pool->queue->isfull = 0;
	pool->queue->head = 0;
	pool->queue->tail= 0;
	pool->queue->count = 0;
	pool->queue->sockets = (int *) malloc(sizeof(int) * queue_size);
	check_ptr(pool->queue->sockets);
	

	//init syncho tools for job queue
	 // value has to be set to zero because there's nothing in the queue
	if(sem_init(&(pool->queue->isempty),0,0) != 0) raise_error("isempty semaphore");
	if(pthread_mutex_init(&(pool->queue->job_mutex), NULL) != 0) raise_error("job_mutex");

	//start creating threads
	for(i=0;i < thread_num;++i){
		pool->workers[i] = (struct thread *) malloc(sizeof(struct thread));
		pool->workers[i]->iswait = 1;
		//loop all threads to checking dict
		if(pthread_create(&(pool->workers[i]->tid),NULL,(void *)threadpool_work, (struct threadpool *) pool) != 0)
			raise_error("thread creating");
	}
	return pool;
}

//adding job to queue
int threadpool_add(struct threadpool *pool, int socket){
	int pos;
	if((pool == NULL) || (socket < 0))
		return EXIT_FAILURE;

	// locking time
	if(pthread_mutex_lock(&(pool->queue->job_mutex)) != 0) 
		return EXIT_FAILURE;

	//check if full, release lock and return to a loop till queue is empty
	if(pool->queue->isfull == 1){
		if(pthread_mutex_unlock(&(pool->queue->job_mutex)) != 0) return EXIT_FAILURE;		
		return pool->queue->isfull;
	}
	// put socket into queue
	pos = pool->queue->tail;
	pool->queue->sockets[pos] = socket; 
	//increment end of queue and increase count in queue
	pool->queue->tail = (pool->queue->tail+1)%(pool->queue->size); 
	(pool->queue->count)++;
	//check if full
	if((pool->queue->count) == (pool->queue->size))
		pool->queue->isfull = 1;
	
	//call semaphore and do V
	if(sem_post(&pool->queue->isempty) != 0) return EXIT_FAILURE;
	//end locking time
	if(pthread_mutex_unlock(&(pool->queue->job_mutex)) != 0) return EXIT_FAILURE;
	
	return EXIT_SUCCESS;
}

//get thread to work
void threadpool_work(struct threadpool *pool){
	pthread_t id = pthread_self();
	int tposition = threadpool_self(pool, id);
	int socket;
	int head;
	int status;

	while(1){
		//wait for queue to fill
		if(sem_wait(&pool->queue->isempty) != 0) raise_error("thread sem");		
		//start of lock 
		if(pthread_mutex_lock(&(pool->queue->job_mutex)) != 0) raise_error("thread mutex");
				//setself not not waiting
				pool->workers[tposition]->iswait = 0; 
				//gets head of buffer
				head = pool->queue->head;
				//increment buffer head
				(pool->queue->head) = (pool->queue->head+1)%(pool->queue->size); 
				//get socket
				socket = pool->queue->sockets[head];
				// empty taken socket position
				pool->queue->sockets[head] = 0;
				(pool->queue->count)--;
				// since server cant just be blocked, this is a way to communicate with server
				if((pool->queue->count) < (pool->queue->size))
					pool->queue->isfull = 0;
		if(pthread_mutex_unlock(&(pool->queue->job_mutex)) != 0) raise_error("thread mutex");
		// end of lock
		printf("Thread: %d - servicing: %d\n",tposition, socket);
		//start word checking with the socket 
		status = wordChecking(socket);
		if(status <0) raise_error("thread wordChecking");
		
		// finsihed its work, go back to waiting
		pool->workers[tposition]->iswait = 1; 
	}
}

//return position of thread n threadpool
int threadpool_self(struct threadpool *pool, pthread_t id){
	int pos;
	for(pos=0;pos <pool->thread_count;pos++)
		if(id == (pool->workers[pos]->tid))
			return pos;
}

//if all iswait for all thread is 0, it will return 1 else 0
int threadpool_isbusy(struct threadpool *pool){
	int size = pool->thread_count;
	int i;
	int yes = 1;
	int no = 0;
	for(i = 0; i < size; i++)
		if((pool->workers[i]->iswait) == 1)
			return no;

	return yes;
}
// check for malloc error
void check_ptr(void* ptr){
	if(!ptr)
		raise_error("Failed memory allocation");
}
//less work to call error
void raise_error(const char* err){
	fprintf(stderr, "%s\n",err);
	exit(EXIT_FAILURE);
}