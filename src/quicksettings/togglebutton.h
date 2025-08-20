#ifndef TOGGLEBUTTON_H
#define TOGGLEBUTTON_H

#include <gtk/gtk.h>

typedef struct {
  const gchar *icon_on;
  const gchar *icon_off;
  gchar *cmd;
} ToggleButtonProps;

GtkWidget *togglebutton(ToggleButtonProps *props, gboolean initial);
GtkWidget *notification_button(void);
GtkWidget *colorpicker_button(void);

#endif // !TOGGLEBUTTON_H
