#ifndef MAIN_H
#define MAIN_H

#include "gio/gio.h"
#include "glib.h"
#include "wp/core.h"

typedef struct {
  GMainLoop *loop;
  GDBusConnection *dbus_connection;
  WpCore *core;
  WpObjectManager *om;
  guint pending_plugins;
  gint exit_code;
} MainContext;

#endif // !MAIN_H
