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
void adapter_set_powered(Adapter *self, gboolean powered);
gboolean adapter_get_discovering(Adapter *self);
void adapter_set_discovering(Adapter *self, gboolean discovering);

G_END_DECLS

#endif // !ADAPTER_H
