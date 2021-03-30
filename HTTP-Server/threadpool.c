#include "pthread.h"
#include "threadpool.h"
#include "stdlib.h"
#include <unistd.h>
#include "stdio.h"
/**
 * create_threadpool creates a fixed-sized thread
 * pool.  If the function succeeds, it returns a (non-NULL)
 * "threadpool", else it returns NULL.
 * this function should:
 * 1. input sanity check 
 * 2. initialize the threadpool structure
 * 3. initialized mutex and conditional variables
 * 4. create the threads, the thread init function is do_work and its argument is the initialized threadpool. 
 */
threadpool *create_threadpool(int num_threads_in_pool)
{
    int create_status = 0;
    int cond_init_status = 0;
    if (num_threads_in_pool > MAXT_IN_POOL || num_threads_in_pool <= 0)
    {
        printf("Usage: threadpool <pool-size> <max-number-of-jobs>\n");
        return NULL;
    }
    threadpool *new_threadpool = (threadpool *)calloc(1, sizeof(threadpool));
    if (new_threadpool == NULL)
    {
        perror("Error while allocating memory for 'new_threadpool' \n ");
        return NULL;
    }
    new_threadpool->threads = (pthread_t *)calloc(num_threads_in_pool, sizeof(pthread_t));
    if (new_threadpool->threads == NULL)
    {
        perror("Error while allocating memory for 'threads' \n ");
        free(new_threadpool);
        return NULL;
    }
    cond_init_status = pthread_cond_init(&new_threadpool->q_empty, NULL);
    if (cond_init_status == -1)
    {
        perror("Error while initializing q_empty condition.\n");
        free(new_threadpool->threads);
        free(new_threadpool);
        return NULL;
    }
    cond_init_status = pthread_cond_init(&new_threadpool->q_not_empty, NULL);
    if (cond_init_status == -1)
    {
        perror("Error while initializing q_not_empty condition.\n");
        free(new_threadpool->threads);
        free(new_threadpool);
        return NULL;
    }
    new_threadpool->qsize = 0;
    new_threadpool->num_threads = num_threads_in_pool;
    new_threadpool->dont_accept = 0;
    new_threadpool->shutdown = 0;
    new_threadpool->qhead = NULL;
    new_threadpool->qtail = NULL;
    new_threadpool->qlock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    for (int i = 0; i < num_threads_in_pool; i++)
    {
        create_status = pthread_create(&new_threadpool->threads[i], NULL, do_work, new_threadpool);
        if (create_status != 0)
        {

            free(new_threadpool->threads);
            free(new_threadpool);
            return NULL;
        }
    }
    return new_threadpool;
}
void dispatch(threadpool *from_me, dispatch_fn dispatch_to_here, void *arg)
{
    if(from_me == NULL || dispatch_to_here == NULL || arg == NULL)
    {
        fprintf(stderr,"One of arguments given to dispatch is NULL. \n");
        return;
    }
    int mutex_status = 0;
    work_t *new = (work_t *)calloc(1, sizeof(work_t));
    if (new == NULL)
    {
        perror("Error while allocating memory for 'threads' \n ");
        return;
    }
    new->routine = NULL;
    new->arg = NULL;
    new->arg = arg;
    new->routine = dispatch_to_here;
    new->next = NULL;
    mutex_status = pthread_mutex_lock(&from_me->qlock);
    if (mutex_status != 0)
    {
        perror("Error while locking the mutex.\n ");
        free(new);
        return;
    }
    if (from_me->dont_accept == 0)
    {
        if (from_me->qsize == 0)
        {
            from_me->qhead = new;
            from_me->qtail = new;
        }
        else if (from_me->qsize > 0)
        {
            from_me->qtail->next = new;
            from_me->qtail = new;
        }
        from_me->qsize++;
        mutex_status = pthread_mutex_unlock(&from_me->qlock);
        if (mutex_status != 0)
        {
            perror("Error while un-locking the mutex.\n ");
            free(new);
            return;
        }
        pthread_cond_signal(&from_me->q_not_empty);
        
    }
    else
    {
        fprintf(stderr,"Cannot take anymore tasks, shutdown process begun sorry.\n");
        return;
    }
}
void *do_work(void *p)
{
    int mutex_status = 0;
    threadpool *pool = (threadpool *)p;
    while (1)
    {
        mutex_status = pthread_mutex_lock(&pool->qlock);
        if (mutex_status != 0)
        {
            perror("Error while locking the mutex.\n ");
            return NULL;
        }
        if (pool->shutdown == 1)
        {
            mutex_status = pthread_mutex_unlock(&pool->qlock);
            if (mutex_status != 0)
            {
                perror("Error while locking the mutex.\n ");
            }
            return NULL;
        }
        work_t *to_do = NULL;
        if (pool->qsize == 0)
        {
            mutex_status = pthread_mutex_unlock(&pool->qlock);
            if (mutex_status != 0)
            {
                perror("Error while locking the mutex.\n ");
            }
            pthread_cond_wait(&pool->q_not_empty, &pool->qlock);
        }
        if (pool->shutdown == 1)
        {
            mutex_status = pthread_mutex_unlock(&pool->qlock);
            if (mutex_status != 0)
            {
                perror("Error while locking the mutex.\n ");
            }
            return NULL;
        }
        if (pool->qsize > 0)
        {
            to_do = pool->qhead;
            pool->qhead = to_do->next;
            if (to_do->next == NULL)
            {
                pool->qtail = NULL;
            }
            pool->qsize--;
            if (pool->qsize == 0 && pool->dont_accept == 1)
            {
                pthread_cond_signal(&pool->q_empty);
            }
        }
        mutex_status = pthread_mutex_unlock(&pool->qlock);
        if (mutex_status != 0)
        {
            perror("Error while locking the mutex.\n ");
            return NULL;
        }
        if(to_do!=NULL)
            {
                to_do->routine(to_do->arg);
                free(to_do);
            }     
    }
}
void destroy_threadpool(threadpool *destroyme)
{
    int mutex_status = 0;
    mutex_status = pthread_mutex_lock(&destroyme->qlock);
    if (mutex_status != 0)
    {
        perror("Error while locking the mutex.\n ");
        return;
    }
    destroyme->dont_accept = 1;
    if (destroyme->qsize > 0) pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock);
    destroyme->shutdown = 1;
    mutex_status = pthread_mutex_unlock(&destroyme->qlock);
    
    if (mutex_status != 0)
    {
        perror("Error while locking the mutex.\n ");
        return;
    }
    pthread_cond_broadcast(&destroyme->q_not_empty);
    pthread_cond_broadcast(&destroyme->q_empty);
    for (int i = 0; i < destroyme->num_threads; i++)
    {
        pthread_join(destroyme->threads[i], NULL);
    }
    free(destroyme->threads);
    free(destroyme);
}
