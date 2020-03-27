#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define __USE_XOPEN
#include <time.h>

#include "strategy_testing.h"

#include "batch_execution.h"
#include "histogram.h"

#define GRID_BUFFER_SIZE 256

void initGridBaseScenarios(struct SimState *base, struct SimState *slots, double * const *params, const double *lows, const double *highs, const int *ns, int offset, int k) {
    if (k <= 0) {
        copySimState(slots, base);
    } else {
        offset /= *ns;
        double inc = (*highs - *lows) / *ns;
        **params = *lows;
        for (int i = 0; i < *ns; ++i) {
            initGridBaseScenarios(base, slots + i * offset, params + 1, lows + 1, highs + 1, ns + 1, offset, k - 1);
            **params += inc;
        }
    }
}
void gridTest(struct SimState *baseScenario, double * const *params, const double *lows, const double *highs, const int *ns, int numParams, int numShots, const char **paramNames) {
    // Init some baseScenarios with the param varied
    int numScenarios = 1;
    for (int i = 0; i < numParams; ++i) {
        numScenarios *= ns[i];
    }
    struct SimState *baseScenarios = malloc(sizeof(struct SimState) * numScenarios);

    initGridBaseScenarios(baseScenario, baseScenarios, params, lows, highs, ns, numScenarios, numParams);

    // Default setup:
    struct tm structSimStartTime;
    strptime("1/1/2019 01:00", "%m/%d/%Y%n%H:%M", &structSimStartTime);
    structSimStartTime.tm_isdst = -1;
    structSimStartTime.tm_sec = 0;
    time_t simStartTime, simEndTime;
    simStartTime = mktime(&structSimStartTime);
    simEndTime   = simStartTime + 300*DAY;
    long skipSeconds = (simEndTime - simStartTime) / (numShots - 1);

    // Execute and collect results
    int numTimes = 0;
    int *results = runTimesMulti(baseScenarios, numScenarios, simStartTime, simEndTime, skipSeconds, &numTimes);

    // Pretty-print results:
    printGridResults(results, paramNames, lows, highs, ns, numScenarios * numTimes, baseScenario->cash, numParams, "");
    free(results);
}
void printGridResults(int *results, const char **paramNames, const double *lows, const double *highs, const int *ns, int offset, int startCash, int k, const char *labelInProgress) {
    if (k <= 0) {
        // Done with index calculations. Compute statistics
        int numSuccesses = 0;
        double avgProfit = 0.0;
        for (int j = 0; j < offset; ++j) {
            avgProfit += (results[j] -= startCash);
            if (results[j] > 0) {
                ++numSuccesses;
            }
        }
        avgProfit /= offset;

        // Print out statistics and histogram
        printf("%s\n", labelInProgress);
        printf("  Profit on %d/%d (%.0f%%)\n",
            numSuccesses,
            offset,
            100.0 * numSuccesses / offset);
        printf("  Average profit: ");
        if (avgProfit > 0) {
            printf("$%.4f (+%.1f%%)\n", avgProfit / 100, 100 * avgProfit / startCash);
        } else {
            printf("-$%.4f (-%.1f%%)\n", -avgProfit / 100, -100 * avgProfit / startCash);
        }
        printf("  Profit distribution:\n");
        drawHistogram(results, results + offset, 10);

    } else {
        offset /= *ns;
        double inc = (*highs - *lows) / *ns;
        char buf[GRID_BUFFER_SIZE];
        for (int i = 0; i < *ns; ++i) {
            snprintf(buf, GRID_BUFFER_SIZE, "%s%s = %f, ", labelInProgress, *paramNames, *lows + i * inc);
            printGridResults(results + i * offset, paramNames + 1, lows + 1, highs + 1, ns + 1, offset, startCash, k - 1, buf);
        }
    }
}
