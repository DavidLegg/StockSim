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
    long boughtPrice;    // init to 0
    long boughtQuantity; // init to 0
};
BOUND_SIZE(struct MeanReversionArgs,ORDER_AUX_BYTES);
enum OrderStatus meanReversion(struct SimState *state, struct Order *order);

// Portfolio Rebalancing: Buy/Sell Strategy
// Start by setting up a "desired balance" of assets.
// At each time step, buys and sells each asset as necessary to get back to that balance.
// If maxAssetValue is non-negative, will allocate no more than that amount into the market at each step.
#define REBALANCING_MAX_SYMBOLS 128
#define REBALANCING_BUFFER_FACTOR 0.95
struct PortfolioRebalanceArgs {
    union Symbol assets[REBALANCING_MAX_SYMBOLS];
    double weights[REBALANCING_MAX_SYMBOLS];
    long maxAssetValue;
    int symbolsUsed;
};
BOUND_SIZE(struct PortfolioRebalanceArgs,ORDER_AUX_BYTES);
enum OrderStatus portfolioRebalance(struct SimState *state, struct Order *order);

// Random-choice Portfolio Rebalancing: Buy/Sell Strategy
// Starts by selecting some random stocks,
// Then uses the portfolioRebalance strategy on those stocks, with equal weights.
struct RandomPortfolioRebalanceArgs {
    long maxAssetValue;
    int numSymbols;
};
BOUND_SIZE(struct RandomPortfolioRebalanceArgs,ORDER_AUX_BYTES);
enum OrderStatus randomPortfolioRebalance(struct SimState *state, struct Order *order);

// Volatility-chosen Portfolio Rebalancing: Buy/Sell Strategy
// Starts by selecting random stocks with volatility within +/- epsilon of targetVolatility
// Measures volatility over history length of time before now, sampling at sampleFrequency
struct VolatilityPortfolioRebalanceArgs {
    double targetVolatility;
    double epsilon;
    time_t history;
    time_t sampleFrequency;
    long maxAssetValue;
    int numSymbols;
};
BOUND_SIZE(struct VolatilityPortfolioRebalanceArgs,ORDER_AUX_BYTES);
enum OrderStatus volatilityPortfolioRebalance(struct SimState *state, struct Order *order);

// Mean Price-chosen Portfolio Rebalancing: Buy/Sell Strategy
// Starts by selecting random stocks with mean price within +/- epsilon of targetPrice
// Measures mean price over history length of time before now, sampling at sampleFrequency
struct MeanPricePortfolioRebalanceArgs {
    double targetPrice;
    double epsilon;
    time_t history;
    time_t sampleFrequency;
    long maxAssetValue;
    int numSymbols;
};
BOUND_SIZE(struct MeanPricePortfolioRebalanceArgs,ORDER_AUX_BYTES);
enum OrderStatus meanPricePortfolioRebalance(struct SimState *state, struct Order *order);

// Buy-Balanced: Buy Strategy
// Buys approximately totalValue, split among assets according to weights
// Equivalent to first round of PortfolioRebalancing.
struct BuyBalancedArgs {
    union Symbol assets[REBALANCING_MAX_SYMBOLS];
    double weights[REBALANCING_MAX_SYMBOLS];
    long totalValue;
    int symbolsUsed;
};
BOUND_SIZE(struct BuyBalancedArgs,ORDER_AUX_BYTES);
enum OrderStatus buyBalanced(struct SimState *state, struct Order *order);

/**
 * Misc. Helpful Tools
 */

/**
 * Returns an array of n random symbols from the symbols file.
 * If symbolsFile is NULL, then uses DEFAULT_SYMBOLS_FILE
 * If requiredDataStart and/or end are non-zero, only selects symbols
 *   for which there is data starting on or before the required start,
 *   ending on or after the required end
 */
union Symbol *randomSymbols(int n, time_t requiredDataStart, time_t requiredDataEnd);

#endif // ifndef STRATEGIES_H