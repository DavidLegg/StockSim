#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#define HISTOGRAM_MAX_BAR_LENGTH 64

enum HistogramFormat {HF_Integer, HF_Float, HF_Currency};
void drawHistogram(long *data, long *dataEnd, int numBins, enum HistogramFormat fmt);

#endif // ifndef HISTOGRAM_H