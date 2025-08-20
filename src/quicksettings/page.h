#ifndef PAGE_H
#define PAGE_H
#include <gtk/gtk.h>

typedef struct {
  GtkWidget *box;
  GtkWidget *active_name;
  GtkWidget *image;
  GtkWidget *toggle_btn;
  GtkWidget *arrow_btn;
  GtkWidget *arrow_image;
  GtkWidget *revealer;
  GtkWidget *revealer_box;
  GtkWidget *revealer_scrolled;
  void *page_data;
} PageButton;

PageButton *create_page_button(const gchar *title_str, const gchar *icon_name);

#endif // !PAGE_H
