#ifndef DISPLAY_TOOLS_H
#define DISPLAY_TOOLS_H

/**
 * Progress Bar
 *   Call initProgressBar with number of updates to draw,
 *   Then call updateProgressBar every time you want an update
 */
void initProgressBar(int size);
void updateProgressBar();

#endif // ifndef DISPLAY_TOOLS_H