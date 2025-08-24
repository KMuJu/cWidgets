#include "audio.h"
#include <glib-object.h>
#include <glib.h>
#include <glibconfig.h>
#include <gtk/gtk.h>
#include <gtk/gtkshortcut.h>
#include <spa/pod/iter.h>
#include <wp/core.h>
#include <wp/node.h>
#include <wp/object.h>
#include <wp/plugin.h>
#include <wp/proxy-interfaces.h>
#include <wp/proxy.h>

static const char ICON_MUTED[] = "audio-volume-muted-symbolic";
static const char ICON_LOW[] = "audio-volume-low-symbolic";
static const char ICON_MEDIUM[] = "audio-volume-medium-symbolic";
static const char ICON_HIGH[] = "audio-volume-high-symbolic";
static const char ICON_OVERAMPLIFIED[] = "audio-volume-overamplified-symbolic";

static const gchar *sink_media_class = "Audio/Sink";

typedef struct {
  GtkWidget *image;
  GtkWidget *label;
  guint32 default_sink_id;
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

  if (node_id == as->default_sink_id) {
    update_volume_info(as, node_id);
  }
}

static void on_def_nodes_changed(WpPlugin *def_node_api, gpointer user_data) {
  AudioState *as = (AudioState *)user_data;

  if (def_node_api) {
    g_signal_emit_by_name(def_node_api, "get-default-node", sink_media_class,
                          &as->default_sink_id);
    update_volume_info(as, as->default_sink_id);
    g_message("New default sink id: %u", as->default_sink_id);
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
    g_signal_connect(as->mixer_api, "changed", G_CALLBACK(on_mixer_changed),
                     as);
  } else {
    g_warning("Could not find mixer-api plugin");
  }
  as->def_nodes_api = wp_plugin_find(core, "default-nodes-api");
  if (as->def_nodes_api) {
    g_signal_connect(as->def_nodes_api, "changed",
                     G_CALLBACK(on_def_nodes_changed), as);

    on_def_nodes_changed(as->def_nodes_api, as);
  } else {
    g_warning("Could not find def_node-api plugin");
  }

  wp_core_install_object_manager(core, om);
  wp_object_manager_request_object_features(om, WP_TYPE_NODE,
                                            WP_OBJECT_FEATURES_ALL);
}
