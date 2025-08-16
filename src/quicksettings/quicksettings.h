#ifndef QUICKSETTING
#define QUICKSETTING

#include <gdk/gdk.h>

void start_quick_settings(GdkDisplay *display, GdkMonitor *monitor);
void toggle_quick_settings(GdkMonitor *monitor);

#endif // !QUICKSETTING
