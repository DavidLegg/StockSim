#ifndef STRATEGIES_H
#define STRATEGIES_H

#include "types.h"

/**
 * Basic Strategies
 *   * Not to be used for actual trading. *
 *   Used for testing the simulator itself.
 */

enum OrderStatus basicStrat1(struct SimState *state, struct Order *order);

#endif // ifndef STRATEGIES_H