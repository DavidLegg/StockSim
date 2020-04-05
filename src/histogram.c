#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "types.h"

#include "histogram.h"

#define HIST_LABEL_WIDTH 15

void drawHistogram(long *data, long *dataEnd, int numBins, enum HistogramFormat fmt) {
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

    const int bufSize = 32;
    char buf[bufSize];
    memset(buf, 0, bufSize * sizeof(char));
    char *s;

    // Draw each bin
    for (int i = 0; i < numBins; ++i) {
        switch (fmt) {
            case HF_Integer:
                printf("%*ld", HIST_LABEL_WIDTH, mn + (long)ceil(i * binWidth));
                break;
            case HF_Float:
                printf("%*.2f", HIST_LABEL_WIDTH, mn + i * binWidth);
                break;
            case HF_Currency:
                snprintf(buf, bufSize, "%*.2f", HIST_LABEL_WIDTH, ( mn + i * binWidth ) / DOLLAR );
                s = strrchr(buf, ' '); // find the padding cell right before the number starts
                *s = '$'; // add the dollar sign there
                printf("%*s", HIST_LABEL_WIDTH, s); // and print the string, starting at s, padded as necessary
                break;
            default:
                fprintf(stderr, "Unknown histogram format %d\n", fmt);
                exit(1);
        }
        printf(" | ");
        for (int j = bins[i] - 1; j > 0; --j) {
            printf("=");
        }
        if (bins[i]) {
            printf(">");
        }
        printf("\n");
    }
    switch (fmt) {
        case HF_Integer:
            printf("%*ld", HIST_LABEL_WIDTH, mx);
            break;
        case HF_Float:
            printf("%*.2f", HIST_LABEL_WIDTH, (double)mx);
            break;
        case HF_Currency:
            snprintf(buf, bufSize, "%*.2f", HIST_LABEL_WIDTH, mx / (double)DOLLAR );
            s = strrchr(buf, ' '); // find the padding cell right before the number starts
            *s = '$'; // add the dollar sign there
            printf("%*s", HIST_LABEL_WIDTH, s); // and print the string, starting at s, padded as necessary
            break;
        default:
            fprintf(stderr, "Unknown histogram format %d\n", fmt);
            exit(1);
    }
    printf("\n");
}

