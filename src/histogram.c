#include <stdio.h>
#include <math.h>
#include <string.h>

#include "histogram.h"

void drawHistogram(int *data, int *dataEnd, int numBins) {
    int bins[numBins];
    memset(bins, 0, sizeof(bins));

    int mn, mx, mxBin;
    mn = mx = *data;
    
    // Calculate min/max values
    for (int *p = data + 1; p < dataEnd; ++p) {
        if (*p < mn) {
            mn = *p;
        } else if (*p > mx) {
            mx = *p;
        }
    }

    // Split values up among bins
    double binWidth = (mx - mn) / (double)numBins;
    int temp;
    for (int *p = data; p < dataEnd; ++p) {
        temp = (int)floor((*p - mn) / binWidth);
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
        printf("%10d | ", mn + (int)ceil(i * binWidth));
        for (int j = bins[i] - 1; j > 0; --j) {
            printf("=");
        }
        if (bins[i]) {
            printf(">");
        }
        printf("\n");
    }
    printf("%10d\n", mx);
}

