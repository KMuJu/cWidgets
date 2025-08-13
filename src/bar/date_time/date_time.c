#include "date_time.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"

static const gchar FORMAT_MINUTES[] = "%H:%M";
static const gchar FORMAT_SECONDS[] = "%H:%M:%S";
static const gchar *current_format = FORMAT_MINUTES;

typedef struct {
  GtkWidget *date_label;
  GtkWidget *time_label;
} DateTimeWidgets;

static gboolean on_timeout(gpointer data) {
  DateTimeWidgets *dtw = data;
  GDateTime *now = g_date_time_new_now_local();
  gchar *time_formatted = g_date_time_format(now, current_format);
  gchar *date_formatted = g_date_time_format(now, "%d.%m.%y");

  gtk_label_set_text(GTK_LABEL(dtw->time_label), time_formatted);
  gtk_label_set_text(GTK_LABEL(dtw->date_label), date_formatted);

  return G_SOURCE_CONTINUE;
}

void start_date_time_widget(GtkWidget *box) {
  DateTimeWidgets *dtw = g_new0(DateTimeWidgets, 1);
  dtw->date_label = gtk_label_new("DATE");
  dtw->time_label = gtk_label_new("TIME");

  gtk_box_append(GTK_BOX(box), dtw->time_label);
  gtk_box_append(GTK_BOX(box), dtw->date_label);

  g_timeout_add(1000, on_timeout, dtw);
  on_timeout(dtw);
}
