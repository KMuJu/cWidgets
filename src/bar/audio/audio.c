#include "audio.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"
#include "wp/core.h"
#include "wp/node.h"
#include "wp/object.h"
#include "wp/plugin.h"
#include "wp/proxy-interfaces.h"
#include "wp/proxy.h"
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
  WpPlugin *def_nodes_api;
} AudioState;

static void update_audio_ui(AudioState *as, gboolean muted, int audio_level) {
  if (audio_level == -1) {
    g_message("No active audio\n");
    return;
  }
  if (audio_level > 999) {
    g_message("Audio level is too high: %d", audio_level);
    return;
  }
  char audio_str[4];
  snprintf(audio_str, sizeof(audio_str), "%d", audio_level);
  const char *icon_name;
  if (muted) {
    icon_name = ICON_MUTED;
  } else if (audio_level == 0) {
    icon_name = ICON_MUTED;
    muted = TRUE;
  } else if (audio_level < 33) {
    icon_name = ICON_LOW;
  } else if (audio_level < 66) {
    icon_name = ICON_MEDIUM;
  } else if (audio_level <= 100) {
    icon_name = ICON_HIGH;
  } else {
    icon_name = ICON_OVERAMPLIFIED;
  }

  gtk_label_set_text(GTK_LABEL(as->label), audio_str);
  gtk_widget_set_visible(as->label, !muted);
  gtk_image_set_from_icon_name(GTK_IMAGE(as->image), icon_name);
}

static void update_volume_info(AudioState *as, guint32 node_id) {
  GVariant *variant = NULL;
  gboolean mute = FALSE;
  gdouble volume = 1.0;

  g_signal_emit_by_name(as->mixer_api, "get-volume", node_id, &variant);
  if (!variant) {
    fprintf(stderr, "Node %d does not support volume\n", node_id);
    return;
  }
  g_variant_lookup(variant, "volume", "d", &volume);
  g_variant_lookup(variant, "mute", "b", &mute);
  g_clear_pointer(&variant, g_variant_unref);

  update_audio_ui(as, mute, (int)(volume * 100));
}

static void on_mixer_changed(WpPlugin *mixer_api, guint32 node_id,
                             gpointer user_data) {
  AudioState *as = (AudioState *)user_data;

  // Only update if it's our default sink
  if (node_id == wp_proxy_get_bound_id(WP_PROXY(as->default_sink_node))) {
    update_volume_info(as, node_id);
  }
}

static void on_def_nodes_changed(WpPlugin *def_node_api, guint32 node_id,
                                 gpointer user_data) {
  AudioState *as = (AudioState *)user_data;
  g_message("Default node changed: %d", node_id);

  const gchar *media_class = "Audio/Sink";

  if (as->def_nodes_api)
    g_signal_emit_by_name(as->def_nodes_api, "get-default-node", media_class,
                          &as->default_sink_node);
}

static void object_added_callback(WpObjectManager *self, gpointer object,
                                  gpointer user_data) {
  AudioState *as = user_data;
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
    as->default_sink_id = wp_proxy_get_bound_id(WP_PROXY(node));
    as->default_sink_node = g_object_ref(node); // Keep reference

    g_message("Default sink: %s (ID: %u)", node_name, as->default_sink_id);

    update_volume_info(as, as->default_sink_id);
  }
}

static void object_removed_callback(WpObjectManager *om, WpObject *object,
                                    gpointer user_data) {
  AudioState *data = user_data;

  if (WP_IS_NODE(object) && WP_NODE(object) == data->default_sink_node) {
    g_message("Default sink removed");

    g_clear_object(&data->default_sink_node);
    data->default_sink_id = 0;

    update_audio_ui(data, FALSE, -1);
  }
}

void start_audio_widget(GtkWidget *box, WpCore *core, WpObjectManager *om) {
  GtkWidget *audio_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GtkWidget *image = gtk_image_new_from_icon_name(ICON_MUTED);
  GtkWidget *label = gtk_label_new("...");

  AudioState *as = calloc(1, sizeof(AudioState));
  as->image = image;
  as->label = label;

  gtk_box_append(GTK_BOX(audio_box), image);
  gtk_box_append(GTK_BOX(audio_box), label);

  gtk_box_append(GTK_BOX(box), audio_box);

  as->mixer_api = wp_plugin_find(core, "mixer-api");
  if (as->mixer_api) {
    g_object_ref(as->mixer_api); // Keep a reference

    // Connect to mixer change signals
    g_signal_connect(as->mixer_api, "changed", G_CALLBACK(on_mixer_changed),
                     as);
  } else {
    g_warning("Could not find mixer-api plugin");
  }
  as->def_nodes_api = wp_plugin_find(core, "default-nodes-api");
  if (as->def_nodes_api) {
    g_object_ref(as->def_nodes_api); // Keep a reference

    // Connect to def_node change signals
    g_signal_connect(as->def_nodes_api, "changed",
                     G_CALLBACK(on_def_nodes_changed), as);
  } else {
    g_warning("Could not find def_node-api plugin");
  }

  g_signal_connect(om, "object-added", G_CALLBACK(object_added_callback), as);
  g_signal_connect(om, "object-removed", G_CALLBACK(object_removed_callback),
                   as);

  wp_core_install_object_manager(core, om);
  wp_object_manager_request_object_features(om, WP_TYPE_NODE,
                                            WP_OBJECT_FEATURES_ALL);
}
