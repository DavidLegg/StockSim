#include <stdlib.h>
#include <stdio.h>

#include "batch_execution.h"
#include "execution.h"
#include "load_prices.h"

static struct JobQueue JOB_QUEUE;

void initJobQueue(void) {
    for (int i = 0; i < JOB_QUEUE_LENGTH; ++i) {
        JOB_QUEUE.scenarios[i] = NULL;
        JOB_QUEUE.results[i]   = NULL;
    }
    sem_init(&JOB_QUEUE.jobSlotsAvailable, 0, JOB_QUEUE_LENGTH);
    sem_init(&JOB_QUEUE.jobsAvailable, 0, 0);
    sem_init(&JOB_QUEUE.nextJobLock, 0, 1);
    sem_init(&JOB_QUEUE.resultSlotsAvailable, 0, JOB_QUEUE_LENGTH);
    sem_init(&JOB_QUEUE.resultsAvailable, 0, 0);
    sem_init(&JOB_QUEUE.nextResultSlotLock, 0, 1);
    JOB_QUEUE.nextJobSlot = JOB_QUEUE.nextJob = 0;
    JOB_QUEUE.nextResultSlot = JOB_QUEUE.nextResult = 0;

    pthread_t thread;
    for (int i = 0; i < NUM_JOBS; ++i) {
        if (pthread_create(&thread, NULL, runJobs, NULL)) {
            fprintf(stderr, "Error creating thread pool.\n");
            exit(1);
        }
    }
}

void addJob(struct SimState *scenario) {
    sem_wait(&JOB_QUEUE.jobSlotsAvailable);
    JOB_QUEUE.scenarios[JOB_QUEUE.nextJobSlot] = scenario;
    JOB_QUEUE.nextJobSlot = (JOB_QUEUE.nextJobSlot + 1) % JOB_QUEUE_LENGTH;
    sem_post(&JOB_QUEUE.jobsAvailable);
}

struct SimState *getJobResult() {
    sem_wait(&JOB_QUEUE.resultsAvailable);
    struct SimState *result = JOB_QUEUE.results[JOB_QUEUE.nextResult];
    JOB_QUEUE.nextResult = (JOB_QUEUE.nextResult + 1) % JOB_QUEUE_LENGTH;
    sem_post(&JOB_QUEUE.resultSlotsAvailable);
    return result;
}

// Add GCC unused attribute to stop GCC complaining
// if I don't use this variable in the body.
void *runJobs(__attribute__ ((unused)) void *dummy) {
    struct PriceCache *priceCache = malloc(sizeof(struct PriceCache));
    initializePriceCache(priceCache);
    struct SimState *scenario;
    while (1) {
        // Acquire next job
        sem_wait(&JOB_QUEUE.jobsAvailable);
        sem_wait(&JOB_QUEUE.nextJobLock);
        scenario = JOB_QUEUE.scenarios[JOB_QUEUE.nextJob];
        JOB_QUEUE.nextJob = (JOB_QUEUE.nextJob + 1) % JOB_QUEUE_LENGTH;
        sem_post(&JOB_QUEUE.nextJobLock);
        sem_post(&JOB_QUEUE.jobSlotsAvailable);

        // Execute job
        scenario->priceCache = priceCache;
        runScenario(scenario);
        scenario->priceCache = NULL; // don't leak address of priceCache

        // Post result
        sem_wait(&JOB_QUEUE.resultSlotsAvailable);
        sem_wait(&JOB_QUEUE.nextResultSlotLock);
        JOB_QUEUE.results[JOB_QUEUE.nextResultSlot] = scenario;
        JOB_QUEUE.nextResultSlot = (JOB_QUEUE.nextResultSlot + 1) % JOB_QUEUE_LENGTH;
        sem_post(&JOB_QUEUE.nextResultSlotLock);
        sem_post(&JOB_QUEUE.resultsAvailable);
    }
    return NULL;
}

struct runTimesArgs {
    struct SimState *slots[JOB_QUEUE_LENGTH];
    sem_t slotsAvailable;
    long numberScenarios;
};

void *runTimesResults(void *voidArgs) {
    struct runTimesArgs *args = (struct runTimesArgs*)voidArgs;
    long resultsCollected = 0;
    struct SimState *scenario;
    while (resultsCollected < args->numberScenarios) {
        scenario = getJobResult();
        **(int**)(scenario->aux) = scenario->cash;
        args->slots[resultsCollected % JOB_QUEUE_LENGTH] = scenario;
        sem_post(&(args->slotsAvailable));
        ++resultsCollected;
        if (resultsCollected % 100 == 0) {
            printf("%ld/%ld (%.0f%%)\n", resultsCollected, args->numberScenarios, 100.0 * resultsCollected / args->numberScenarios);
        }
    }
    return NULL;
}

int *runTimes(
    struct SimState *baseScenario,
    time_t startTime,
    time_t endTime,
    long skipSeconds,
    int **array_end) {
    struct runTimesArgs args;
    args.numberScenarios = 1 + (endTime - startTime) / skipSeconds;
    int *results = malloc(sizeof(int) * args.numberScenarios);
    *array_end = results + args.numberScenarios;
    struct SimState scenarios[JOB_QUEUE_LENGTH];
    int nextAvailable = 0;
    sem_init(&(args.slotsAvailable), 0, JOB_QUEUE_LENGTH);
    pthread_t resultsThread;

    // Load up the initial slots
    for (int i = 0; i < JOB_QUEUE_LENGTH; ++i) {
        args.slots[i] = scenarios + i;
    }

    pthread_create(&resultsThread, NULL, runTimesResults, &args);

    // Write jobs into slots as they become available
    for (int *p = results; startTime <= endTime; startTime += skipSeconds, ++p) {
        sem_wait(&(args.slotsAvailable));
        copySimState(args.slots[nextAvailable], baseScenario);
        args.slots[nextAvailable]->time = startTime;
        *(int**)args.slots[nextAvailable]->aux = p;
        addJob(args.slots[nextAvailable]);
        nextAvailable = (nextAvailable + 1) % JOB_QUEUE_LENGTH;
    }

    return (pthread_join(resultsThread, NULL) ? NULL : results);
}

int *runTimesMulti(
    struct SimState *baseScenarios,
    int n,
    time_t startTime,
    time_t endTime,
    long skipSeconds,
    int *numberTimes) {

    struct runTimesArgs args;
    *numberTimes = 1 + (endTime - startTime) / skipSeconds;
    args.numberScenarios = n * (*numberTimes);
    int *results = malloc(sizeof(int) * args.numberScenarios);
    struct SimState scenarios[JOB_QUEUE_LENGTH];
    int nextAvailable = 0;
    sem_init(&(args.slotsAvailable), 0, JOB_QUEUE_LENGTH);
    pthread_t resultsThread;

    // Load up the initial slots
    for (int i = 0; i < JOB_QUEUE_LENGTH; ++i) {
        args.slots[i] = scenarios + i;
    }

    pthread_create(&resultsThread, NULL, runTimesResults, &args);

    // Write jobs into slots as they become available
    for (int *p = results; startTime <= endTime; startTime += skipSeconds, ++p) {
        for (int i = 0; i < n; ++i) {
            sem_wait(&(args.slotsAvailable));
            copySimState(args.slots[nextAvailable], baseScenarios + i);
            args.slots[nextAvailable]->time = startTime;
            *(int**)args.slots[nextAvailable]->aux = p + i * (*numberTimes);
            addJob(args.slots[nextAvailable]);
            nextAvailable = (nextAvailable + 1) % JOB_QUEUE_LENGTH;
        }
    }

    return (pthread_join(resultsThread, NULL) ? NULL : results);
}

