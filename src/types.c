#include <stdio.h>
#include <string.h>

#include "types.h"

const time_t SECOND = 1;
const time_t MINUTE = 60;
const time_t HOUR   = 3600;
const time_t DAY    = 86400;
const time_t WEEK   = 604800;
const time_t MONTH  = 2592000;
const time_t YEAR   = 31536000;
const int CENT = 1;
const int DOLLAR = 100;

void initSimState(struct SimState *state, time_t startTime) {
    state->time = startTime;
    state->maxActiveOrder = 0;
    state->maxActivePosition = 0;
    state->cash = 0;
    state->priceFn = NULL;
    memset(state->aux, 0, SIMSTATE_AUX_BYTES);

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
    order->customFn = NULL;
    order->symbol.id = 0;
    order->quantity = 0;
    memset(order->aux, 0, ORDER_AUX_BYTES);
}

void initPosition(struct Position *position) {
    position->quantity = 0;
    for (int i = 0; i < SYMBOL_LENGTH; ++i) {
        position->symbol.name[i] = 0;
    }
}

void copySimState(struct SimState *dest, struct SimState *src) {
    memcpy(dest->aux, src->aux, SIMSTATE_AUX_BYTES);
    memcpy(dest->orders, src->orders, sizeof(struct Order) * src->maxActiveOrder);
    memcpy(dest->positions, src->positions, sizeof(struct Position) * src->maxActivePosition);
    dest->priceFn = src->priceFn;
    dest->cash = src->cash;
    dest->maxActiveOrder = src->maxActiveOrder;
    dest->maxActivePosition = src->maxActivePosition;
    dest->time = src->time;
}

int worth(struct SimState *state) {
    int worth = state->cash;
    for (int i = 0; i < state->maxActivePosition; ++i) {
        worth += state->positions[i].quantity * state->priceFn(&(state->positions[i].symbol), state->time);
    }
    return worth;
}

void printSimState(struct SimState *state) {
    const char *indent = "  ";
    char timeBuffer[128];
    strftime(timeBuffer, 128, "%m/%d/%Y %H:%M", localtime( &(state->time) ));
    printf("SimState\n%sTime: %s\n%sCash: $%0.2f\n",
        indent, timeBuffer,
        indent, state->cash / 100.0);

    long worth = state->cash;
    long positionWorth;

    if (state->maxActivePosition) {
        printf("%sPositions:\n", indent);
        for (int i = 0; i < state->maxActivePosition; ++i) {
            if (state->positions[i].quantity) {
                printf("%s%s", indent, indent);
                for (int j = 0; j < SYMBOL_LENGTH && state->positions[i].symbol.name[j]; ++j) {
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
        for (int i = 0; i < state->maxActiveOrder; ++i) {
            if (state->orders[i].status != None) {
                printf("%s%s%s ", indent, indent, textOrderType(state->orders[i].type));
                for (int j = 0; j < SYMBOL_LENGTH && state->orders[i].symbol.name[j]; ++j) {
                    printf("%c", state->orders[i].symbol.name[j]);
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
        case Custom:
            return "Custom";
        default:
            return "Unknown";
    }
}