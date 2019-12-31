#include <math.h>

#include "strategies.h"
#include "execution.h"

enum OrderStatus basicStrat1(struct SimState *state, struct Order *order) {
    static const int MAX_ITERS = 5;
    static int iters = 0;
    static int havePosition = 0;
    static unsigned int price;

    if (!havePosition) {
        price = state->priceFn(&(order->symbol), state->time);
        buy(state, &(order->symbol), order->quantity);
        havePosition = 1;
    } else if (state->priceFn(&(order->symbol), state->time) > price) {
        sell(state, &(order->symbol), order->quantity);
        havePosition = 0;
        ++iters;
        if (iters >= MAX_ITERS) {
            return None;
        }
    }

    return Active;
}

enum OrderStatus emaStrat(struct SimState *state, struct Order *order) {
    static const unsigned int MAX_ITERS = 100;
    static const unsigned int PERIOD = 120;
    static double discount;
    discount = pow(0.01, 1.0 / PERIOD);
    static unsigned int iters = 0;
    static int havePosition = 0;
    static double avgPrice = 0.0;
    static unsigned int numSamples = 0;
    static unsigned int boughtPrice;
    static unsigned int price;

    price = state->priceFn(&(order->symbol), state->time);
    avgPrice = avgPrice * discount + price * (1 - discount);
    if (numSamples < PERIOD) {
        ++numSamples;
    } else if (!havePosition && price < 0.9999*avgPrice) {
        buy(state, &(order->symbol), order->quantity);
        havePosition = 1;
        boughtPrice = price;
    } else if (havePosition && price > 1.0001*boughtPrice) {
        sell(state, &(order->symbol), order->quantity);
        havePosition = 0;
        ++iters;
        if (iters >= MAX_ITERS) {
            return None;
        }
    } else if (havePosition && price < 0.9999*boughtPrice) {
        sell(state, &(order->symbol), order->quantity);
        havePosition = 0;
        ++iters;
        if (iters >= MAX_ITERS) {
            return None;
        }
    }

    return Active;
}