#define _GNU_SOURCE
#include "appicons.h"
#include "bar.h"
#include "keys.h"
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#include "config.h"

int runtime_bar_enabled = 0;
int lemonbar_height = 0;

Display *dpy;
Window root;
Client *clients = NULL;
int current_client = -1;
int screen_width, screen_height;
Atom net_wm_window_type, net_wm_window_type_dock;
Atom net_supported, net_supporting_wm_check, net_client_list, net_active_window, net_wm_name;
Window wm_check_win;

int x_error_handler(Display *d, XErrorEvent *e) {
  (void)d;
  (void)e;
  return 0;
}

void handle_sigusr1(int sig) {
  (void)sig;
  bar_trigger_update();
}

int is_command_in_path(const char *cmd) {
  char *path = getenv("PATH");
  if (!path) return 0;
  char *path_copy = strdup(path);
  if (!path_copy) return 0;
  char *dir = strtok(path_copy, ":");
  int found = 0;
  while (dir) {
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);
    if (access(full_path, X_OK) == 0) {
      found = 1;
      break;
    }
    dir = strtok(NULL, ":");
  }
  free(path_copy);
  return found;
}

void update_client_list() {
  Window wins[config.max_windows];
  int count = 0;
  for (int i = 0; i < config.max_windows; i++) {
    if (clients[i].active) {
      wins[count++] = clients[i].win;
    }
  }
  XChangeProperty(dpy, root, net_client_list, XA_WINDOW, 32, PropModeReplace,
                  (unsigned char *)wins, count);
}

void update_active_window() {
  Window w = (current_client >= 0 && clients[current_client].active)
                 ? clients[current_client].win
                 : None;
  XChangeProperty(dpy, root, net_active_window, XA_WINDOW, 32, PropModeReplace,
                  (unsigned char *)&w, 1);
}

double get_dpi(Display *d) {
  int screen = DefaultScreen(d);
  double height_mm = DisplayHeightMM(d, screen);
  if (height_mm > 0) {
    return (DisplayHeight(d, screen) * 25.4) / height_mm;
  }
  return 96.0;
}

int get_scaled_bar_height(Display *d) {
  double dpi = get_dpi(d);
  int height = (int)(config.bar_height * (dpi / 96.0) + 0.5);
  if (height < 10) height = 10;
  return height;
}

int get_scaled_font_size(Display *d) {
  double dpi = get_dpi(d);
  int font_size = (int)(config.bar_font_size * (dpi / 96.0) + 0.5);
  if (font_size < 4) font_size = 4;
  return font_size;
}

void spawn(const char *cmd) {
  pid_t pid = fork();
  if (pid == 0) {
    if (fork() == 0) {
      setsid();
      execl("/bin/sh", "/bin/sh", "-c", cmd, NULL);
      _exit(0);
    }
    _exit(0);
  }
  if (pid > 0) {
    waitpid(pid, NULL, 0);
  }
}

void reload_config() {
  config_load();
  if (config.max_windows > 128) {
    config.max_windows = 128;
  }

  if (config.bar_enabled && is_command_in_path("lemonbar")) {
    runtime_bar_enabled = 1;
    lemonbar_height = get_scaled_bar_height(dpy);
    spawn_lemonbar(dpy);
  } else {
    runtime_bar_enabled = 0;
    lemonbar_height = 0;
    kill_lemonbar();
  }

  XUngrabKey(dpy, AnyKey, AnyModifier, root);
  keys_grab(dpy, root);

  for (int i = 0; i < config.max_windows; i++) {
    if (clients[i].active) {
      XMoveResizeWindow(dpy, clients[i].win, 0, lemonbar_height, screen_width,
                        screen_height - lemonbar_height);
    }
  }

  bar_trigger_update();
  printf("monowm: configuration reloaded\n");
}

void toggle_bar() {
  config.bar_enabled = !config.bar_enabled;
  if (config.bar_enabled && is_command_in_path("lemonbar")) {
    runtime_bar_enabled = 1;
    lemonbar_height = get_scaled_bar_height(dpy);
    spawn_lemonbar(dpy);
    bar_start_refresh_thread(clients, config.max_windows, &current_client, dpy);
  } else {
    runtime_bar_enabled = 0;
    lemonbar_height = 0;
    kill_lemonbar();
  }

  for (int i = 0; i < config.max_windows; i++) {
    if (clients[i].active) {
      XMoveResizeWindow(dpy, clients[i].win, 0, lemonbar_height, screen_width,
                        screen_height - lemonbar_height);
    }
  }
  bar_trigger_update();
}

void setup() {
  signal(SIGUSR1, handle_sigusr1);
  signal(SIGPIPE, SIG_IGN);

  XSetErrorHandler(x_error_handler);
  dpy = XOpenDisplay(NULL);
  if (!dpy) {
    fprintf(stderr, "Cannot open display\n");
    exit(1);
  }

  root = DefaultRootWindow(dpy);
  screen_width = DisplayWidth(dpy, DefaultScreen(dpy));
  screen_height = DisplayHeight(dpy, DefaultScreen(dpy));

  if (config.max_windows > 128) {
    config.max_windows = 128;
  }
  clients = malloc(sizeof(Client) * 128);
  if (!clients) {
    fprintf(stderr, "Out of memory\n");
    exit(1);
  }

  if (config.bar_enabled && is_command_in_path("lemonbar")) {
    runtime_bar_enabled = 1;
    lemonbar_height = get_scaled_bar_height(dpy);
    spawn_lemonbar(dpy);
  } else {
    runtime_bar_enabled = 0;
    lemonbar_height = 0;
  }

  net_wm_window_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
  net_wm_window_type_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
  net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
  net_supporting_wm_check = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
  net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
  net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
  net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);

  Atom supported[] = {
    net_supported,
    net_supporting_wm_check,
    net_client_list,
    net_active_window,
    net_wm_window_type,
    net_wm_window_type_dock
  };
  XChangeProperty(dpy, root, net_supported, XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)supported, sizeof(supported) / sizeof(Atom));

  wm_check_win = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
  XChangeProperty(dpy, wm_check_win, net_supporting_wm_check, XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wm_check_win, 1);
  XChangeProperty(dpy, wm_check_win, net_wm_name, XInternAtom(dpy, "UTF8_STRING", False), 8,
                  PropModeReplace, (unsigned char *)"monowm", 6);
  XChangeProperty(dpy, root, net_supporting_wm_check, XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wm_check_win, 1);
  XChangeProperty(dpy, root, net_wm_name, XInternAtom(dpy, "UTF8_STRING", False), 8,
                  PropModeReplace, (unsigned char *)"monowm", 6);

  for (int i = 0; i < 128; i++) {
    clients[i].win = None;
    clients[i].active = 0;
    clients[i].ignore_unmap = 0;
  }

  XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask | StructureNotifyMask);

  keys_grab(dpy, root);

  XSync(dpy, False);

  for (int i = 0; i < config.autostart_count; i++) {
    spawn(config.autostarts[i]);
  }

  bar_start_refresh_thread(clients, config.max_windows, &current_client, dpy);

  update_bar(clients, config.max_windows, current_client, dpy);
}

int add_client(Window w) {
  for (int i = 0; i < config.max_windows; i++) {
    if (!clients[i].active) {
      clients[i].win = w;
      clients[i].active = 1;
      clients[i].ignore_unmap = 0;
      update_client_list();
      return i;
    }
  }
  return -1;
}

void focus_client(int idx) {
  if (idx < 0 || idx >= config.max_windows || !clients[idx].active)
    return;

  XMoveResizeWindow(dpy, clients[idx].win, 0, lemonbar_height, screen_width,
                    screen_height - lemonbar_height);

#if !KEEP_INACTIVE_MAPPED
  int old_client = current_client;
#endif
  current_client = idx;

  XMapWindow(dpy, clients[idx].win);
  XRaiseWindow(dpy, clients[idx].win);
  XSetInputFocus(dpy, clients[idx].win, RevertToPointerRoot, CurrentTime);

#if !KEEP_INACTIVE_MAPPED
  if (old_client >= 0 && old_client < config.max_windows &&
      clients[old_client].active && old_client != idx) {
    clients[old_client].ignore_unmap = 1;
    XUnmapWindow(dpy, clients[old_client].win);
  }
#endif

  update_active_window();
  update_bar(clients, config.max_windows, current_client, dpy);
}

void remove_client(int idx) {
  if (idx < 0 || idx >= config.max_windows || !clients[idx].active)
    return;

  for (int i = idx; i < config.max_windows - 1; i++) {
    clients[i] = clients[i + 1];
  }
  clients[config.max_windows - 1].win = None;
  clients[config.max_windows - 1].active = 0;

  update_client_list();

  if (current_client == idx) {
    if (clients[idx].active) {
      focus_client(idx);
    } else if (idx > 0 && clients[idx - 1].active) {
      focus_client(idx - 1);
    } else {
      current_client = -1;
      update_active_window();
      update_bar(clients, config.max_windows, current_client, dpy);
    }
  } else if (current_client > idx) {
    current_client--;
    update_active_window();
    update_bar(clients, config.max_windows, current_client, dpy);
  } else {
    update_bar(clients, config.max_windows, current_client, dpy);
  }
}

void handle_client_message(XClientMessageEvent *e) {
  if (e->message_type == net_active_window) {
    for (int i = 0; i < config.max_windows; i++) {
      if (clients[i].active && clients[i].win == e->window) {
        focus_client(i);
        break;
      }
    }
  }
}

void manage_window(Window w) {
  int idx = add_client(w);
  if (idx == -1) {
    fprintf(stderr, "Maximum windows reached\n");
    return;
  }

  XMoveResizeWindow(dpy, w, 0, lemonbar_height, screen_width,
                    screen_height - lemonbar_height);

  XSetWindowBorderWidth(dpy, w, 0);

#if CLIENT_BG_PREVENT_FLASH == 1
  XSetWindowBackground(dpy, w, BlackPixel(dpy, DefaultScreen(dpy)));
#elif CLIENT_BG_PREVENT_FLASH == 2
  XSetWindowBackgroundPixmap(dpy, w, None);
#endif

  focus_client(idx);
}

int is_dock(Window w) {
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *prop = NULL;

  if (XGetWindowProperty(dpy, w, net_wm_window_type, 0, sizeof(Atom), False,
                         XA_ATOM, &actual_type, &actual_format, &nitems,
                         &bytes_after, &prop) == Success &&
      prop) {
    Atom type = *(Atom *)prop;
    XFree(prop);
    if (type == net_wm_window_type_dock)
      return 1;
  }

  XClassHint ch;
  int result = 0;
  if (XGetClassHint(dpy, w, &ch)) {
    if (ch.res_name && (strcmp(ch.res_name, "lemonbar") == 0 ||
                        strcmp(ch.res_name, "bar") == 0)) {
      result = 1;
    }
    if (ch.res_name)
      XFree(ch.res_name);
    if (ch.res_class)
      XFree(ch.res_class);
  }
  return result;
}

void handle_map_request(XMapRequestEvent *e) {
  XWindowAttributes attr;
  XGetWindowAttributes(dpy, e->window, &attr);
  if (attr.override_redirect)
    return;

  if (is_dock(e->window)) {
    XMapWindow(dpy, e->window);
    return;
  }

  manage_window(e->window);
}

void handle_destroy_notify(XDestroyWindowEvent *e) {
  for (int i = 0; i < config.max_windows; i++) {
    if (clients[i].active && clients[i].win == e->window) {
      remove_client(i);
      break;
    }
  }
}

void handle_unmap_notify(XUnmapEvent *e) {
  for (int i = 0; i < config.max_windows; i++) {
    if (clients[i].active && clients[i].win == e->window) {
      if (clients[i].ignore_unmap) {
        clients[i].ignore_unmap = 0;
      } else {
        remove_client(i);
      }
      break;
    }
  }
}

int main(int argc, char *argv[]) {
  config_load();

  if (argc > 1 && strcmp(argv[1], "--is-bar-enabled") == 0) {
    return config.bar_enabled ? 0 : 1;
  }

  if (argc > 1 && strcmp(argv[1], "--get-bar-height") == 0) {
    Display *d = XOpenDisplay(NULL);
    if (!d) {
      printf("%d\n", config.bar_height);
      return 0;
    }
    printf("%d\n", get_scaled_bar_height(d));
    XCloseDisplay(d);
    return 0;
  }

  if (argc > 1 && strcmp(argv[1], "--get-bar-font") == 0) {
    Display *d = XOpenDisplay(NULL);
    if (!d) {
      printf("%s:size=%d\n", config.bar_font_name, config.bar_font_size);
      return 0;
    }
    printf("%s:size=%d\n", config.bar_font_name, get_scaled_font_size(d));
    XCloseDisplay(d);
    return 0;
  }

  setup();

  XEvent ev;
  while (1) {
    XNextEvent(dpy, &ev);

    switch (ev.type) {
    case ConfigureNotify:
      if (ev.xconfigure.window == root) {
        screen_width = ev.xconfigure.width;
        screen_height = ev.xconfigure.height;
        for (int i = 0; i < config.max_windows; i++) {
          if (clients[i].active) {
            XMoveResizeWindow(dpy, clients[i].win, 0, lemonbar_height, screen_width,
                              screen_height - lemonbar_height);
          }
        }
      }
      break;
    case MapRequest:
      handle_map_request(&ev.xmaprequest);
      break;
    case DestroyNotify:
      handle_destroy_notify(&ev.xdestroywindow);
      break;
    case UnmapNotify:
      handle_unmap_notify(&ev.xunmap);
      break;
    case KeyPress:
      keys_handle(dpy, &ev.xkey);
      break;
    case ClientMessage:
      handle_client_message(&ev.xclient);
      break;
    }
  }

  XCloseDisplay(dpy);
  kill_lemonbar();
  if (clients) free(clients);
  return 0;
}
