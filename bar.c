#define _GNU_SOURCE
#include "bar.h"
#include "appicons.h"
#include "config.h"
#include <X11/Xutil.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>

// Mutex for thread-safe bar updates
static pthread_mutex_t bar_mutex = PTHREAD_MUTEX_INITIALIZER;

static int bar_update_pipe[2] = {-1, -1};

// Cached bar state passed in from main
static Client *bar_clients;
static int bar_max_windows;
static int *bar_current_client_ptr;
static Display *bar_dpy;

const char *get_client_icon(Display *dpy, Window w) {
  XClassHint ch;
  const char *icon = get_default_icon();

  if (XGetClassHint(dpy, w, &ch)) {
    if (ch.res_class) {
      icon = get_icon_by_name(ch.res_class);
    }
    if (ch.res_name)
      XFree(ch.res_name);
    if (ch.res_class)
      XFree(ch.res_class);
  }
  return icon;
}

// Bar Module Renderers
static char cached_windows[1024] = {0};

static int render_windows(Client *clients, int max_windows, int current_client,
                          Display *dpy, char *buf, int size) {
  int offset = 0;
  int count = 0;

  for (int i = 0; i < max_windows; i++) {
    if (clients[i].active) {
      count++;
      const char *icon = get_client_icon(dpy, clients[i].win);

      if (i == current_client) {
        offset += snprintf(buf + offset, size - offset,
                           " %%{F%s}%%{B%s} %s %%{B-}%%{F-} ",
                           BAR_COLOR_ACTIVE_FG, BAR_COLOR_ACTIVE_BG, icon);
      } else {
        offset += snprintf(buf + offset, size - offset, " %%{F%s} %s %%{F-} ",
                           BAR_COLOR_INACTIVE_FG, icon);
      }
    }
  }
  if (count == 0) {
    offset += snprintf(buf + offset, size - offset, " No windows ");
  }
  return offset;
}

#if BAR_SHOW_TIME
// Render time into buf, return bytes written
static int render_time(char *buf, int size) {
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  char time_str[64];

  strftime(time_str, sizeof(time_str), BAR_TIME_FORMAT, tm_info);
  return snprintf(buf, size, " %s ", time_str);
}
#endif

#if BAR_SHOW_VOLUME
// Read an integer from a shell command, return -1 on failure
static int read_cmd_int(const char *cmd) {
  FILE *f = popen(cmd, "r");
  if (!f)
    return -1;

  int val = -1;
  if (fscanf(f, "%d", &val) != 1)
    val = -1;
  pclose(f);
  return val;
}

// Check if audio is muted
static int is_volume_muted(void) {
  FILE *f = popen(BAR_VOLUME_MUTE_CMD, "r");
  if (!f)
    return 0;

  char buf[16] = {0};
  if (fgets(buf, sizeof(buf), f) == NULL) {
    pclose(f);
    return 0;
  }
  pclose(f);
  buf[strcspn(buf, "\n")] = '\0';
  return (strcmp(buf, "yes") == 0);
}

// Get the appropriate volume icon
static const char *get_volume_icon(int percent, int muted) {
  if (muted)
    return BAR_VOLUME_ICON_MUTE;
  if (percent >= 66)
    return BAR_VOLUME_ICON_HIGH;
  if (percent >= 33)
    return BAR_VOLUME_ICON_MED;
  return BAR_VOLUME_ICON_LOW;
}

// Render volume into buf, return bytes written
static int render_volume(char *buf, int size) {
  int percent = read_cmd_int(BAR_VOLUME_CMD);
  if (percent < 0)
    return snprintf(buf, size, " %s N/A ", BAR_VOLUME_ICON_MUTE);

  int muted = is_volume_muted();
  const char *icon = get_volume_icon(percent, muted);

  if (muted) {
    return snprintf(buf, size, " %%{F#888888}%s %d%%%%%%{F-} ", icon, percent);
  }
  return snprintf(buf, size, " %s %d%% ", icon, percent);
}
#endif

#if BAR_SHOW_BATTERY
// Read battery percentage from sysfs, return -1 on failure
static int read_battery_percent(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/capacity", BAR_BATTERY_PATH);

  FILE *f = fopen(path, "r");
  if (!f)
    return -1;

  int percent = -1;
  if (fscanf(f, "%d", &percent) != 1)
    percent = -1;
  fclose(f);
  return percent;
}

// Check if battery is charging
static int is_battery_charging(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/status", BAR_BATTERY_PATH);

  FILE *f = fopen(path, "r");
  if (!f)
    return 0;

  char status[32] = {0};
  if (fgets(status, sizeof(status), f) == NULL) {
    fclose(f);
    return 0;
  }
  fclose(f);

  // Remove trailing newline
  status[strcspn(status, "\n")] = '\0';
  return (strcmp(status, "Charging") == 0 || strcmp(status, "Full") == 0);
}

// Get the appropriate battery icon based on level
static const char *get_battery_icon(int percent, int charging) {
  if (charging)
    return BAR_BATTERY_ICON_CHARGING;
  if (percent >= 75)
    return BAR_BATTERY_ICON_FULL;
  if (percent >= 50)
    return BAR_BATTERY_ICON_75;
  if (percent >= 25)
    return BAR_BATTERY_ICON_50;
  if (percent >= 10)
    return BAR_BATTERY_ICON_25;
  return BAR_BATTERY_ICON_EMPTY;
}

// Render battery into buf, return bytes written
static int render_battery(char *buf, int size) {
  int percent = read_battery_percent();
  if (percent < 0)
    return snprintf(buf, size, " 󱃍 N/A ");

  int charging = is_battery_charging();
  const char *icon = get_battery_icon(percent, charging);

  // Color: red if <=10%, yellow if <=25%, default otherwise
  if (percent <= 10) {
    return snprintf(buf, size, " %%{F#ff5555}%s %d%%%%%%{F-} ", icon, percent);
  } else if (percent <= 25) {
    return snprintf(buf, size, " %%{F#f1fa8c}%s %d%%%%%%{F-} ", icon, percent);
  }
  return snprintf(buf, size, " %s %d%% ", icon, percent);
}
#endif

// Bar Output Composer
static void compose_bar(Client *clients, int max_windows, int current_client,
                        Display *dpy, int refresh_windows) {
  pthread_mutex_lock(&bar_mutex);

  if (refresh_windows) {
    render_windows(clients, max_windows, current_client, dpy, cached_windows,
                   (int)sizeof(cached_windows));
  }

  char left[1024] = {0};
  char center[1024] = {0};
  char right[1024] = {0};

  int l_off = 0, c_off = 0, r_off = 0;

//Windows module (always from cache)
#if BAR_SHOW_WINDOWS
#if BAR_WINDOWS_POSITION == 'l'
  l_off +=
      snprintf(left + l_off, (int)sizeof(left) - l_off, "%s", cached_windows);
#elif BAR_WINDOWS_POSITION == 'c'
  c_off += snprintf(center + c_off, (int)sizeof(center) - c_off, "%s",
                    cached_windows);
#elif BAR_WINDOWS_POSITION == 'r'
  r_off +=
      snprintf(right + r_off, (int)sizeof(right) - r_off, "%s", cached_windows);
#endif
#endif

//Time module
#if BAR_SHOW_TIME
#if BAR_TIME_POSITION == 'l'
  l_off += render_time(left + l_off, (int)sizeof(left) - l_off);
#elif BAR_TIME_POSITION == 'c'
  c_off += render_time(center + c_off, (int)sizeof(center) - c_off);
#elif BAR_TIME_POSITION == 'r'
  r_off += render_time(right + r_off, (int)sizeof(right) - r_off);
#endif
#endif

//Volume module
#if BAR_SHOW_VOLUME
#if BAR_VOLUME_POSITION == 'l'
  l_off += render_volume(left + l_off, (int)sizeof(left) - l_off);
#elif BAR_VOLUME_POSITION == 'c'
  c_off += render_volume(center + c_off, (int)sizeof(center) - c_off);
#elif BAR_VOLUME_POSITION == 'r'
  r_off += render_volume(right + r_off, (int)sizeof(right) - r_off);
#endif
#endif

//Battery module
#if BAR_SHOW_BATTERY
#if BAR_BATTERY_POSITION == 'l'
  l_off += render_battery(left + l_off, (int)sizeof(left) - l_off);
#elif BAR_BATTERY_POSITION == 'c'
  c_off += render_battery(center + c_off, (int)sizeof(center) - c_off);
#elif BAR_BATTERY_POSITION == 'r'
  r_off += render_battery(right + r_off, (int)sizeof(right) - r_off);
#endif
#endif

  // Suppress unused variable warnings when modules are disabled
  (void)l_off;
  (void)c_off;
  (void)r_off;

  // Compose final lemonbar output: %{l}...%{c}...%{r}...
  printf("%%{l}%s%%{c}%s%%{r}%s\n", left, center, right);
  fflush(stdout);

  pthread_mutex_unlock(&bar_mutex);
}

// Called from the main thread when windows change
void update_bar(Client *clients, int max_windows, int current_client,
                Display *dpy) {
#if BAR_ENABLED
  compose_bar(clients, max_windows, current_client, dpy, 1);
#else
  (void)clients;
  (void)max_windows;
  (void)current_client;
  (void)dpy;
#endif
}

#if BAR_ENABLED
// Periodic Bar Refresh Thread
static void *bar_refresh_thread(void *arg) {
  (void)arg;
  struct pollfd pfd;
  pfd.fd = bar_update_pipe[0];
  pfd.events = POLLIN;

  while (1) {
    int ret = poll(&pfd, 1, BAR_UPDATE_INTERVAL * 1000);
    if (ret > 0) {
      char dummy;
      // Drain the non-blocking pipe
      while (read(bar_update_pipe[0], &dummy, 1) > 0);
    }
    compose_bar(bar_clients, bar_max_windows, *bar_current_client_ptr, bar_dpy,
                0);
  }
  return NULL;
}
#endif

void bar_start_refresh_thread(Client *clients, int max_windows,
                              int *current_client_ptr, Display *dpy) {
#if BAR_ENABLED
  bar_clients = clients;
  bar_max_windows = max_windows;
  bar_current_client_ptr = current_client_ptr;
  bar_dpy = dpy;

  if (pipe(bar_update_pipe) == 0) {
    // Make read end non-blocking so we can safely drain it in a loop
    int flags = fcntl(bar_update_pipe[0], F_GETFL, 0);
    if (flags != -1) {
      fcntl(bar_update_pipe[0], F_SETFL, flags | O_NONBLOCK);
    }
  }

  pthread_t refresh_tid;
  pthread_create(&refresh_tid, NULL, bar_refresh_thread, NULL);
  pthread_detach(refresh_tid);
#else
  (void)clients;
  (void)max_windows;
  (void)current_client_ptr;
  (void)dpy;
#endif
}

void bar_trigger_update(void) {
#if BAR_ENABLED
  if (bar_update_pipe[1] != -1) {
    char dummy = 1;
    ssize_t n = write(bar_update_pipe[1], &dummy, 1);
    (void)n;
  }
#endif
}
