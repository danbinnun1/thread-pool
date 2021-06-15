#include "threadPool.h"

#include <stdlib.h>

typedef struct job {
    void (*func)(void* arg);
    void* arg;
} job;

static void* thread_function(void* arg);

ThreadPool* tpCreate(int numOfThreads) {
    ThreadPool* thread_pool = malloc(sizeof(ThreadPool));
    thread_pool->threads = malloc(numOfThreads * sizeof(pthread_t));
    thread_pool->queue = osCreateQueue();
    thread_pool->destroyed = 0;
    pthread_mutex_init(&thread_pool->queue_mutex, NULL);
    pthread_mutex_init(&thread_pool->thread_pool_mutex, NULL);
    pthread_cond_init(&thread_pool->cond, NULL);
    for (size_t i = 0; i < numOfThreads; i++) {
        pthread_create(&thread_pool->threads[i], NULL, thread_function,
                       thread_pool);
        pthread_detach(&thread_pool->threads[i]);
    }
}

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc)(void*),
                 void* param) {
    job* j = malloc(sizeof(job));
    j->func = computeFunc;
    j->arg = param;
    pthread_mutex_lock(&threadPool->queue_mutex);
    osEnqueue(&threadPool->queue, j);
    pthread_cond_signal(&threadPool->cond);
    pthread_mutex_unlock(&threadPool->queue_mutex);
}

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks) {}

static void* thread_function(void* arg) {
    ThreadPool* threadPool = (ThreadPool*)arg;
    while (1) {
        pthread_mutex_lock(&threadPool->queue_mutex);
        job* j;
        if (osIsQueueEmpty(threadPool->queue)) {
            pthread_cond_wait(&threadPool->cond, &threadPool->queue_mutex);
        }
        j = (job*)(osDequeue(threadPool->queue));
        pthread_mutex_unlock(&threadPool->queue_mutex);
        j->func(j->arg);
        free(j);
    }
}