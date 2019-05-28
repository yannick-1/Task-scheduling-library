
#include "dispatchQueue.h"
#include <stdio.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdbool.h>


/*  Linked list functions  */

/*  enqueue function  */
void enqueue(dispatch_queue_t *queue, task_t task)
{
    node_ll *newNode = (node_ll*) malloc(sizeof(node_ll));    
    node_ll *current = (*queue).nodeHead;
    (*newNode).next = NULL;
    (*newNode).task = task;
    while ((*current).next != NULL) 
    {
        current = (*current).next;
    }
    (*current).next = newNode;
}


/*  dequeue function  */
node_ll* dequeue(dispatch_queue_t *queue)
{
    node_ll *temp = (*queue).nodeHead;
    (*queue).nodeHead = (*temp).next;
    return temp;
}


/*  if no head, push head onto queue  */
void pushHead(dispatch_queue_t *queue, task_t *task)
{
    node_ll *head = (node_ll*) malloc(sizeof(node_ll));
    (*head).next = NULL;
    (*head).task = *task;
    (*queue).nodeHead = head;
}


/*  Destroys the dispatch queue queue. 
    All allocated memory and resources such as semaphores are released and returned.  */
void dispatch_queue_destroy(dispatch_queue_t *queue) {
    pthread_mutex_destroy(&(*queue).queueMutex);
    pthread_mutex_destroy(&(*queue).busyThreadMutex);
    sem_destroy(&(*queue).finished_sem);
    sem_destroy(&(*queue).queue_sem);
    free(queue);
}


/*  Destroys the task. Call this function as soon as a task has completed.
    All memory allocated to the task should be returned.  */
void task_destroy(task_t *task)
{
//  this breaks stuff ... 
//    free((*task).params);
//    free((*task).work);  
    free(task);
}


/*  function to run threads concurrently  */
void *run_task(void *param) {   
    /*  case to dispatch queue to prevent warnings  */
    dispatch_queue_t *queue = (dispatch_queue_t*) param;
    /*  loop forever...  */
    while (true) 
    {   /*  loop until more tasks are pushed to the queue  */
        sem_wait(&(*queue).queue_sem);
        /*  check for termination condition  */
        if ((*queue).terminate)
        {
            break;
        }
        /*  update the active threads count and aquire mutex lock  */
        pthread_mutex_lock(&(*queue).queueMutex);
        pthread_mutex_lock(&(*queue).busyThreadMutex);
        (*queue).busyThreads += 1;
        pthread_mutex_unlock(&(*queue).busyThreadMutex);
        /*  dequeue a task from queue  */
        node_ll *target = dequeue(queue);
        if ((*queue).queue_type == SERIAL)
        {
                /*  release the lock after performing the task  */
                (*target).task.work((*target).task.params);
                /*  free memory  */
                task_destroy(&(*target).task);
                /*  reduce active threads on the queue when thread is done  */
                pthread_mutex_lock(&(*queue).busyThreadMutex);
                (*queue).busyThreads -= 1;
                /*  "parents are allowed to kill their children" - robert sheehan  */
                pthread_mutex_unlock(&(*queue).busyThreadMutex);
                pthread_mutex_unlock(&(*queue).queueMutex);            

        }
        else    /*  if CONCURRENT  */
        {
                /*  release the lock before performing the task  */
                pthread_mutex_unlock(&(*queue).queueMutex);
                (*target).task.work((*target).task.params);
                /*  free memory  */
                task_destroy(&(*target).task);
                /*  reduce active threads on the queue as this thread is done  */
                pthread_mutex_lock(&(*queue).busyThreadMutex);
                (*queue).busyThreads -= 1;
                pthread_mutex_unlock(&(*queue).busyThreadMutex);
        }
        if (((*queue).nodeHead == NULL && (*queue).busyThreads == 0))
        {
            sem_post(&(*queue).finished_sem);
        }
    }
    /*  clean up...  */
    pthread_exit(0);
    }


/*  start up threads & run the tasks on the sem queue  */
void createThreads(dispatch_queue_t *queue, queue_type_t queue_type)
{
    int num_cores = get_nprocs_conf();
    for (int i = 0;i<num_cores;i++)
    {
        pthread_t *thread = (pthread_t*) malloc(sizeof(pthread_t));
        pthread_create(thread, NULL, run_task, queue);
    }
}

/*  Creates a dispatch queue, sets up any associated threads and a linked list 
    to be used by the added tasks. The queueType is either CONCURRENT or SERIAL.
    Returns: A pointer to the created dispatch queue.  */
dispatch_queue_t *dispatch_queue_create(queue_type_t queue_type)
{
    pthread_mutex_t queueMutex;
    pthread_mutex_t busyThreadMutex;
    dispatch_queue_t *queue = (dispatch_queue_t *) malloc(sizeof(dispatch_queue_t));
    /*  setup of dispatch queue  */   
    (*queue).busyThreadMutex = busyThreadMutex;     
    (*queue).queueMutex = queueMutex;
    (*queue).queue_type = queue_type;
    (*queue).terminate = false;
    /*  setup of dispatch queue  threads */
    sem_t finished_sem;
    sem_t queue_sem;
    (*queue).queue_sem = queue_sem;
    (*queue).finished_sem = finished_sem;
    /*  setup of sems and locks  */
    sem_init(&(*queue).queue_sem, 0, 0);
    sem_init(&(*queue).finished_sem, 0, 0);
    pthread_mutex_init(&(*queue).queueMutex, NULL);
    pthread_mutex_init(&(*queue).busyThreadMutex, NULL);
    /*  create threads  */
    createThreads(queue, queue_type);
    return queue;
}



/*  Creates a task. work is the function to be called when the task is executed, 
    param is a pointer to either a structure which holds all of the parameters 
    for the work function to execute with or a single parameter which the work function uses. 
    If it is a single parameter it must either be a pointer or something,
    which can be cast to or from a pointer. 
    The name is a string of up to 63 characters. This is useful for debugging purposes.
    Returns: A pointer to the created task.  */
task_t *task_create(void (*work)(void*), void *param, char *name)
{
    task_t *task = (task_t*) malloc(sizeof(task_t));
    (*task).work = (void (*)(void *))work;
    *(*task).name = *name;    
    (*task).params = param;
    /*  redundant??  */
    (*task).type = ASYNC;
    return task;
}


/*  Sends the task to the queue (which could be either CONCURRENT or SERIAL).
    This function does not return to the calling thread until the task has been completed.  */
void dispatch_sync(dispatch_queue_t *queue, task_t *task)
{
    (*task).work((*task).params);
    task_destroy(task);
}


/*  Sends the task to the queue (which could be either CONCURRENT or SERIAL). 
    This function returns immediately, the task will be dispatched sometime in the future.  */
void dispatch_async(dispatch_queue_t *queue, task_t *task) 
{
    /*  locks queue  */
    pthread_mutex_lock(&(*queue).queueMutex);
    /*  check if head exists (queue is not empty)  */
    if ((*queue).nodeHead == NULL)
    {
        pushHead(queue, task);
    } 
    else 
    {
        enqueue(queue, *task);
    }
    /*  unlocks lock  */
    pthread_mutex_unlock(&(*queue).queueMutex);
    /*  notify sem that tasks  areavalible  */
    sem_post(&(*queue).queue_sem);
}


/*  Waits (blocks) until all tasks on the queue have completed.
    If new tasks are added to the queue after this is called they are ignored.  */
void dispatch_queue_wait(dispatch_queue_t *queue)
{
    /*  blocks until no more active theads & tasks in queue  */
    sem_wait(&(*queue).finished_sem);
    /*  add all waiting tasks waiting on queue semaphore  */
    (*queue).terminate = true;
    int num_cores = get_nprocs_conf();
    for (int i = 0; i < num_cores; i++)
    {
        sem_post(&(*queue).queue_sem);
    }
}
