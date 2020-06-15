#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>

#include "batch_execution.h"
#include "rng.h"
#include "execution.h"
#include "load_prices.h"

static struct JobQueue JOB_QUEUE;
static int JOB_QUEUE_INITIALIZED = 0;

void initJobQueue(void) {
    if (JOB_QUEUE_INITIALIZED) return; // nothing to do!

    historicalPriceInit();

    initJQStateQueue(&JOB_QUEUE.open);
    initJQStateQueue(&JOB_QUEUE.ready);
    initJQStateQueue(&JOB_QUEUE.done);
    for (int i = 0; i < JOB_QUEUE_LENGTH; ++i) {
        pushJQState(&JOB_QUEUE.open, JOB_QUEUE.dataSlots + i);
    }
    sem_init(&JOB_QUEUE.jobSlotsAvailable, 0, JOB_QUEUE_LENGTH);
    sem_init(&JOB_QUEUE.jobsAvailable, 0, 0);
    sem_init(&JOB_QUEUE.readyLock, 0, 1);
    sem_init(&JOB_QUEUE.resultSlotsAvailable, 0, JOB_QUEUE_LENGTH);
    sem_init(&JOB_QUEUE.resultsAvailable, 0, 0);
    sem_init(&JOB_QUEUE.doneLock, 0, 1);

    pthread_t thread;
    cpu_set_t cpu_set;
    for (int i = 0; i < NUM_WORKERS; ++i) {
        if (pthread_create(&thread, NULL, runJobs, NULL)) {
            fprintf(stderr, "Error creating thread pool.\n");
            exit(1);
        }
        CPU_ZERO(&cpu_set);
        CPU_SET(i % NUM_CPUS, &cpu_set);
        pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpu_set);
        tsRandAddThread(thread);
        historicalPriceAddThread(thread);
    }
    JOB_QUEUE_INITIALIZED = 1;
}

void addJob(struct SimState *scenario) {
    // When room is available, copy into a data slot and push a reference to the ready queue
    sem_wait(&JOB_QUEUE.jobSlotsAvailable);
    struct SimState *dataSlot = popJQState(&JOB_QUEUE.open);
    copySimState(dataSlot, scenario);
    pushJQState(&JOB_QUEUE.ready, dataSlot);
    sem_post(&JOB_QUEUE.jobsAvailable);
}

void getJobResult(void (*resultHandler)(struct SimState *)) {
    sem_wait(&JOB_QUEUE.resultsAvailable);
    struct SimState *dataSlot = popJQState(&JOB_QUEUE.done);
    sem_post(&JOB_QUEUE.resultSlotsAvailable);
    resultHandler(dataSlot);
    pushJQState(&JOB_QUEUE.open, dataSlot);
    sem_post(&JOB_QUEUE.jobSlotsAvailable);
}

void initJQStateQueue(struct JQStateQueue *queue) {
    for (int i = 0; i < JOB_QUEUE_LENGTH; ++i) queue->slots[i] = NULL;
    queue->front = 0;
    queue->back  = 0;
}

void pushJQState(struct JQStateQueue *queue, struct SimState *state) {
    queue->slots[queue->front] = state;
    queue->front = (queue->front + 1) % JOB_QUEUE_LENGTH;
}

struct SimState *popJQState(struct JQStateQueue *queue) {
    struct SimState *state = queue->slots[queue->back];
    queue->slots[queue->back] = NULL; // DEBUG
    queue->back = (queue->back + 1) % JOB_QUEUE_LENGTH;
    return state;
}

// Add GCC unused attribute to stop GCC complaining
// if I don't use this variable in the body.
void *runJobs(__attribute__ ((unused)) void *dummy) {
    struct SimState *scenario;
    while (1) {
        // Acquire next job
        sem_wait(&JOB_QUEUE.jobsAvailable);
        sem_wait(&JOB_QUEUE.readyLock);
        scenario = popJQState(&JOB_QUEUE.ready);
        sem_post(&JOB_QUEUE.readyLock);

        // Execute job
        runScenario(scenario);

        // Post result
        sem_wait(&JOB_QUEUE.resultSlotsAvailable);
        sem_wait(&JOB_QUEUE.doneLock);
        pushJQState(&JOB_QUEUE.done, scenario);
        sem_post(&JOB_QUEUE.doneLock);
        sem_post(&JOB_QUEUE.resultsAvailable);
    }
    return NULL;
}
