#include "battery.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"
#include <dirent.h>
#include <stdint.h>
#include <string.h>

struct BatteryWidgets {
  GtkWidget *label;
  GtkWidget *image;
};

int read_value(const char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp)
    return -1;

  int val;
  if (fscanf(fp, "%d", &val) != 1) {
    fclose(fp);
    return -1;
  }

  fclose(fp);
  return val;
}

int get_battery_level(void) {
  const char *power_path = "/sys/class/power_supply";
  DIR *power_dir = opendir(power_path);
  if (!power_dir)
    return -1;

  struct dirent *folder;
  int64_t power_now = 0, power_full = 0;
  while ((folder = readdir(power_dir)) != NULL) {
    if (strncmp("BAT", folder->d_name, 3) != 0)
      continue;

    char path_now[512], path_full[512];
    int now = -1, full = -1;

    snprintf(path_now, 512, "%s/%s/energy_now", power_path, folder->d_name);
    snprintf(path_full, 512, "%s/%s/energy_full", power_path, folder->d_name);

    now = read_value(path_now);
    full = read_value(path_full);

    if (now < 0 || full <= 0) {
      snprintf(path_now, 512, "%s/%s/charge_now", power_path, folder->d_name);
      snprintf(path_full, 512, "%s/%s/charge_full", power_path, folder->d_name);

      now = read_value(path_now);
      full = read_value(path_full);
    }

    if (now < 0 || full <= 0)
      continue;

    power_now += now;
    power_full += full;
  }

  int percentage = (power_now * 100) / power_full;
  if (percentage < 0)
    percentage = 0;
  if (percentage > 100)
    percentage = 100;

  return percentage;
}

gboolean battery_refresh(gpointer data) {
  struct BatteryWidgets *bw = data;
  GtkWidget *label = bw->label;
  GtkWidget *image = bw->image;

  const int level = get_battery_level();
  int last_digit = level % 10;
  int level_icon = (level / 10) * 10;
  if (last_digit >= 5)
    level_icon += 10;

  char icon_name[26];
  snprintf(icon_name, 26, "battery-level-%d-symbolic", level_icon);

  char percent_str[4];
  snprintf(percent_str, 4, "%d%%", level);

  gtk_label_set_label(GTK_LABEL(label), percent_str);
  gtk_image_set_from_icon_name(GTK_IMAGE(image), icon_name);

  return TRUE;
}

void start_battery_widget(GtkWidget *box) {
  GtkWidget *image = gtk_image_new_from_icon_name("battery-symbolic");
  GtkWidget *label = gtk_label_new("...");

  gtk_box_append(GTK_BOX(box), image);
  gtk_box_append(GTK_BOX(box), label);

  struct BatteryWidgets *bw = calloc(1, sizeof(struct BatteryWidgets));
  bw->label = label;
  bw->image = image;
  battery_refresh(bw);
}
