#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "execution.h"

int TRANSACTION_FEE = 25;

/**
 * Main execution loops
 */

void runScenario(struct SimState *state) {
    while (state->maxActiveOrder) {
        step(state);
    }
}

void runScenarioDemo(struct SimState *state, int waitTimeMs) {
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

void graphScenario(struct SimState *state) {
    FILE *gp = popen("gnuplot -persistent", "w");
    fprintf(gp, "set title 'Stock Simulation'\n");
    fprintf(gp, "plot '-' with lines\n");
    while (state->maxActiveOrder) {
        step(state);
        fprintf(gp, "%ld %f\n", state->time, worth(state) / 100.0);
    }
    fprintf(gp, "e\n");
}

void step(struct SimState *state) {
    state->time += (MINUTES_PER_STEP * 60);
    long transactionCost = 0;
    for (int i = 0; i < state->maxActiveOrder; ++i) {
        if (state->orders[i].status == Active) {
            switch (state->orders[i].type) {
                case Buy:
                    transactionCost = state->orders[i].quantity * state->priceFn(&(state->orders[i].symbol), state->time);
                    transactionCost += transactionCost * TRANSACTION_FEE / 10000;
                    if (state->cash >= transactionCost) {
                        state->cash -= transactionCost;
                        addPosition(state, &(state->orders[i].symbol), state->orders[i].quantity);
                        state->orders[i].status = None; // delete this order once it executes
                    } else {
                        fprintf(stderr, "Insufficient cash.\n");
                        exit(1);
                    }
                    break;
                case Sell:
                    {int j;
                    for (j = 0; j < state->maxActivePosition; ++j) {
                        if (state->positions[j].symbol.id == state->orders[i].symbol.id) {
                            if (state->positions[j].quantity >= state->orders[i].quantity) {
                                transactionCost = state->orders[i].quantity * state->priceFn(&(state->orders[i].symbol), state->time);
                                transactionCost -= transactionCost * TRANSACTION_FEE / 10000;
                                state->positions[j].quantity -= state->orders[i].quantity;
                                state->cash += transactionCost;
                                state->orders[i].status = None; // delete this order once it executes
                            } else {
                                fprintf(stderr, "Insufficient shares.\n");
                                exit(1);
                            }
                            break;
                        }
                    }
                    if (j >= state->maxActivePosition) {
                        fprintf(stderr, "Insufficient shares.\n");
                        exit(1);
                    }}
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

void addPosition(struct SimState *state, union Symbol *symbol, int quantity) {
    int i;
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

void buy(struct SimState *state, union Symbol *symbol, int quantity) {
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

void sell(struct SimState *state, union Symbol *symbol, int quantity) {
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
    int quantity,
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
