#include <stdlib.h>
#include <stdio.h>

#include "types.h"
#include "rng.h"

#define MAX_RNG_THREADS 16

static pthread_t RNG_THREAD_IDS[MAX_RNG_THREADS];
static unsigned int RNG_SEEDS[MAX_RNG_THREADS];
static int RNG_IN_USE = 0;

void tsRandInit(unsigned int seed) {
    pthread_t tid = pthread_self();
    RNG_THREAD_IDS[0] = tid;
    RNG_SEEDS[0] = seed;
    RNG_IN_USE = 1;
}

void tsRandAddThread(pthread_t tid) {
    if (RNG_IN_USE >= MAX_RNG_THREADS) {
        fprintf(stderr, "Max number of random number generators exceeded\n");
        exit(1);
    }

    RNG_THREAD_IDS[RNG_IN_USE] = tid;
    pthread_t selfID = pthread_self();
    for (int i = 0; i < RNG_IN_USE; ++i) {
        if (pthread_equal(RNG_THREAD_IDS[i], selfID)) {
            RNG_SEEDS[RNG_IN_USE] = rand_r(RNG_SEEDS + i);
        }
    }
    ++RNG_IN_USE;
}

int tsRand() {
    pthread_t tid = pthread_self();
    for (int i = 0; i < RNG_IN_USE; ++i) {
        if (pthread_equal(RNG_THREAD_IDS[i], tid)) {
            int output = rand_r(RNG_SEEDS + i);
            return output;
        }
    }
    fprintf(stderr, "No random number generator initialized for thread %lu\n", tid);
    exit(1);
}
