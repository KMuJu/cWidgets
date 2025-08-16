#ifndef DEVICE_H
#define DEVICE_H

#include "gio/gio.h"
#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define DEVICE_TYPE device_get_type()
G_DECLARE_FINAL_TYPE(Device, device, BLUETOOTH /*Module*/,
                     DEVICE /*Object name*/, GDBusProxy)

gboolean device_get_paired(Device *self);
gboolean device_get_connected(Device *self);
gchar *device_get_name(Device *self);
gchar *device_get_address(Device *self);

G_END_DECLS

#endif // !DEVICE_H
