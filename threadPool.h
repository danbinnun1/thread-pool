#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include <pthread.h>
#include <signal.h>

#include "osqueue.h"

typedef struct thread_pool {
    pthread_t* threads;
    OSQueue* queue;
    pthread_mutex_t queue_mutex;
    pthread_mutex_t thread_pool_mutex;
    pthread_cond_t cond;
    int destroyed;
} ThreadPool;

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc)(void*),
                 void* param);

#endif
