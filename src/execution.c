#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "execution.h"

/**
 * Main execution loops
 */

void runScenario(struct SimState *state) {
    while (state->maxActiveOrder) {
        step(state);
    }
}

void runScenarioDemo(struct SimState *state, unsigned int waitTimeMs) {
    struct timespec waitTime;
    waitTime.tv_sec  = waitTimeMs / 1000;
    waitTime.tv_nsec = (waitTimeMs % 1000) * 1000000L;

    while (state->maxActiveOrder) {
        printSimState(state);
        nanosleep(&waitTime, NULL);
        step(state);
    }
    printSimState(state);
}

void step(struct SimState *state) {
    state->time += (MINUTES_PER_STEP * 60);
    long transactionCost = 0;
    for (unsigned int i = 0; i < state->maxActiveOrder; ++i) {
        if (state->orders[i].status == Active) {
            switch (state->orders[i].type) {
                case Buy:
                    transactionCost = state->orders[i].quantity * state->priceFn(&(state->orders[i].symbol), state->time);
                    if (state->cash > transactionCost) {
                        state->cash -= transactionCost;
                        addPosition(state, &(state->orders[i].symbol), state->orders[i].quantity);
                        state->orders[i].status = None; // delete this order once it executes
                    } // TODO: handle insufficient funds case?
                    break;
                case Sell:
                    for (unsigned int j = 0; j < state->maxActivePosition; ++j) {
                        if (state->positions[j].symbol.id == state->orders[i].symbol.id) {
                            if (state->positions[j].quantity >= state->orders[i].quantity) {
                                state->positions[j].quantity -= state->orders[i].quantity;
                                state->cash += state->orders[i].quantity * state->priceFn(&(state->orders[i].symbol), state->time);
                                state->orders[i].status = None; // delete this order once it executes
                            } // TODO: handle insufficient quantity case
                            break;
                        }
                    }
                    // TODO: handle no position held case
                    break;
                case Custom:
                    state->orders[i].status = state->orders[i].customFn(state, state->orders + i);
                    break;
                default:
                    fprintf(stderr, "Unhandled order type %d.\n", state->orders[i].type);
                    exit(1);
            }
        }
    }
    while (state->maxActiveOrder > 0 && 
           state->orders[state->maxActiveOrder - 1].status != Active) {
        --(state->maxActiveOrder);
    }
    while (state->maxActivePosition > 0 && 
           state->positions[state->maxActivePosition - 1].quantity == 0) {
        --(state->maxActivePosition);
    }
}

/**
 * Single-Transaction helpers
 */

void addPosition(struct SimState *state, union Symbol *symbol, unsigned int quantity) {
    unsigned int i;
    for (i = 0; i < state->maxActivePosition; ++i) {
        if (state->positions[i].symbol.id == symbol->id) break;
    }
    if (i >= MAX_POSITIONS) {
        fprintf(stderr, "No more positions available.\n");
        exit(1);
    }
    state->positions[i].quantity += quantity;
    if (i == state->maxActivePosition) {
        ++(state->maxActivePosition);
        state->positions[i].symbol = *symbol;
    }
}

void buy(struct SimState *state, union Symbol *symbol, unsigned int quantity) {
    if (state->maxActiveOrder >= MAX_ORDERS) {
        fprintf(stderr, "No more orders available.\n");
        exit(1);
    }
    state->orders[state->maxActiveOrder].status    = Active;
    state->orders[state->maxActiveOrder].type      = Buy;
    state->orders[state->maxActiveOrder].symbol.id = symbol->id;
    state->orders[state->maxActiveOrder].quantity  = quantity;
    state->orders[state->maxActiveOrder].customFn  = NULL;
    ++(state->maxActiveOrder);
}

void sell(struct SimState *state, union Symbol *symbol, unsigned int quantity) {
    if (state->maxActiveOrder >= MAX_ORDERS) {
        fprintf(stderr, "No more orders available.\n");
        exit(1);
    }
    state->orders[state->maxActiveOrder].status    = Active;
    state->orders[state->maxActiveOrder].type      = Sell;
    state->orders[state->maxActiveOrder].symbol.id = symbol->id;
    state->orders[state->maxActiveOrder].quantity  = quantity;
    state->orders[state->maxActiveOrder].customFn  = NULL;
    ++(state->maxActiveOrder);
}

void makeCustomOrder(
    struct SimState *state,
    union Symbol *symbol,
    unsigned int quantity,
    OrderFn *customFn) {

    if (state->maxActiveOrder >= MAX_ORDERS) {
        fprintf(stderr, "No more orders available.\n");
        exit(1);
    }
    state->orders[state->maxActiveOrder].status    = Active;
    state->orders[state->maxActiveOrder].type      = Custom;
    state->orders[state->maxActiveOrder].symbol.id = symbol->id;
    state->orders[state->maxActiveOrder].quantity  = quantity;
    state->orders[state->maxActiveOrder].customFn  = customFn;
    ++(state->maxActiveOrder);
}
