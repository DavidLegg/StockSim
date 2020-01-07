#ifndef STRUCTS_H
#define STRUCTS_H

#include <time.h>
#include <stdint.h>

/**
 * Debugging utilities
 */

#ifdef DEBUG
#include <stdio.h>
#define db_printf(format, ...) printf("DEBUG:%s:%d: " format, __BASE_FILE__, __LINE__, __VA_ARGS__)
#else
#define db_printf(format, ...) // delete db_printf on non-debug builds
#endif

// Usage: Include a line like the following in the header after declaring the struct
//   BOUND_SIZE(struct MyStruct, BYTES_AVAILABLE_FOR_MYSTRUCT);
#define BOUND_SIZE(type,bound) _Static_assert( (sizeof(type) <= bound), "Type " #type " exceeds size bound " #bound )

/**
 * Constants & Forward-Declarations
 */

#define MAX_ORDERS 128
#define MAX_POSITIONS 128
#define SYMBOL_LENGTH 4
#define SYMBOL_ID_TYPE uint32_t
#define ORDER_AUX_BYTES 64
#define SIMSTATE_AUX_BYTES 64

// Nominal time units for convenient use later on.
extern const time_t SECOND;
extern const time_t MINUTE;
extern const time_t HOUR;
extern const time_t DAY;
extern const time_t WEEK;
extern const time_t MONTH;
extern const time_t YEAR;
extern const int CENT;
extern const int DOLLAR;

struct Order;
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
typedef int (*GetPriceFn)(const union Symbol*, const time_t time);

// Orders

enum OrderStatus {
    None,
    Active
};

enum OrderType {
    Buy,
    Sell,
    Custom
};

typedef enum OrderStatus OrderFn(struct SimState*, struct Order*);

struct Order {
    char aux[ORDER_AUX_BYTES];
    enum OrderStatus status;
    enum OrderType type;
    OrderFn *customFn;
    union Symbol symbol;
    int quantity;
};

// Positions

struct Position {
    int quantity;
    union Symbol symbol;
};

// Main Simulation State structure

struct SimState {
    char aux[SIMSTATE_AUX_BYTES];
    struct Order orders[MAX_ORDERS];
    struct Position positions[MAX_POSITIONS];
    GetPriceFn priceFn;
    long cash;
    int maxActiveOrder;
    int maxActivePosition;
    time_t time;
};



/**
 * Initializers
 */

void initSimState(struct SimState *state, time_t startTime);
void initOrder(struct Order *order);
void initPosition(struct Position *position);


/**
 * Efficient copy functions
 */
void copySimState(struct SimState *dest, struct SimState *src);


/**
 * Pretty-printers
 */

void printSimState(struct SimState *state);
const char *textOrderStatus(const enum OrderStatus status);
const char *textOrderType(const enum OrderType type);

#endif // ifndef STRUCTS_H