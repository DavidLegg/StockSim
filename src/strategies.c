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