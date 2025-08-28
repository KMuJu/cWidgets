#ifndef QUICKSETTING
#define QUICKSETTING

#include "wp/core.h"
#include <gdk/gdk.h>

void start_quick_settings(GdkDisplay *display, GdkMonitor *monitor,
                          WpObjectManager *om, WpCore *core);
void toggle_quick_settings(GdkMonitor *monitor);

#endif // !QUICKSETTING
