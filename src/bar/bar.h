#ifndef BAR_H
#define BAR_H

#include "wp/wp.h"
#include <gtk/gtk.h>

GtkWidget *bar_init_window(GdkDisplay *display, GdkMonitor *monitor);
void bar(GdkDisplay *display, GdkMonitor *monitor, GDBusConnection *conneciton,
         WpCore *core, WpObjectManager *om);

#endif // !DEBUG
