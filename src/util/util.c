#include "util.h"
#include <gdk/gdk.h>

static GdkCursor *pointer_cursor = NULL;

GdkCursor *get_pointer_cursor(void) {
  if (pointer_cursor)
    return pointer_cursor;

  pointer_cursor = gdk_cursor_new_from_name("pointer", NULL);
  return pointer_cursor;
}

void cursor_util_cleanup(void) { g_clear_object(&pointer_cursor); }
