#ifndef TEST_STRAT_H
#define TEST_STRAT_H

#define JOB_QUEUE_LENGTH 32
#define NUM_JOBS 8

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

struct JobQueue {
    struct SimState *scenarios[JOB_QUEUE_LENGTH];
    struct SimState *results[JOB_QUEUE_LENGTH];
    sem_t jobSlotsAvailable, jobsAvailable, nextJobLock,
          resultSlotsAvailable, resultsAvailable, nextResultSlotLock;
    int nextJobSlot, nextJob, nextResultSlot, nextResult;
};

/**
 * Initializers
 */

void initJobQueue(void);

/**
 * Basic Usage
 */

void addJob(struct SimState *scenario);
struct SimState *getJobResult();

/**
 * High-level Usage
 */
// Runs baseScenario with start times that vary
//   between startTime and endTime by intervals of skipTime.
//   Collects the final worth of each scenario, returning an
//   array, ordered by time.
// Puts one-after-the-end location into end
int *runTimes(struct SimState *baseScenario, time_t startTime, time_t endTime, long skipSeconds, int **array_end);

/**
 * Helpers
 */

void *runJobs(void *);

#endif // ifndef TEST_STRAT_H