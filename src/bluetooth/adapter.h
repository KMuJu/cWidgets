#ifndef ADAPTER_H
#define ADAPTER_H

#include "gio/gio.h"
#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define ADAPTER_TYPE adapter_get_type()
G_DECLARE_FINAL_TYPE(Adapter, adapter, BLUETOOTH /*Module*/,
                     ADAPTER /*Object name*/, GDBusProxy)

gchar *adapter_get_name(Adapter *self);
gchar *adapter_get_address(Adapter *self);
gboolean adapter_get_powered(Adapter *self);
gboolean adapter_get_discovering(Adapter *self);

G_END_DECLS

#endif // !ADAPTER_H
