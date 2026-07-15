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
#include <signal.h>
#include <sys/wait.h>

extern int get_scaled_bar_height(Display *d);
extern int get_scaled_font_size(Display *d);

static pthread_mutex_t bar_mutex = PTHREAD_MUTEX_INITIALIZER;

static int bar_update_pipe[2] = {-1, -1};

static Client *bar_clients;
static int bar_max_windows;
static int *bar_current_client_ptr;
static Display *bar_dpy;

int lemonbar_pipe_fd = -1;
pid_t lemonbar_pid = -1;

void spawn_lemonbar(Display *d) {
  if (lemonbar_pid > 0) return;

  int pipefd[2];
  if (pipe(pipefd) != 0) return;

  lemonbar_pid = fork();
  if (lemonbar_pid == 0) {
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);

    char geom_str[64];
    snprintf(geom_str, sizeof(geom_str), "x%d", get_scaled_bar_height(d));
    char font_str[256];
    snprintf(font_str, sizeof(font_str), "%s:size=%d", config.bar_font_name, config.bar_font_size);

    execlp("lemonbar", "lemonbar", "-p", "-g", geom_str, "-f", font_str, NULL);
    _exit(1);
  } else if (lemonbar_pid > 0) {
    close(pipefd[0]);
    lemonbar_pipe_fd = pipefd[1];

    int flags = fcntl(lemonbar_pipe_fd, F_GETFL, 0);
    if (flags != -1) {
      fcntl(lemonbar_pipe_fd, F_SETFL, flags | O_NONBLOCK);
    }
  }
}

void kill_lemonbar(void) {
  if (lemonbar_pid > 0) {
    kill(lemonbar_pid, SIGTERM);
    waitpid(lemonbar_pid, NULL, 0);
    lemonbar_pid = -1;
  }
  if (lemonbar_pipe_fd != -1) {
    close(lemonbar_pipe_fd);
    lemonbar_pipe_fd = -1;
  }
}

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
                           config.bar_color_active_fg, config.bar_color_active_bg, icon);
      } else {
        offset += snprintf(buf + offset, size - offset, " %%{F%s} %s %%{F-} ",
                           config.bar_color_inactive_fg, icon);
      }
    }
  }
  if (count == 0) {
    offset += snprintf(buf + offset, size - offset, " No windows ");
  }
  return offset;
}

static int render_time(char *buf, int size) {
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  char time_str[64];

  strftime(time_str, sizeof(time_str), config.bar_time_format, tm_info);
  return snprintf(buf, size, " %s ", time_str);
}

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

static int is_volume_muted(void) {
  FILE *f = popen(config.bar_volume_mute_cmd, "r");
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

static const char *get_volume_icon(int percent, int muted) {
  if (muted)
    return config.bar_volume_icon_mute;
  if (percent >= 66)
    return config.bar_volume_icon_high;
  if (percent >= 33)
    return config.bar_volume_icon_med;
  return config.bar_volume_icon_low;
}

static int render_volume(char *buf, int size) {
  int percent = read_cmd_int(config.bar_volume_cmd);
  if (percent < 0)
    return snprintf(buf, size, " %s N/A ", config.bar_volume_icon_mute);

  int muted = is_volume_muted();
  const char *icon = get_volume_icon(percent, muted);

  if (muted) {
    return snprintf(buf, size, " %%{F#888888}%s %d%%%%%%{F-} ", icon, percent);
  }
  return snprintf(buf, size, " %s %d%% ", icon, percent);
}

static int read_battery_percent(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/capacity", config.bar_battery_path);

  FILE *f = fopen(path, "r");
  if (!f)
    return -1;

  int percent = -1;
  if (fscanf(f, "%d", &percent) != 1)
    percent = -1;
  fclose(f);
  return percent;
}

static int is_battery_charging(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/status", config.bar_battery_path);

  FILE *f = fopen(path, "r");
  if (!f)
    return 0;

  char status[32] = {0};
  if (fgets(status, sizeof(status), f) == NULL) {
    fclose(f);
    return 0;
  }
  fclose(f);

  status[strcspn(status, "\n")] = '\0';
  return (strcmp(status, "Charging") == 0 || strcmp(status, "Full") == 0);
}

static const char *get_battery_icon(int percent, int charging) {
  if (charging)
    return config.bar_battery_icon_charging;
  if (percent >= 75)
    return config.bar_battery_icon_full;
  if (percent >= 50)
    return config.bar_battery_icon_75;
  if (percent >= 25)
    return config.bar_battery_icon_50;
  if (percent >= 10)
    return config.bar_battery_icon_25;
  return config.bar_battery_icon_empty;
}

static int render_battery(char *buf, int size) {
  int percent = read_battery_percent();
  if (percent < 0)
    return snprintf(buf, size, " 󱃍 N/A ");

  int charging = is_battery_charging();
  const char *icon = get_battery_icon(percent, charging);

  if (percent <= 10) {
    return snprintf(buf, size, " %%{F#ff5555}%s %d%%%%%%{F-} ", icon, percent);
  } else if (percent <= 25) {
    return snprintf(buf, size, " %%{F#f1fa8c}%s %d%%%%%%{F-} ", icon, percent);
  }
  return snprintf(buf, size, " %s %d%% ", icon, percent);
}

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

  // Windows module
  if (config.bar_show_windows) {
    if (config.bar_windows_position == 'l') {
      l_off += snprintf(left + l_off, (int)sizeof(left) - l_off, "%s", cached_windows);
    } else if (config.bar_windows_position == 'c') {
      c_off += snprintf(center + c_off, (int)sizeof(center) - c_off, "%s", cached_windows);
    } else if (config.bar_windows_position == 'r') {
      r_off += snprintf(right + r_off, (int)sizeof(right) - r_off, "%s", cached_windows);
    }
  }

  // Time module
  if (config.bar_show_time) {
    if (config.bar_time_position == 'l') {
      l_off += render_time(left + l_off, (int)sizeof(left) - l_off);
    } else if (config.bar_time_position == 'c') {
      c_off += render_time(center + c_off, (int)sizeof(center) - c_off);
    } else if (config.bar_time_position == 'r') {
      r_off += render_time(right + r_off, (int)sizeof(right) - r_off);
    }
  }

  // Volume module
  if (config.bar_show_volume) {
    if (config.bar_volume_position == 'l') {
      l_off += render_volume(left + l_off, (int)sizeof(left) - l_off);
    } else if (config.bar_volume_position == 'c') {
      c_off += render_volume(center + c_off, (int)sizeof(center) - c_off);
    } else if (config.bar_volume_position == 'r') {
      r_off += render_volume(right + r_off, (int)sizeof(right) - r_off);
    }
  }

  // Battery module
  if (config.bar_show_battery) {
    if (config.bar_battery_position == 'l') {
      l_off += render_battery(left + l_off, (int)sizeof(left) - l_off);
    } else if (config.bar_battery_position == 'c') {
      c_off += render_battery(center + c_off, (int)sizeof(center) - c_off);
    } else if (config.bar_battery_position == 'r') {
      r_off += render_battery(right + r_off, (int)sizeof(right) - r_off);
    }
  }

  (void)l_off;
  (void)c_off;
  (void)r_off;

  if (lemonbar_pipe_fd != -1) {
    char bar_str[2048];
    int len = snprintf(bar_str, sizeof(bar_str), "%%{l}%s%%{c}%s%%{r}%s\n", left, center, right);
    if (len > 0) {
      ssize_t n = write(lemonbar_pipe_fd, bar_str, len);
      (void)n;
    }
  }

  pthread_mutex_unlock(&bar_mutex);
}

void update_bar(Client *clients, int max_windows, int current_client,
                Display *dpy) {
  if (runtime_bar_enabled) {
    compose_bar(clients, max_windows, current_client, dpy, 1);
  } else {
    (void)clients;
    (void)max_windows;
    (void)current_client;
    (void)dpy;
  }
}

static void *bar_refresh_thread(void *arg) {
  (void)arg;
  struct pollfd pfd;
  pfd.fd = bar_update_pipe[0];
  pfd.events = POLLIN;

  while (1) {
    int timeout_ms = config.bar_update_interval * 1000;
    int ret = poll(&pfd, 1, timeout_ms);
    if (ret > 0) {
      char dummy;
      while (read(bar_update_pipe[0], &dummy, 1) > 0);
    }
    compose_bar(bar_clients, bar_max_windows, *bar_current_client_ptr, bar_dpy, 0);
  }
  return NULL;
}

void bar_start_refresh_thread(Client *clients, int max_windows,
                              int *current_client_ptr, Display *dpy) {
  static int thread_started = 0;
  if (!runtime_bar_enabled) {
    return;
  }
  if (thread_started) {
    return;
  }
  bar_clients = clients;
  bar_max_windows = max_windows;
  bar_current_client_ptr = current_client_ptr;
  bar_dpy = dpy;

  if (pipe(bar_update_pipe) == 0) {
    int flags = fcntl(bar_update_pipe[0], F_GETFL, 0);
    if (flags != -1) {
      fcntl(bar_update_pipe[0], F_SETFL, flags | O_NONBLOCK);
    }
  }

  pthread_t refresh_tid;
  pthread_create(&refresh_tid, NULL, bar_refresh_thread, NULL);
  pthread_detach(refresh_tid);
  thread_started = 1;
}

void bar_trigger_update(void) {
  if (runtime_bar_enabled && bar_update_pipe[1] != -1) {
    char dummy = 1;
    ssize_t n = write(bar_update_pipe[1], &dummy, 1);
    (void)n;
  }
}
