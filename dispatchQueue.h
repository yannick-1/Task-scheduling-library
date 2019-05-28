
#define DISPATCHQUEUE_H
#define error_exit(MESSAGE) perror(MESSAGE), exit(EXIT_FAILURE)

#include <stdio.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdbool.h>

/*  whether dispatching a task synchronously or asynchronously  */
typedef enum 
{ 
    ASYNC, SYNC
} task_dispatch_type_t;

/*  The type of dispatch queue */
typedef enum 
{ 
    CONCURRENT, SERIAL
} queue_type_t;

typedef struct task 
{
    char name[64];              /*  to identify it when debugging  */
    void (*work)(void *);       /*  the function to run  */
    void *params;               /*  parameters to pass to the function  */
    task_dispatch_type_t type;  /*  asynchronous or synchronous  */
} task_t;

typedef struct dispatch_queue_t dispatch_queue_t;               /* the dispatch queue type  */
typedef struct dispatch_queue_thread_t dispatch_queue_thread_t; /* the dispatch queue thread type  */
typedef struct node_ll node_ll;                                 /* linked list */

struct node_ll
{   /*  linked list  */
    task_t task;            /* the task to be run  */
    struct node_ll *next;   /* pointer to next node  */
}; 

struct dispatch_queue_thread_t
{
    dispatch_queue_t *queue;/* the queue this thread is associated with  */
    pthread_t thread;       /* the thread which runs the task  */
    sem_t thread_sem;       /* the semaphore the thread waits on until a task is allocated  */
    task_t *task;           /* the current task for this tread  */
};

struct dispatch_queue_t
{
    queue_type_t queue_type;            /* the type of queue */
    int busyThreads;                    /* number of activly threads working on a task   */
    bool terminate;                      /* condition to determine if queue workers should terminaten     */ 
    sem_t finished_sem;                 /* semaphore  to notify the main thread that  tasks are done  */
    sem_t queue_sem;                    /* semaphore of queue to notify threads when a task is ready    */
    pthread_mutex_t busyThreadMutex;    /* busy mutex associated with  queue  */
    pthread_mutex_t queueMutex;         /* mutex associated with queue  */
    struct node_ll *nodeHead;           /* head of ll  */
};

task_t *task_create(void (*)(void*), void*, char*);
void task_destroy(task_t*);
dispatch_queue_t *dispatch_queue_create(queue_type_t);
void dispatch_queue_destroy(dispatch_queue_t*);
void dispatch_async(dispatch_queue_t*, task_t*);
void dispatch_sync(dispatch_queue_t*, task_t*);
void dispatch_queue_wait(dispatch_queue_t*);
void enqueue(dispatch_queue_t *queue,task_t task);
node_ll *dequeue(dispatch_queue_t *queue);