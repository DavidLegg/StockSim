#ifndef STATS_H
#define STATS_H

#define HISTOGRAM_MAX_BAR_LENGTH 64

enum StatsFormat {SF_Integer, SF_Float, SF_Currency};

// TODO: add constant flags for which stats to print,
//   that can be |'d together to get combos
//   Also add the flag param below, and implement.
enum StatType {
    MINMAX = 1,
    MEAN   = 2,
    STDDEV = 4
};
const enum StatType ALL_STATS;
void printStats(long *data, long *dataEnd, enum StatType types, enum StatsFormat fmt);

void drawHistogram(long *data, long *dataEnd, int numBins, enum StatsFormat fmt);

#endif // ifndef STATS_H