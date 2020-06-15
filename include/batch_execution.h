#ifndef TEST_STRAT_H
#define TEST_STRAT_H

#define JOB_QUEUE_LENGTH 32
#define NUM_WORKERS 8
#define NUM_CPUS 8

#include <pthread.h>
#include <semaphore.h>

#include "types.h"

/**
 * Explanation:
 *   Single-producer, many-consumer job pool.
 *   A single main thread uses initJobQueue()
 *   to start a pool of workers, then uses
 *   addJob(...) to add jobs for them to run.
 *   They'll then run those jobs, with the resulting
 *   SimState object (modified in place), left behind.
 *   It is suggested that the main thread spawn
 *   an additional child thread to reap those results
 *   using calls to getJobResult, for maximum efficiency.
 */

/**
 * Structs
 */

struct JQStateQueue {
    struct SimState *slots[JOB_QUEUE_LENGTH];
    int front, back;
};

struct JobQueue {
    struct SimState dataSlots[JOB_QUEUE_LENGTH];
    struct JQStateQueue open;
    struct JQStateQueue ready;
    struct JQStateQueue done;
    sem_t jobSlotsAvailable, jobsAvailable, readyLock,
          resultSlotsAvailable, resultsAvailable, doneLock;
};

/**
 * Initializers
 */

void initJobQueue(void);

/**
 * Basic Usage
 */

void addJob(struct SimState *scenario);
void getJobResult(void (*resultHandler)(struct SimState *));

/**
 * Helpers
 */

void initJQStateQueue(struct JQStateQueue *queue);
void pushJQState(struct JQStateQueue *queue, struct SimState *state);
struct SimState *popJQState(struct JQStateQueue *queue);

void *runJobs(void *);

#endif // ifndef TEST_STRAT_H