#include <semaphore.h>
struct thread{
    pthread_t tid;
    int iswait; // if the thread is running
}thread;

struct threadpool{
    struct thread **workers;
    int thread_count;
    struct job_queue *queue;
}threadpool;


struct dictionary{
    char *name; // dictionary name;
    char **word_list;
    int entries;
}dictionary;

struct job_queue{
    pthread_mutex_t job_mutex; // accessing to queue mutex
    int *sockets;
    int size;
    int isfull; // for producer
    sem_t isempty; //counting sem
    int head; // head of the buffer to consume
    int tail; // end of the buffer to produce
    int count; // how many queue has been fill
}job_queue;

typedef enum {
    EXIT_FAILURE = -1,
    EXIT_SUCCESS =  0
} exit_value;

void check_ptr(void* ptr);//
void raise_error(const char* err);//
struct threadpool *threadpool_init(int thread_num, int queue_size);//
void threadpool_work(struct threadpool *pool);
int threadpool_add(struct threadpool *pool, int socket);//
int threadpool_self(struct threadpool *pool, pthread_t id);  
int wordChecking(int socket);//
int threadpool_isbusy(struct threadpool *pool);
