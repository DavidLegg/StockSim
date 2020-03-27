#include <stdlib.h>
#include <math.h>

#include "strategies.h"
#include "execution.h"

enum OrderStatus basicStrat1(struct SimState *state, struct Order *order) {
    static const int MAX_ITERS = 5;
    static int iters = 0;
    static int havePosition = 0;
    static int price;

    if (!havePosition) {
        price = state->priceFn(&(order->symbol), state->time, state->priceCache);
        buy(state, &(order->symbol), order->quantity);
        havePosition = 1;
    } else if (state->priceFn(&(order->symbol), state->time, state->priceCache) > price) {
        sell(state, &(order->symbol), order->quantity);
        havePosition = 0;
        ++iters;
        if (iters >= MAX_ITERS) {
            return None;
        }
    }

    return Active;
}

enum OrderStatus timeHorizon(struct SimState *state, struct Order *order) {
    struct TimeHorizonArgs *aux = (struct TimeHorizonArgs*)order->aux;
    if (!aux->cutoff) {
        aux->cutoff = state->time + aux->offset;
    }
    if (state->time >= aux->cutoff) {
        // Remove all current orders
        for (int i = 0; i < state->maxActiveOrder; ++i) {
            state->orders[i].status = None;
        }
        // Liquidate all current positions
        for (int i = 0; i < state->maxActivePosition; ++i) {
            if (state->positions[i].quantity > 0) {
                sell(state, &(state->positions[i].symbol), state->positions[i].quantity);
            }
        }
        return None;
    } else {
        // Otherwise, don't do anything at all.
        return Active;
    }
}

enum OrderStatus meanReversion(struct SimState *state, struct Order *order) {
    struct MeanReversionArgs *aux = (struct MeanReversionArgs*)order->aux;
    int price = state->priceFn(&(order->symbol), state->time, state->priceCache);

    aux->ema = aux->ema * aux->emaDiscount + price * (1 - aux->emaDiscount);
    if (aux->initialSamples) {
        --aux->initialSamples;
    } else if (!aux->boughtQuantity && price < aux->buyFactor * aux->ema && price < state->cash) {
        aux->boughtPrice    = price;
        // Mathematically, price + price * FEE / 10000 is exactly
        // the price that we would pay.
        // Here, we add 1 to cover (conservatively) truncation error.
        aux->boughtQuantity = state->cash / (price + 1 + price * TRANSACTION_FEE / 10000);
        buy(state, &(order->symbol), aux->boughtQuantity);
    } else if (aux->boughtQuantity && (price > aux->sellFactor * aux->ema || price < aux->stopFactor * aux->boughtPrice)) {
        sell(state, &(order->symbol), aux->boughtQuantity);
        aux->boughtQuantity = 0;
    }

    return Active;
}

enum OrderStatus portfolioRebalance(struct SimState *state, struct Order *order) {
    int currentPrices[REBALANCING_MAX_SYMBOLS];
    int values[REBALANCING_MAX_SYMBOLS];
    int buyingPower = state->cash;

    struct PortfolioRebalanceArgs *args = (struct PortfolioRebalanceArgs *) order->aux;

    // Get current prices, current values for each asset class,
    //   and tally total available value
    for (int i = 0; i < args->symbolsUsed; ++i) {
        currentPrices[i] = state->priceFn(args->assets + i, state->time, state->priceCache);
        values[i] = 0;
        for (int j = 0; j < state->maxActivePosition; ++j) {
            if (state->positions[i].symbol.id == args->assets[i].id) {
                values[i] = state->positions[i].quantity * currentPrices[i];
                break;
            }
        }
        buyingPower += values[i];
    }

    // Calculate desired values for each asset class
    //   and place appropriate orders to achieve them
    if (buyingPower > args->maxAssetValue) {
        buyingPower = args->maxAssetValue;
    }
    buyingPower *= REBALANCING_BUFFER_FACTOR;
    int orderQuantity = 0;
    for (int i = 0; i < args->symbolsUsed; ++i) {
        // Calculate amount to buy/sell as difference between
        //   desired and current values, divided by price per unit of asset
        orderQuantity = (int)round( ((buyingPower * args->weights[i]) - (double)values[i]) / currentPrices[i] );
        if (orderQuantity > 0) {
            buy(state, args->assets + i, orderQuantity);
        } else if (orderQuantity < 0) {
            sell(state, args->assets + i, -orderQuantity);
        } // else orderQuantity == 0, no action needed
    }

    return Active;
}
