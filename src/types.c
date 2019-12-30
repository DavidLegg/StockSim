#include <stdio.h>

#include "types.h"

void initSimState(struct SimState *state, time_t startTime) {
    state->time = startTime;
    state->maxActiveOrder = 0;
    state->maxActivePosition = 0;
    state->cash = 0;
    state->priceFn = NULL;

    for (int i = 0; i < MAX_ORDERS; ++i) {
        initOrder( &(state->orders[i]) );
    }
    for (int i = 0; i < MAX_ORDERS; ++i) {
        initPosition( &(state->positions[i]) );
    }
}

void initOrder(struct Order *order) {
    order->status = None;
    order->type = Buy;
    for (int i = 0; i < SYMBOL_LENGTH; ++i) {
        order->symbol.name[i] = 0;
    }
    order->quantity = 0;
}

void initPosition(struct Position *position) {
    position->quantity = 0;
    for (int i = 0; i < SYMBOL_LENGTH; ++i) {
        position->symbol.name[i] = 0;
    }
}

void printSimState(struct SimState *state) {
    const char *indent = "  ";
    char timeBuffer[128];
    strftime(timeBuffer, 128, "%m/%d/%Y %H:%M", localtime( &(state->time) ));
    printf("SimState\n%sTime: %s\n%sCash: $%0.2f\n",
        indent, timeBuffer,
        indent, state->cash / 100.0);

    unsigned long worth = state->cash;
    unsigned long positionWorth;

    if (state->maxActivePosition) {
        printf("%sPositions:\n", indent);
        for (unsigned int i = 0; i < state->maxActivePosition; ++i) {
            if (state->positions[i].quantity) {
                printf("%s%s", indent, indent);
                for (unsigned int j = 0; j < SYMBOL_LENGTH && state->positions[i].symbol.name[j]; ++j) {
                    printf("%c", state->positions[i].symbol.name[j]);
                }
                positionWorth = state->positions[i].quantity * state->priceFn(&(state->positions[i].symbol), state->time);
                worth += positionWorth;
                printf(" x %d : $%0.2f\n",
                    state->positions[i].quantity,
                    positionWorth / 100.0);
            }
        }
    } else {
        printf("%sNo positions.\n", indent);
    }

    if (state->maxActiveOrder) {
        printf("%sOrders:\n", indent);
        for (unsigned int i = 0; i < state->maxActiveOrder; ++i) {
            if (state->orders[i].status != None) {
                printf("%s%s%s ", indent, indent, textOrderType(state->orders[i].type));
                for (unsigned int j = 0; j < SYMBOL_LENGTH; ++j) {
                    printf("%c", state->orders[i].symbol.name[j] ? state->orders[i].symbol.name[j] : ' ');
                }
                printf(" x %d\n", state->orders[i].quantity);
            }
        }
    } else {
        printf("%sNo orders.\n", indent);
    }

    printf("%sWorth: $%0.2f\n", indent, worth / 100.0);
}

const char *textOrderStatus(const enum OrderStatus status) {
    switch (status) {
        case None:
            return "None";
        case Active:
            return "Active";
        default:
            return "Unknown";
    }
}

const char *textOrderType(const enum OrderType type) {
    switch (type) {
        case Buy:
            return "Buy";
        case Sell:
            return "Sell";
        default:
            return "Unknown";
    }
}