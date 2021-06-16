#include "threadPool.h"

#include <stdlib.h>
#include <stdio.h>

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
    thread_pool->threads_running = 0;
    thread_pool->take_tasks_from_queue = 1;
    thread_pool->num_of_threads = numOfThreads;
    pthread_mutex_init(&thread_pool->queue_mutex, NULL);
    pthread_mutex_init(&thread_pool->thread_pool_mutex, NULL);
    pthread_cond_init(&thread_pool->cond, NULL);
    for (size_t i = 0; i < numOfThreads; i++) {
        pthread_create(&thread_pool->threads[i], NULL, thread_function,
                       thread_pool);
        pthread_detach(&thread_pool->threads[i]);
    }
    return thread_pool;
}

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc)(void*),
                 void* param) {
    if (threadPool->destroyed) return;
    job* j = malloc(sizeof(job));
    j->func = computeFunc;
    j->arg = param;
    pthread_mutex_lock(&threadPool->queue_mutex);
    osEnqueue(threadPool->queue, j);
    threadPool->threads_running++;
    pthread_cond_signal(&threadPool->cond);
    pthread_mutex_unlock(&threadPool->queue_mutex);
}

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks) {
    while (1) {
        pthread_mutex_lock(&threadPool->queue_mutex);
        threadPool->destroyed = 1;
        if (threadPool->threads_running == 0 &&
            (!shouldWaitForTasks || osIsQueueEmpty(threadPool->queue))) {
            pthread_cond_signal(&threadPool->cond);
            pthread_mutex_unlock(&threadPool->queue_mutex);
            break;
        }

        pthread_cond_wait(&threadPool->cond, &threadPool->queue_mutex);
        pthread_mutex_unlock(&threadPool->queue_mutex);
    }
    osDestroyQueue(threadPool->queue);
    free(threadPool->threads);
}

static void* thread_function(void* arg) {
    ThreadPool* threadPool = (ThreadPool*)arg;
    while (1) {
        pthread_mutex_lock(&threadPool->queue_mutex);
        job* j;
        while (osIsQueueEmpty(threadPool->queue)) {
            if (threadPool->destroyed) {
                pthread_mutex_unlock(&threadPool->queue_mutex);
                return NULL;
            }
            pthread_cond_wait(&threadPool->cond, &threadPool->queue_mutex);
        }
        if (!threadPool->take_tasks_from_queue) {
            pthread_mutex_unlock(&threadPool->queue_mutex);
            return NULL;
        }
        j = (job*)(osDequeue(threadPool->queue));
        pthread_mutex_unlock(&threadPool->queue_mutex);
        j->func(j->arg);
        free(j);
        pthread_mutex_lock(&threadPool->queue_mutex);
        threadPool->threads_running--;
        pthread_cond_signal(&threadPool->cond);
        pthread_mutex_unlock(&threadPool->queue_mutex);
        if (!threadPool->take_tasks_from_queue) {
            return NULL;
        }
    }
}