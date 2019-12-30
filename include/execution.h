#ifndef EXECUTION_H
#define EXECUTION_H

#include "types.h"

#define MINUTES_PER_STEP 30

/**
 * Main execution loops
 */

void runScenario(struct SimState *state);

void step(struct SimState *state);

/**
 * Single-Transaction helpers
 */

void addPosition(struct SimState *state, union Symbol *symbol, unsigned int quantity);
void buy(struct SimState *state, union Symbol *symbol, unsigned int quantity);
void sell(struct SimState *state, union Symbol *symbol, unsigned int quantity);

#endif // ifndef EXECUTION_H