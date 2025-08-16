#ifndef BATTERY_H
#define BATTERY_H

#include <glib.h>
#include <gtk/gtk.h>

void start_battery_widget(GtkWidget *box, GDBusConnection *connection);

#endif // !BATTERY_H
