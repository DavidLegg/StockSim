#ifndef STRATEGIES_H
#define STRATEGIES_H

#include "types.h"

/**
 * Basic Strategies
 *   * Not to be used for actual trading. *
 *   Used for testing the simulator itself.
 */

enum OrderStatus basicStrat1(struct SimState *state, struct Order *order);


/**
 * Modular strategies
 * Meant for actual trading, these are more like
 * components of more complicated strategies, that
 * can be mixed and matched easily.
 */

/**
 * Exit Strategies
 */

// Time-Horizon: Exit Strategy
// Kills all orders, liquidates all positions at cutoff.
// User must initialize order.aux to TimeHorizonArgs struct
struct TimeHorizonArgs {
    time_t offset;
    time_t cutoff;
};
BOUND_SIZE(struct TimeHorizonArgs,ORDER_AUX_BYTES);
enum OrderStatus timeHorizon(struct SimState *state, struct Order *order);

/**
 * Buy/Sell Strategies
 */

// Mean-Reversion: Buy/Sell strategy
// Buys when price < buyFactor * EMA
// Sell when price > sellFactor * EMA,
//   or when price < stopFactor * boughtPrice
// User should init order.aux to meanReversionArgs struct
struct MeanReversionArgs {
    double emaDiscount; // new ema = (old ema) * discount + price * (1 - discount)
    double ema;         // initialize to 0, or price estimate
    double buyFactor;   // set close to, less than, 1
    double sellFactor;  // set close to, greater than, 1
    double stopFactor;  // set close to, less than, 1
    int initialSamples; // # of samples to take before acting
    int boughtPrice;    // init to 0
    int boughtQuantity; // init to 0
};
BOUND_SIZE(struct MeanReversionArgs,ORDER_AUX_BYTES);
enum OrderStatus meanReversion(struct SimState *state, struct Order *order);

#endif // ifndef STRATEGIES_H