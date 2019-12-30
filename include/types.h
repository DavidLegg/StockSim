#ifndef STRUCTS_H
#define STRUCTS_H

#include <time.h>
#include <stdint.h>


/**
 * Constants & Forward-Declarations
 */

#define MAX_ORDERS 128
#define MAX_POSITIONS 128
#define SYMBOL_LENGTH 4
#define SYMBOL_ID_TYPE uint32_t

struct SimState;



/**
 * Struct, enum, and named function types
 */

// Symbols & Pricing

union Symbol {
    // Contains both a text version, as a short char-string,
    // and an "integer" version, for quick comparison
    char name[SYMBOL_LENGTH];
    SYMBOL_ID_TYPE id;
};

// returns price as number of cents
typedef unsigned int (*GetPriceFn)(const union Symbol*, const time_t time);

// Orders

enum OrderStatus {
    None,
    Active
};

enum OrderType {
    Buy,
    Sell
};

struct Order {
    enum OrderStatus status;
    enum OrderType type;
    union Symbol symbol;
    unsigned int quantity;
};

// Positions

struct Position {
    unsigned int quantity;
    union Symbol symbol;
};

// Main Simulation State structure

struct SimState {
    struct Order orders[MAX_ORDERS];
    struct Position positions[MAX_POSITIONS];
    GetPriceFn priceFn;
    long cash;
    unsigned int maxActiveOrder;
    unsigned int maxActivePosition;
    time_t time;
};



/**
 * Initializers
 */

void initSimState(struct SimState *state, time_t startTime);
void initOrder(struct Order *order);
void initPosition(struct Position *position);



/**
 * Pretty-printers
 */

void printSimState(struct SimState *state);
const char *textOrderStatus(const enum OrderStatus status);
const char *textOrderType(const enum OrderType type);

#endif // ifndef STRUCTS_H