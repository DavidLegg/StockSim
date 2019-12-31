#include "strategies.h"
#include "execution.h"

enum OrderStatus basicStrat1(struct SimState *state, struct Order *order) {
    static const int MAX_ITERS = 1;
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