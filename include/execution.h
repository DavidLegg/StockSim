#ifndef EXECUTION_H
#define EXECUTION_H

#include "types.h"

int MINUTES_PER_STEP;

// Defined in basis points
// $ charged = Gross * Fee points / 10 000
extern int TRANSACTION_FEE;

/**
 * Main execution loops
 */

void runScenario(struct SimState *state);
void runScenarioDemo(struct SimState *state, int waitTimeMs);
void graphScenario(struct SimState *state);

void step(struct SimState *state);

/**
 * Single-Transaction helpers
 */

void addPosition(struct SimState *state, union Symbol *symbol, int quantity);
struct Order *buy(struct SimState *state, union Symbol *symbol, int quantity);
struct Order *sell(struct SimState *state, union Symbol *symbol, int quantity);
struct Order *makeCustomOrder(struct SimState *state, union Symbol *symbol, int quantity, OrderFn *customFn);

#endif // ifndef EXECUTION_H