#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "stats.h"

#include "types.h"

#define STATS_LABEL_WIDTH 12
#define STATS_VALUE_WIDTH 12
#define HIST_LABEL_WIDTH 15
#define BUF_SIZE 32

const enum StatType ALL_STATS = (MINMAX | MEAN | STDDEV);

void printStatLine(const char *name, double value, enum StatsFormat fmt) {
    switch (fmt) {
        case SF_Integer:
            printf("%*s %*ld\n", STATS_LABEL_WIDTH, name, STATS_VALUE_WIDTH, (long)value);
            break;
        case SF_Float:
            printf("%*s %*.4f\n", STATS_LABEL_WIDTH, name, STATS_VALUE_WIDTH, value);
            break;
        case SF_Currency:
            {
                char buf[BUF_SIZE];
                memset(buf, 0, BUF_SIZE * sizeof(char));
                snprintf(buf, BUF_SIZE, "%*.2f", STATS_LABEL_WIDTH, value / DOLLAR );
                char *s = strrchr(buf, ' '); // find the padding cell right before the number starts
                if (s[1] == '-') {
                    // If there's a negative sign, move it before the dollar sign
                    s[0] = '-';
                    s[1] = '$';
                } else {
                    s[0] = '$';
                }
                printf("%*s %*s\n", STATS_LABEL_WIDTH, name, STATS_VALUE_WIDTH, s); // print the label and formatted string
            }
            break;
        default:
            fprintf(stderr, "Unknown stats format %d\n", fmt);
            exit(1);
    }
}
void printStats(long *data, long *dataEnd, enum StatType types, enum StatsFormat fmt) {
    int N = dataEnd - data;
    if (!N) {
        fprintf(stderr, "No data for printStats\n");
        exit(1);
    }
    printStatLine("# Data", N, SF_Integer);

    if (types & (MEAN | STDDEV)) {
        // Iterative mean algorithm, as given by Heiko Hoffman at http://www.heikohoffmann.de/htmlthesis/node134.html
        // Numerically stable, and avoids the overflow issue of total-then-divide
        double mean = 0.0;
        int t = 1;
        for (long *p = data; p < dataEnd; ++p, ++t) {
            mean += (*p - mean) / t;
        }
        if (types & MEAN) {
            printStatLine("Mean", mean, fmt);
        }
        if (types & STDDEV) {
            double sumSquares = 0.0;
            for (long *p = data; p < dataEnd; ++p) {
                sumSquares += (*p - mean) * (*p - mean);
            }
            printStatLine("Std. Dev.", sqrt(sumSquares / N), fmt);
        }
    }
    if (types & MINMAX) {
        long mn, mx;
        mn = mx = *data;
        for (long *p = data; p < dataEnd; ++p) {
            if (mn > *p) {
                mn = *p;
            } else if (mx < *p) {
                mx = *p;
            }
        }
        printStatLine("Min", mn, fmt);
        printStatLine("Max", mx, fmt);
    }
}

void printLabel(long mn, double offset, enum StatsFormat fmt) {
    switch (fmt) {
        case SF_Integer:
            printf("%*ld", HIST_LABEL_WIDTH, mn + (long)ceil(offset));
            break;
        case SF_Float:
            printf("%*.2f", HIST_LABEL_WIDTH, mn + offset);
            break;
        case SF_Currency:
            {
                char buf[BUF_SIZE];
                memset(buf, 0, BUF_SIZE * sizeof(char));
                snprintf(buf, BUF_SIZE, "%*.2f", HIST_LABEL_WIDTH, ( mn + offset ) / DOLLAR );
                char *s = strrchr(buf, ' '); // find the padding cell right before the number starts
                if (s[1] == '-') {
                    // If there's a negative sign, move it before the dollar sign
                    s[0] = '-';
                    s[1] = '$';
                } else {
                    s[0] = '$';
                }
                printf("%*s", HIST_LABEL_WIDTH, s); // print the string, starting at s, padded as necessary
            }
            break;
        default:
            fprintf(stderr, "Unknown stats format %d\n", fmt);
            exit(1);
    }
}
void drawHistogram(long *data, long *dataEnd, int numBins, enum StatsFormat fmt) {
    int bins[numBins];
    memset(bins, 0, sizeof(bins));

    long mn, mx;
    mn = mx = *data;
    int mxBin;
    
    // Calculate min/max values
    for (long *p = data + 1; p < dataEnd; ++p) {
        if (*p < mn) {
            mn = *p;
        } else if (*p > mx) {
            mx = *p;
        }
    }
    mx += 1; // elegantly handles the case when all values are equal

    // Split values up among bins
    double binWidth = (mx - mn) / (double)numBins;
    long temp;
    for (long *p = data; p < dataEnd; ++p) {
        temp = (long)floor((*p - mn) / binWidth);
        ++bins[temp >= numBins ? numBins - 1 : temp];
    }

    // Determine rescaling factor
    mxBin = 0;
    for (int *b = bins + numBins - 1; b >= bins; --b) {
        if (*b > mxBin) {
            mxBin = *b;
        }
    }

    // If necessary, rescale all bins
    // (int)ceil reduces tiny bins to 0, not 1, by design
    if (mxBin > HISTOGRAM_MAX_BAR_LENGTH) {
        double rescale = HISTOGRAM_MAX_BAR_LENGTH / (double)mxBin;
        for (int *b = bins + numBins - 1; b >= bins; --b) {
            *b = (int)ceil(*b * rescale);
        }
    }

    // Draw each bin
    for (int i = 0; i < numBins; ++i) {
        printLabel(mn, i * binWidth, fmt);
        printf(" | ");
        for (int j = bins[i] - 1; j > 0; --j) {
            printf("=");
        }
        if (bins[i]) {
            printf(">");
        }
        printf("\n");
    }
    printLabel(mx, 0, fmt);
    printf("\n");
}
