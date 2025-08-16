#ifndef BT_H
#define BT_H

#include "gio/gio.h"
#include <glib-object.h>
#include <glib.h>

#define BLUETOOTH_TYPE bluetooth_get_type()
G_DECLARE_FINAL_TYPE(Bluetooth, bluetooth, BLUETOOTH /*Module*/,
                     BT /*Object name*/, GObject)

Bluetooth *bluetooth_get_default(void);
void bluetooth_set_dbus_conn(GDBusConnection *om);
void bluetooth_install_signals(Bluetooth *self);

#endif // !BT_H
