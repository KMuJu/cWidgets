#ifndef LOG_H
#define LOG_H

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static FILE *log_file = NULL;

#define LOG(fmt, ...)                                                          \
  do {                                                                         \
    if (log_file) {                                                            \
      time_t now = time(NULL);                                                 \
      struct tm *t = localtime(&now);                                          \
      fprintf(log_file, "[%02d:%02d:%02d] " fmt "\n", t->tm_hour, t->tm_min,   \
              t->tm_sec, ##__VA_ARGS__);                                       \
      fflush(log_file);                                                        \
    }                                                                          \
  } while (0)

static inline void init_logger(const char *filename) {
  log_file = fopen(filename, "a");
  if (!log_file) {
    perror("Failed to open log file");
    exit(EXIT_FAILURE);
  }
  LOG("");
  LOG("========NEW LOGGER========");
}

static inline void close_logger(void) {
  if (log_file) {
    fclose(log_file);
    log_file = NULL;
  }
}

static inline void gtk_log_handler(const gchar *domain, GLogLevelFlags level,
                                   const gchar *message, gpointer user_data) {
  const char *level_str = "INFO";
  if (level & G_LOG_LEVEL_WARNING)
    level_str = "WARNING";
  else if (level & G_LOG_LEVEL_ERROR)
    level_str = "ERROR";
  else if (level & G_LOG_LEVEL_CRITICAL)
    level_str = "CRITICAL";
  else if (level & G_LOG_LEVEL_DEBUG)
    level_str = "DEBUG";

  LOG("[%s] %s: %s", domain ? domain : "APP", level_str, message);
}

static inline void redirect_glib_logs(void) {
  g_log_set_default_handler(gtk_log_handler, NULL);
}

#endif // !LOG_H
