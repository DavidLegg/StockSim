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

struct emaStratAux {
    time_t timeHorizon;
    int havePosition;
    double avgPrice;
    int numSamples;
    int boughtPrice;
};

enum OrderStatus emaStrat(struct SimState *state, struct Order *order) {
    static const long TIME_LENGTH = 30*24*60*60; // 30 days
    static const int OBSERVATION_PERIOD = 240;
    static double DISCOUNT = 0.9623506263980885; // 0.01^(1/120)

    struct emaStratAux *aux = (struct emaStratAux *)(order->aux);
    if (!order->aux) {
        order->aux = malloc(sizeof(struct emaStratAux));
        aux = (struct emaStratAux *)(order->aux);
        aux->timeHorizon = state->time + TIME_LENGTH;
        aux->havePosition = 0;
        aux->avgPrice = 0.0;
        aux->numSamples = 0;
    }

    if (state->time >= aux->timeHorizon) {
        if (aux->havePosition) {
            sell(state, &(order->symbol), order->quantity);
        }
        free(order->aux);
        return None;
    }

    int price = state->priceFn(&(order->symbol), state->time);
    aux->avgPrice = aux->avgPrice * DISCOUNT + price * (1 - DISCOUNT);
    if (aux->numSamples < OBSERVATION_PERIOD) {
        ++(aux->numSamples);
    } else if (!aux->havePosition && price < 0.9999*aux->avgPrice && 10 * price * order->quantity <= 9 * state->cash) {
        buy(state, &(order->symbol), order->quantity);
        aux->havePosition = 1;
        aux->boughtPrice  = price;
    } else if (aux->havePosition && price > 1.0001*aux->boughtPrice) {
        sell(state, &(order->symbol), order->quantity);
        aux->havePosition = 0;
    } else if (aux->havePosition && price < 0.9999*aux->boughtPrice) {
        sell(state, &(order->symbol), order->quantity);
        aux->havePosition = 0;
    }

    return Active;
}