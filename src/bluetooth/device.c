#include "device.h"
#include "bt.h"
#include "gio/gio.h"
#include <glib-object.h>
#include <glib.h>
#include <unistd.h>

static void on_device_properties_changed(Device *proxy,
                                         GVariant *changed_properties,
                                         GStrv invalidated_properties,
                                         gpointer user_data);

struct _Device {
  GDBusProxy parent_instance;
  Bluetooth *bt;
  gulong props_changed_signal;
};

enum {
  SIGNAL_PAIRED,
  SIGNAL_CONNECTED,
  SIGNAL_NAME,
  SIGNAL_ICON,
  N_SIGNALS,
};

static guint signals[N_SIGNALS] = {0};

G_DEFINE_TYPE(Device, device, G_TYPE_DBUS_PROXY)

static void device_dispose(GObject *gobject);
static void device_finalize(GObject *gobject);

static void device_class_init(DeviceClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = device_dispose;
  object_class->finalize = device_finalize;

  signals[SIGNAL_PAIRED] =
      g_signal_new("paired", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
                   NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  signals[SIGNAL_CONNECTED] =
      g_signal_new("connected", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
                   NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  signals[SIGNAL_NAME] =
      g_signal_new("name", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, NULL,
                   NULL, NULL, G_TYPE_NONE, 1, G_TYPE_CHAR);

  signals[SIGNAL_ICON] =
      g_signal_new("icon", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, NULL,
                   NULL, NULL, G_TYPE_NONE, 1, G_TYPE_CHAR);
}

static void device_init(Device *self) {
  self->props_changed_signal =
      g_signal_connect(self, "g-properties-changed",
                       G_CALLBACK(on_device_properties_changed), NULL);
}

static void device_dispose(GObject *gobject) {
  Device *self = BLUETOOTH_DEVICE(gobject);

  if (self->props_changed_signal != 0) {
    g_signal_handler_disconnect(self, self->props_changed_signal);
    self->props_changed_signal = 0;
  }

  g_object_unref(self->bt);

  G_OBJECT_CLASS(device_parent_class)->dispose(gobject);
}
static void device_finalize(GObject *gobject) {
  G_OBJECT_CLASS(device_parent_class)->finalize(gobject);
}

static void on_device_properties_changed(Device *proxy,
                                         GVariant *changed_properties,
                                         GStrv invalidated_properties,
                                         gpointer user_data) {
  GVariantIter iter;
  const gchar *key;
  GVariant *value;

  g_variant_iter_init(&iter, changed_properties);
  while (g_variant_iter_next(&iter, "{&sv}", &key, &value)) {
    g_message("Changed: %s", key);
    if (g_str_equal(key, "Paired")) {
      gboolean paired = g_variant_get_boolean(value);
      g_signal_emit(BLUETOOTH_DEVICE(proxy), signals[SIGNAL_PAIRED], 0, paired);
    } else if (g_str_equal(key, "Connected")) {
      gboolean connected = g_variant_get_boolean(value);
      g_signal_emit(BLUETOOTH_DEVICE(proxy), signals[SIGNAL_CONNECTED], 0,
                    connected);
    } else if (g_str_equal(key, "Name")) {
      const gchar *name = g_variant_get_string(value, NULL);
      g_signal_emit(BLUETOOTH_DEVICE(proxy), signals[SIGNAL_NAME], 0, name);
    } else if (g_str_equal(key, "Icon")) {
      const gchar *icon = g_variant_get_string(value, NULL);
      g_signal_emit(BLUETOOTH_DEVICE(proxy), signals[SIGNAL_ICON], 0, icon);
    }
    g_variant_unref(value);
  }

  bluetooth_sync(proxy->bt);
  bluetooth_update_devices(proxy->bt);
}

void device_set_bt(Device *self, Bluetooth *bt) {
  self->bt = g_object_ref(BLUETOOTH_BT(bt));
}

gboolean device_get_paired(Device *self) {
  g_return_val_if_fail(BLUETOOTH_IS_DEVICE(self), FALSE);

  GVariant *value =
      g_dbus_proxy_get_cached_property(G_DBUS_PROXY(self), "Paired");
  if (value == NULL) {
    return FALSE;
  }

  gboolean paired = g_variant_get_boolean(value);
  g_variant_unref(value);
  return paired;
}

gboolean device_get_connected(Device *self) {
  g_return_val_if_fail(BLUETOOTH_IS_DEVICE(self), FALSE);

  GVariant *value =
      g_dbus_proxy_get_cached_property(G_DBUS_PROXY(self), "Connected");
  if (value == NULL) {
    return FALSE;
  }

  gboolean connected = g_variant_get_boolean(value);
  g_variant_unref(value);
  return connected;
}

gchar *device_get_name(Device *self) {
  g_return_val_if_fail(BLUETOOTH_IS_DEVICE(self), NULL);

  GVariant *value =
      g_dbus_proxy_get_cached_property(G_DBUS_PROXY(self), "Name");
  if (value == NULL) {
    return NULL;
  }

  gchar *name = g_variant_dup_string(value, NULL);
  g_variant_unref(value);
  return name;
}

gchar *device_get_address(Device *self) {
  g_return_val_if_fail(BLUETOOTH_IS_DEVICE(self), NULL);

  GVariant *value =
      g_dbus_proxy_get_cached_property(G_DBUS_PROXY(self), "Address");
  if (value == NULL) {
    return NULL;
  }

  gchar *address = g_variant_dup_string(value, NULL);
  g_variant_unref(value);
  return address;
}

gchar *device_get_icon(Device *self) {
  g_return_val_if_fail(BLUETOOTH_IS_DEVICE(self), NULL);

  GVariant *value =
      g_dbus_proxy_get_cached_property(G_DBUS_PROXY(self), "Icon");
  if (value == NULL) {
    return NULL;
  }

  gchar *icon = g_variant_dup_string(value, NULL);
  g_variant_unref(value);
  return icon;
}
