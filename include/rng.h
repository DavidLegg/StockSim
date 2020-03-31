#ifndef RNG_H
#define RNG_H

#include <pthread.h>

/**
 * Thread-safe Random Number Generator.
 * Bootstraps up from rand_r, using a separate RNG state for each thread.
 */
void tsRandInit(unsigned int seed);
void tsRandAddThread(pthread_t tid);
int tsRand();

#endif // ifndef RNG_H