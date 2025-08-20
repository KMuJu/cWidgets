#include "page.h"
#include "util.h"

static void on_toggle_revealer(GtkButton *self, gpointer data) {
  PageButton *pb = data;
  gboolean revealing =
      gtk_revealer_get_reveal_child(GTK_REVEALER(pb->revealer));
  gtk_revealer_set_reveal_child(GTK_REVEALER(pb->revealer), !revealing);
  gtk_image_set_from_icon_name(GTK_IMAGE(pb->arrow_image),
                               !revealing
                                   ? "go-up-symbolic" // Revealer will be open
                                   : "go-down-symbolic");
}

PageButton *create_page_button(const gchar *title_str, const gchar *icon_name) {
  PageButton *pb = g_new0(PageButton, 1);
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_add_css_class(box, "page");
  // Box that contains the button
  GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class(button_box, "page-button");

  // Toggle button with title, active ap and icon
  GtkWidget *toggle_btn = gtk_button_new();
  gtk_widget_add_css_class(toggle_btn, "toggle");
  gtk_widget_set_cursor(toggle_btn, get_pointer_cursor());
  gtk_widget_set_hexpand(toggle_btn, TRUE);
  GtkWidget *toggle_button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *toggle_button_image = gtk_image_new_from_icon_name(icon_name);
  GtkWidget *toggle_button_info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_hexpand(toggle_button_info_box, TRUE);
  GtkWidget *title = gtk_label_new(title_str);
  gtk_widget_add_css_class(title, "title");
  GtkWidget *ap_name = gtk_label_new("ITBA");
  gtk_widget_add_css_class(ap_name, "name");
  gtk_box_append(GTK_BOX(toggle_button_info_box), title);
  gtk_box_append(GTK_BOX(toggle_button_info_box), ap_name);

  gtk_box_append(GTK_BOX(toggle_button_box), toggle_button_image);
  gtk_box_append(GTK_BOX(toggle_button_box), toggle_button_info_box);

  gtk_button_set_child(GTK_BUTTON(toggle_btn), toggle_button_box);

  // Button with arrow to open revealer
  GtkWidget *arrow_btn = gtk_button_new_from_icon_name("go-down-symbolic");
  GtkWidget *arrow_image = gtk_button_get_child(GTK_BUTTON(arrow_btn));
  gtk_widget_set_cursor(arrow_btn, get_pointer_cursor());
  gtk_widget_add_css_class(arrow_btn, "open");

  gtk_box_append(GTK_BOX(button_box), toggle_btn);
  gtk_box_append(GTK_BOX(button_box), arrow_btn);

  gtk_box_append(GTK_BOX(box), button_box);

  // Revealer with access point entries
  GtkWidget *revealer = gtk_revealer_new();
  gtk_revealer_set_transition_duration(GTK_REVEALER(revealer), 500);
  GtkWidget *revealer_scrolled = gtk_scrolled_window_new();
  GtkWidget *revealer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(revealer_scrolled),
                                revealer_box);
  gtk_revealer_set_child(GTK_REVEALER(revealer), revealer_scrolled);
  gtk_box_append(GTK_BOX(box), revealer);

  pb->box = box;
  pb->arrow_btn = arrow_btn;
  pb->arrow_image = arrow_image;
  pb->toggle_btn = toggle_btn;
  pb->active_name = ap_name;
  pb->image = toggle_button_image;
  pb->revealer = revealer;
  pb->revealer_box = revealer_box;
  pb->revealer_scrolled = revealer_scrolled;

  g_signal_connect(pb->arrow_btn, "clicked", G_CALLBACK(on_toggle_revealer),
                   pb);

  return pb;
}
