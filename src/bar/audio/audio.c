#include "audio.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"
#include "spa/param/props.h"
#include "spa/pod/pod.h"
#include "wp/core.h"
#include "wp/node.h"
#include "wp/object.h"
#include "wp/plugin.h"
#include "wp/proxy-interfaces.h"
#include "wp/spa-pod.h"
#include "wp/wp.h"
#include <spa/pod/iter.h>

static const char ICON_MUTED[] = "audio-volume-muted-symbolic";
static const char ICON_LOW[] = "audio-volume-low-symbolic";
static const char ICON_MEDIUM[] = "audio-volume-medium-symbolic";
static const char ICON_HIGH[] = "audio-volume-high-symbolic";
static const char ICON_OVERAMPLIFIED[] = "audio-volume-overamplified-symbolic";

typedef struct {
  GtkWidget *image;
  GtkWidget *label;
  guint32 default_sink_id;
  WpNode *default_sink_node;
  WpPlugin *mixer_api;
} AudioState;

static void update_audio_ui(AudioState *aw, gboolean muted, int audioLevel) {
  if (audioLevel == -1) {
    g_message("No active audio\n");
    return;
  }
  g_message("Update ui with: Muted(%d), AudioLevel(%d)\n", muted, audioLevel);
}
static void update_volume_info(AudioState *data, guint32 node_id) {}

static void on_mixer_changed(WpPlugin *mixer_api, guint32 node_id,
                             gpointer user_data) {
  AudioState *data = (AudioState *)user_data;
  g_message("Node changed\n");

  // Only update if it's our default sink
  if (node_id == data->default_sink_id) {
    g_message("Default sink changed\n");
    update_volume_info(data, node_id);
  }
}

static void object_added_callback(WpObjectManager *self, gpointer object,
                                  gpointer user_data) {
  AudioState *data = user_data;
  g_message("Object added\n");
  if (!WP_IS_NODE(object)) {
    g_printerr("Oject is not a node\n");
    return;
  }

  WpNode *node = WP_NODE(object);

  g_autoptr(WpProperties) props =
      wp_pipewire_object_get_properties(WP_PIPEWIRE_OBJECT(node));

  const gchar *media_class = wp_properties_get(props, "media.class");
  const gchar *node_name = wp_properties_get(props, "node.name");

  // Check if this is the default audio sink
  if (g_strcmp0(media_class, "Audio/Sink") == 0) {
    data->default_sink_id = wp_proxy_get_bound_id(WP_PROXY(node));
    data->default_sink_node = g_object_ref(node); // Keep reference

    g_print("Found default sink: %s (ID: %u)\n", node_name,
            data->default_sink_id);
  }
}

static void object_removed_callback(WpObjectManager *om, WpObject *object,
                                    gpointer user_data) {
  AudioState *data = user_data;

  if (WP_IS_NODE(object) && WP_NODE(object) == data->default_sink_node) {
    g_print("Default sink removed\n");

    g_clear_object(&data->default_sink_node);
    data->default_sink_id = 0;

    update_audio_ui(data, FALSE, -1);
  }
}

void start_audio_widget(GtkWidget *box, WpCore *core, WpObjectManager *om) {
  GtkWidget *audio_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *image = gtk_image_new_from_icon_name(ICON_MUTED);
  GtkWidget *label = gtk_label_new("...");

  AudioState *aw = calloc(1, sizeof(AudioState));
  aw->image = image;
  aw->label = label;

  gtk_box_append(GTK_BOX(audio_box), image);
  gtk_box_append(GTK_BOX(audio_box), label);

  gtk_box_append(GTK_BOX(box), audio_box);

  aw->mixer_api = wp_plugin_find(core, "mixer-api");
  if (aw->mixer_api) {
    g_object_ref(aw->mixer_api); // Keep a reference

    // Connect to mixer change signals
    g_signal_connect(aw->mixer_api, "changed", G_CALLBACK(on_mixer_changed),
                     aw);
    g_print("Connected to mixer API signals\n");
  } else {
    g_warning("Could not find mixer-api plugin");
  }

  g_print("Connected: %s\n", wp_core_is_connected(core) ? "TRUE" : "FALSE");

  g_signal_connect(om, "object-added", G_CALLBACK(object_added_callback), aw);
  g_signal_connect(om, "object-removed", G_CALLBACK(object_removed_callback),
                   aw);

  wp_core_install_object_manager(core, om);
  wp_object_manager_request_object_features(om, WP_TYPE_NODE,
                                            WP_OBJECT_FEATURES_ALL);
}
