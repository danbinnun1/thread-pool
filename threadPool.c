#include "threadPool.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct job {
    void (*func)(void* arg);
    void* arg;
} job;

static void* thread_function(void* arg);

ThreadPool* tpCreate(int numOfThreads) {
    size_t i;
    ThreadPool* thread_pool = malloc(sizeof(ThreadPool));
    if (thread_pool == NULL) {
        perror("Error in system call");
    }
    thread_pool->threads = malloc(numOfThreads * sizeof(pthread_t));
    if (thread_pool->threads == NULL) {
        perror("Error in system call");
    }
    thread_pool->queue = osCreateQueue();
    thread_pool->destroyed = 0;
    thread_pool->threads_running = 0;
    thread_pool->take_tasks_from_queue = 1;
    thread_pool->num_of_threads = numOfThreads;
    if (pthread_mutex_init(&thread_pool->queue_mutex, NULL)) {
        perror("Error in system call");
    }
    if (pthread_mutex_init(&thread_pool->thread_pool_mutex, NULL)) {
        perror("Error in system call");
    }
    if (pthread_cond_init(&thread_pool->cond, NULL)) {
        perror("Error in system call");
    }
    for (i = 0; i < numOfThreads; i++) {
        if (pthread_create(&thread_pool->threads[i], NULL, thread_function,
                           thread_pool)) {
            perror("Error in system call");
        }
        if (pthread_detach(thread_pool->threads[i])) {
            perror("Error in system call");
        }
    }
    return thread_pool;
}

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc)(void*),
                 void* param) {
    if (threadPool->destroyed) return;
    job* j = malloc(sizeof(job));
    if (j == NULL) {
        perror("Error in system call");
    }
    j->func = computeFunc;
    j->arg = param;
    if (pthread_mutex_lock(&threadPool->queue_mutex)) {
        perror("Error in system call");
    }
    osEnqueue(threadPool->queue, j);
    if (pthread_cond_signal(&threadPool->cond)) {
        perror("Error in system call");
    }
    if (pthread_mutex_unlock(&threadPool->queue_mutex)) {
        perror("Error in system call");
    }
}

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks) {
    while (1) {
        if (pthread_mutex_lock(&threadPool->queue_mutex)) {
            perror("Error in system call");
        }
        threadPool->destroyed = 1;
        threadPool->take_tasks_from_queue = shouldWaitForTasks;
        if (threadPool->threads_running == 0 &&
            (!shouldWaitForTasks || osIsQueueEmpty(threadPool->queue))) {
            if (pthread_cond_signal(&threadPool->cond)) {
                perror("Error in system call");
            }
            if (pthread_mutex_unlock(&threadPool->queue_mutex)) {
                perror("Error in system call");
            }
            break;
        }
        if (pthread_cond_signal(&threadPool->cond)) {
            perror("Error in system call");
        }
        if (pthread_cond_wait(&threadPool->cond, &threadPool->queue_mutex)) {
            perror("Error in system call");
        }
        if (pthread_mutex_unlock(&threadPool->queue_mutex)) {
            perror("Error in system call");
        }
    }
    osDestroyQueue(threadPool->queue);
    free(threadPool->threads);
    free(threadPool);
}

static void* thread_function(void* arg) {
    ThreadPool* threadPool = (ThreadPool*)arg;
    while (1) {
        if (pthread_mutex_lock(&threadPool->queue_mutex)) {
            perror("Error in system call");
        }
        job* j;
        while (osIsQueueEmpty(threadPool->queue)) {
            if (threadPool->destroyed) {
                if (pthread_mutex_unlock(&threadPool->queue_mutex)) {
                    perror("Error in system call");
                }
                return NULL;
            }
            if (pthread_cond_signal(&threadPool->cond)) {
                perror("Error in system call");
            }
            if (pthread_cond_wait(&threadPool->cond,
                                  &threadPool->queue_mutex)) {
                perror("Error in system call");
            }
        }
        if (!threadPool->take_tasks_from_queue) {
            if (pthread_mutex_unlock(&threadPool->queue_mutex)) {
                perror("Error in system call");
            }
            return NULL;
        }
        j = (job*)(osDequeue(threadPool->queue));
        threadPool->threads_running++;
        if (pthread_mutex_unlock(&threadPool->queue_mutex)) {
            perror("Error in system call");
        }
        j->func(j->arg);
        free(j);
        if (pthread_mutex_lock(&threadPool->queue_mutex)) {
            perror("Error in system call");
        }
        threadPool->threads_running--;
        if (pthread_cond_signal(&threadPool->cond)) {
            perror("Error in system call");
        }
        if (pthread_mutex_unlock(&threadPool->queue_mutex)) {
            perror("Error in system call");
        }
        if (!threadPool->take_tasks_from_queue) {
            return NULL;
        }
    }
}