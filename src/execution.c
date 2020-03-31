#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include "execution.h"

int TRANSACTION_FEE = 25;
int MINUTES_PER_STEP = 12*60;

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
    if (!gp) {
        fprintf(stderr, "Could not start gnuplot.\n");
        perror("Error");
        exit(1);
    }
    fprintf(gp, "%s",
        "set title 'Stock Simulation'\n"
        "set timefmt \"%s\"\n"
        "set xdata time\n"
        "set format x \"%m/%d/%y\"\n"
        "set xtics rotate by 60 offset -3,-3\n"
        "set bmargin 4\n"
        "plot '-' using 1:2 title 'Net Worth' with lines\n");
    while (state->maxActiveOrder) {
        step(state);
        fprintf(gp, "%ld %f\n", state->time, worth(state) / (double)DOLLAR);
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
                    transactionCost = state->orders[i].quantity * state->priceFn(&(state->orders[i].symbol), state->time, state->priceCache);
                    transactionCost += transactionCost * TRANSACTION_FEE / 10000;
                    if (state->cash >= transactionCost) {
                        state->cash -= transactionCost;
                        addPosition(state, &(state->orders[i].symbol), state->orders[i].quantity);
                        state->orders[i].status = None; // delete this order once it executes
                    } else {
                        db_printf("State time: %ld", state->time);
                        printSimState(state);
                        fprintf(stderr, "Buy Error - Insufficient cash: Have $%.2f, need $%.2f\n", state->cash / (double)DOLLAR, transactionCost / (double)DOLLAR);
                        exit(1);
                    }
                    break;
                case Sell:
                    {int j;
                    for (j = 0; j < state->maxActivePosition; ++j) {
                        if (state->positions[j].symbol.id == state->orders[i].symbol.id) {
                            if (state->positions[j].quantity >= state->orders[i].quantity) {
                                transactionCost = state->orders[i].quantity * state->priceFn(&(state->orders[i].symbol), state->time, state->priceCache);
                                transactionCost -= transactionCost * TRANSACTION_FEE / 10000;
                                state->positions[j].quantity -= state->orders[i].quantity;
                                state->cash += transactionCost;
                                state->orders[i].status = None; // delete this order once it executes
                            } else {
                                db_printf("State time: %ld", state->time);
                                printSimState(state);
                                fprintf(stderr, "Sell Error - Insufficient shares: Have %.*s x %d, need %d\n", SYMBOL_LENGTH, state->positions[j].symbol.name, state->positions[j].quantity, state->orders[i].quantity);
                                exit(1);
                            }
                            break;
                        }
                    }
                    if (j >= state->maxActivePosition) {
                        db_printf("State time: %ld", state->time);
                        printSimState(state);
                        fprintf(stderr, "Sell Error - No position for sell order %.*s x %d\n", SYMBOL_LENGTH, state->orders[i].symbol.name, state->orders[i].quantity);
                        exit(1);
                    }}
                    break;
                case Custom:
                    state->orders[i].status = state->orders[i].customFn(state, state->orders + i);
                    break;
                default:
                    db_printf("State time: %ld", state->time);
                    printSimState(state);
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

struct Order *buy(struct SimState *state, union Symbol *symbol, int quantity) {
    if (state->maxActiveOrder >= MAX_ORDERS) {
        fprintf(stderr, "No more orders available.\n");
        exit(1);
    }
    state->orders[state->maxActiveOrder].status    = Active;
    state->orders[state->maxActiveOrder].type      = Buy;
    state->orders[state->maxActiveOrder].symbol.id = symbol->id;
    state->orders[state->maxActiveOrder].quantity  = quantity;
    state->orders[state->maxActiveOrder].customFn  = NULL;
    return state->orders + state->maxActiveOrder++;
}

struct Order *sell(struct SimState *state, union Symbol *symbol, int quantity) {
    if (state->maxActiveOrder >= MAX_ORDERS) {
        fprintf(stderr, "No more orders available.\n");
        exit(1);
    }
    state->orders[state->maxActiveOrder].status    = Active;
    state->orders[state->maxActiveOrder].type      = Sell;
    state->orders[state->maxActiveOrder].symbol.id = symbol->id;
    state->orders[state->maxActiveOrder].quantity  = quantity;
    state->orders[state->maxActiveOrder].customFn  = NULL;
    return state->orders + state->maxActiveOrder++;
}

struct Order *makeCustomOrder(
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
    state->orders[state->maxActiveOrder].symbol.id = (symbol ? symbol->id : 0);
    state->orders[state->maxActiveOrder].quantity  = quantity;
    state->orders[state->maxActiveOrder].customFn  = customFn;
    return state->orders + state->maxActiveOrder++;
}
