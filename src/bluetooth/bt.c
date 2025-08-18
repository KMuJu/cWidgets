#include "bt.h"
#include "device.h"
#include "gio/gdbusobjectmanagerclient.h"
#include "gio/gdbusobjectproxy.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"

enum {
  PROP_0,
  PROP_CONNECTED,
  N_PROPS,
};

enum {
  SIGNAL_DEVICES_CHANGED,
  SIGNAL_ADAPTERS_CHANGED,
  SIGNAL_CONNECTED_CHANGED,
  SIGNAL_POWERED_CHANGED,
  N_SIGNALS,
};

struct _Bluetooth {
  GObject parent_instance;
  GHashTable *devices;
  GHashTable *adapters;
  gboolean connected;
  gboolean powered;
  gboolean signals_connected;
};

static GParamSpec *obj_properties[N_PROPS] = {NULL};
static guint signals[N_SIGNALS] = {0};

G_DEFINE_TYPE(Bluetooth, bluetooth, G_TYPE_OBJECT)

static GDBusConnection *dbus_conn = NULL;
static GDBusObjectManager *dbus_om = NULL;

GType dbus_om_get_type(GDBusObjectManagerClient *manager,
                       const gchar *object_path, const gchar *interface_name,
                       gpointer data) {
  // g_print("Creating proxy for object: %s, interface: %s\n", object_path,
  //         interface_name ? interface_name : "(null)");

  if (NULL == interface_name) {
    return G_TYPE_DBUS_OBJECT_PROXY;
  }

  if (g_str_equal(interface_name, "org.bluez.Device1")) {
    return DEVICE_TYPE;
  }
  if (g_str_equal(interface_name, "org.bluez.Adapter1")) {
    return ADAPTER_TYPE;
  }

  return G_TYPE_DBUS_PROXY;
}

void bluetooth_set_dbus_conn(GDBusConnection *conn) {
  if (dbus_conn)
    g_object_unref(dbus_conn);
  dbus_conn = g_object_ref(conn);
}

static GDBusObjectManager *bluetooth_get_dbus_om(void) {
  if (dbus_om)
    return dbus_om;
  if (!dbus_conn) {
    g_error("Tried to get dbus object manager before setting dbus connection");
    return NULL;
  }
  GError *error = NULL;
  dbus_om = g_dbus_object_manager_client_new_sync(
      dbus_conn, G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE, "org.bluez", "/",
      dbus_om_get_type, NULL, NULL, NULL, &error);
  return dbus_om;
}

GDBusConnection *bluetooth_get_dbus_conn(void) { return dbus_conn; }

static void bluetooth_dispose(GObject *object) {
  Bluetooth *self = BLUETOOTH_BT(object);

  g_clear_pointer(&self->devices, g_hash_table_unref);
  g_clear_pointer(&self->adapters, g_hash_table_unref);

  G_OBJECT_CLASS(bluetooth_parent_class)->dispose(object);
}

static void bluetooth_class_init(BluetoothClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->dispose = bluetooth_dispose;

  /* Register signals */
  signals[SIGNAL_DEVICES_CHANGED] = g_signal_new(
      "devices-changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_HASH_TABLE);

  signals[SIGNAL_ADAPTERS_CHANGED] = g_signal_new(
      "adapters-changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_HASH_TABLE);

  signals[SIGNAL_CONNECTED_CHANGED] =
      g_signal_new("connected", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
                   NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  signals[SIGNAL_POWERED_CHANGED] =
      g_signal_new("powered", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
                   NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

static gboolean bluetooth_get_connected(Bluetooth *self) {
  GHashTableIter iter;
  gpointer value;

  g_hash_table_iter_init(&iter, self->devices);
  while (g_hash_table_iter_next(&iter, NULL, &value)) {
    Device *device = value;
    if (device_get_connected(device))
      return TRUE;
  }
  return FALSE;
}

static gboolean bluetooth_get_powered(Bluetooth *self) {
  GHashTableIter iter;
  gpointer value;

  g_hash_table_iter_init(&iter, self->adapters);
  while (g_hash_table_iter_next(&iter, NULL, &value)) {
    Adapter *adapter = value;
    if (adapter_get_powered(adapter))
      return TRUE;
  }
  return FALSE;
}

static void bluetooth_sync(Bluetooth *self) {
  gboolean powered = bluetooth_get_powered(self);
  gboolean connected = bluetooth_get_connected(self);

  if (self->powered != powered) {
    self->powered = powered;
    g_signal_emit(self, SIGNAL_POWERED_CHANGED, 0, powered);
  }
  if (self->connected != connected) {
    self->connected = connected;
    g_signal_emit(self, SIGNAL_CONNECTED_CHANGED, 0, connected);
  }
}

static void on_interface_added(GDBusObjectManager *manager, GDBusObject *object,
                               GDBusInterface *interface, gpointer user_data) {

  Bluetooth *bt = BLUETOOTH_BT(user_data);
  if (BLUETOOTH_IS_DEVICE(interface)) {
    Device *device = BLUETOOTH_DEVICE(interface);
    const gchar *obj_path = g_dbus_object_get_object_path(object);

    g_hash_table_insert(bt->devices, g_strdup(obj_path), g_object_ref(device));

    g_signal_emit(bt, signals[SIGNAL_DEVICES_CHANGED], 0, bt->devices);
  }
  if (BLUETOOTH_IS_ADAPTER(interface)) {
    Adapter *adapter = BLUETOOTH_ADAPTER(interface);
    const gchar *obj_path = g_dbus_object_get_object_path(object);

    g_hash_table_insert(bt->adapters, g_strdup(obj_path),
                        g_object_ref(adapter));

    g_signal_emit(bt, signals[SIGNAL_ADAPTERS_CHANGED], 0, bt->adapters);
  }
}

static void on_interface_removed(GDBusObjectManager *manager,
                                 GDBusObject *object, GDBusInterface *interface,
                                 gpointer user_data) {
  Bluetooth *bt = BLUETOOTH_BT(user_data);
  if (BLUETOOTH_IS_DEVICE(interface)) {
    const gchar *obj_path = g_dbus_object_get_object_path(object);

    g_hash_table_remove(bt->adapters, obj_path);

    g_signal_emit(bt, signals[SIGNAL_ADAPTERS_CHANGED], 0, bt->adapters);

    bluetooth_sync(bt);
  }
  if (BLUETOOTH_IS_ADAPTER(interface)) {
    const gchar *obj_path = g_dbus_object_get_object_path(object);

    g_hash_table_remove(bt->devices, obj_path);

    g_signal_emit(bt, signals[SIGNAL_DEVICES_CHANGED], 0, bt->devices);

    bluetooth_sync(bt);
  }
}

static void on_object_added(GDBusObjectManager *manager, GDBusObject *object,
                            gpointer user_data) {
  GList *ifaces = g_dbus_object_get_interfaces(object);
  for (GList *l = ifaces; l != NULL; l = l->next) {
    on_interface_added(manager, object, G_DBUS_INTERFACE(l->data), user_data);
  }
  g_list_free_full(ifaces, g_object_unref);
}

static void on_object_removed(GDBusObjectManager *manager, GDBusObject *object,
                              gpointer user_data) {
  GList *ifaces = g_dbus_object_get_interfaces(object);
  for (GList *l = ifaces; l != NULL; l = l->next) {
    on_interface_removed(manager, object, G_DBUS_INTERFACE(l->data), user_data);
  }
  g_list_free_full(ifaces, g_object_unref);
}

static void bluetooth_init(Bluetooth *self) {
  self->devices =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
  self->adapters = g_hash_table_new(g_str_hash, g_str_equal);
  self->signals_connected = FALSE;
}

static Bluetooth *bluetooth = NULL;

// Client should unref
Bluetooth *bluetooth_get_default(void) {
  if (NULL != bluetooth) {
    return g_object_ref(bluetooth);
  }

  bluetooth = (Bluetooth *)g_object_new(BLUETOOTH_TYPE, NULL);

  return g_object_ref(bluetooth);
}

void bluetooth_install_signals(Bluetooth *self) {
  if (self->signals_connected)
    return;

  GDBusObjectManager *manager = bluetooth_get_dbus_om();

  g_signal_connect(manager, "interface-added", G_CALLBACK(on_interface_added),
                   self);
  g_signal_connect(manager, "interface-removed",
                   G_CALLBACK(on_interface_removed), self);
  g_signal_connect(manager, "object-added", G_CALLBACK(on_object_added), self);
  g_signal_connect(manager, "object-removed", G_CALLBACK(on_object_removed),
                   self);

  // Iterate over existing objects
}

void bluetooth_call_signals(Bluetooth *self) {
  GDBusObjectManager *manager = bluetooth_get_dbus_om();

  GList *objects = g_dbus_object_manager_get_objects(manager);
  for (GList *l = objects; l != NULL; l = l->next) {
    GDBusObject *object = G_DBUS_OBJECT(l->data);
    GList *ifaces = g_dbus_object_get_interfaces(object);
    for (GList *il = ifaces; il != NULL; il = il->next) {
      on_interface_added(manager, object, G_DBUS_INTERFACE(il->data), self);
    }
    g_list_free_full(ifaces, g_object_unref);
  }
  g_list_free_full(objects, g_object_unref);

  // Emit signals for state
  gboolean powered = bluetooth_get_powered(self);
  g_signal_emit(self, signals[SIGNAL_POWERED_CHANGED], 0, powered);
  gboolean connected = bluetooth_get_connected(self);
  g_signal_emit(self, signals[SIGNAL_CONNECTED_CHANGED], 0, connected);

  self->signals_connected = TRUE;
}

// Client should unref
Adapter *bluetooth_get_adapter(Bluetooth *self) {
  if (g_hash_table_size(self->adapters) == 0) {
    return NULL;
  }
  GList *values = g_hash_table_get_values(self->adapters);
  return g_object_ref(g_list_first(values)->data);
}
