#include <stdio.h>
#include <math.h>
#include <string.h>

#include "histogram.h"

void drawHistogram(long *data, long *dataEnd, int numBins) {
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
        printf("%10ld | ", mn + (long)ceil(i * binWidth));
        for (int j = bins[i] - 1; j > 0; --j) {
            printf("=");
        }
        if (bins[i]) {
            printf(">");
        }
        printf("\n");
    }
    printf("%10ld\n", mx);
}

