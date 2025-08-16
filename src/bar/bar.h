#ifndef BAR_H
#define BAR_H

#include "wp/wp.h"
#include <gtk/gtk.h>

void bar(GtkWidget *window, GDBusConnection *conneciton, WpCore *core,
         WpObjectManager *om);

#endif // !DEBUG
