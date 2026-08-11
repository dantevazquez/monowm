#define _GNU_SOURCE
#include "appicons.h"
#include "bar.h"
#include "keys.h"
#include "window_switcher.h"
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
Atom monowm_reload;
Window wm_check_win;
static uint64_t focus_sequence = 0;

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
  Display *temp_dpy = XOpenDisplay(NULL);
  Display *target_d = temp_dpy ? temp_dpy : d;

  char *dpi_str = XGetDefault(target_d, "Xft", "dpi");
  double dpi = 96.0;
  if (dpi_str) {
    double parsed = atof(dpi_str);
    if (parsed > 0) {
      dpi = parsed;
    }
  } else {
    int screen = DefaultScreen(target_d);
    double height_mm = DisplayHeightMM(target_d, screen);
    if (height_mm > 0) {
      dpi = (DisplayHeight(target_d, screen) * 25.4) / height_mm;
    }
  }

  if (temp_dpy) {
    XCloseDisplay(temp_dpy);
  }
  return dpi;
}

int get_scaled_bar_height(Display *d) {
  double dpi = get_dpi(d);
  int font_size = (int)(config.bar_font_size * (dpi / 96.0) + 0.5);
  int height = font_size * 2;
  if (height < 10) height = 10;
  return height;
}

int get_scaled_font_size(Display *d) {
  double dpi = get_dpi(d);
  int font_size = (int)(config.bar_font_size * (dpi / 96.0) + 0.5);
  if (font_size < 4) font_size = 4;
  return font_size;
}

int get_client_y() {
  if (runtime_bar_enabled && config.bar_position == 'b') {
    return 0;
  }
  return lemonbar_height;
}

void spawn(const char *cmd) {
  pid_t pid = fork();
  if (pid == 0) {
    if (fork() == 0) {
      setsid();
      execlp("sh", "sh", "-c", cmd, NULL);
      _exit(0);
    }
    _exit(0);
  }
  if (pid > 0) {
    waitpid(pid, NULL, 0);
  }
}

void reload_config() {
  window_switcher_cancel();
  config_load();
  window_switcher_reload_config();
  if (config.max_windows > 128) {
    config.max_windows = 128;
  }

#ifdef NO_BAR
  runtime_bar_enabled = 0;
  lemonbar_height = 0;
#else
  kill_lemonbar();
  if (config.bar_enabled && is_command_in_path("lemonbar")) {
    runtime_bar_enabled = 1;
    lemonbar_height = get_scaled_bar_height(dpy);
    spawn_lemonbar(dpy);
  } else {
    runtime_bar_enabled = 0;
    lemonbar_height = 0;
  }
#endif

  XUngrabKey(dpy, AnyKey, AnyModifier, root);
  keys_grab(dpy, root);

  for (int i = 0; i < config.max_windows; i++) {
    if (clients[i].active) {
      XMoveResizeWindow(dpy, clients[i].win, 0, get_client_y(), screen_width,
                        screen_height - lemonbar_height);
    }
  }

  update_bar(clients, config.max_windows, current_client, dpy);
  printf("monowm: configuration reloaded\n");
}

void toggle_bar() {
#ifdef NO_BAR
  return;
#else
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
      XMoveResizeWindow(dpy, clients[i].win, 0, get_client_y(), screen_width,
                        screen_height - lemonbar_height);
    }
  }
  bar_trigger_update();
#endif
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

#ifdef NO_BAR
  runtime_bar_enabled = 0;
  lemonbar_height = 0;
#else
  if (config.bar_enabled && is_command_in_path("lemonbar")) {
    runtime_bar_enabled = 1;
    lemonbar_height = get_scaled_bar_height(dpy);
    spawn_lemonbar(dpy);
  } else {
    runtime_bar_enabled = 0;
    lemonbar_height = 0;
  }
#endif

  net_wm_window_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
  net_wm_window_type_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
  net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
  net_supporting_wm_check = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
  net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
  net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
  net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
  monowm_reload = XInternAtom(dpy, "_MONOWM_RELOAD", False);

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
    clients[i].focus_serial = 0;
  }

  XSelectInput(dpy, root,
               SubstructureRedirectMask | SubstructureNotifyMask |
                   StructureNotifyMask | KeyPressMask | KeyReleaseMask);

  window_switcher_init(dpy, root);
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
    if (clients[i].active && clients[i].win == w)
      return i;
  }

  for (int i = 0; i < config.max_windows; i++) {
    if (!clients[i].active) {
      clients[i].win = w;
      clients[i].active = 1;
      clients[i].ignore_unmap = 0;
      clients[i].focus_serial = 0;
      update_client_list();
      return i;
    }
  }
  return -1;
}

void focus_client(int idx) {
  if (idx < 0 || idx >= config.max_windows || !clients[idx].active)
    return;

  XMoveResizeWindow(dpy, clients[idx].win, 0, get_client_y(), screen_width,
                    screen_height - lemonbar_height);

#if !KEEP_INACTIVE_MAPPED
  int old_client = current_client;
#endif
  current_client = idx;
  clients[idx].focus_serial = ++focus_sequence;

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
  if (window_switcher_is_active())
    window_switcher_screen_changed();
}

void request_close_client(int idx) {
  if (idx < 0 || idx >= config.max_windows || !clients[idx].active)
    return;

  Window win = clients[idx].win;
  Atom *protocols = NULL;
  int count = 0;
  int supports_delete = 0;
  Atom wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
  Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);

  if (XGetWMProtocols(dpy, win, &protocols, &count)) {
    for (int i = 0; i < count; i++) {
      if (protocols[i] == wm_delete) {
        supports_delete = 1;
        break;
      }
    }
    if (protocols)
      XFree(protocols);
  }

  if (supports_delete) {
    XEvent event;
    memset(&event, 0, sizeof(event));
    event.type = ClientMessage;
    event.xclient.window = win;
    event.xclient.message_type = wm_protocols;
    event.xclient.format = 32;
    event.xclient.data.l[0] = wm_delete;
    event.xclient.data.l[1] = CurrentTime;
    XSendEvent(dpy, win, False, NoEventMask, &event);
  } else {
    XKillClient(dpy, win);
  }
}

static int most_recent_client(void) {
  int result = -1;
  uint64_t newest = 0;
  for (int i = 0; i < config.max_windows; i++) {
    if (clients[i].active &&
        (result < 0 || clients[i].focus_serial > newest)) {
      result = i;
      newest = clients[i].focus_serial;
    }
  }
  return result;
}

void remove_client(int idx) {
  if (idx < 0 || idx >= config.max_windows || !clients[idx].active)
    return;

  Window removed_window = clients[idx].win;
  int removed_current = current_client == idx;
  for (int i = idx; i < config.max_windows - 1; i++) {
    clients[i] = clients[i + 1];
  }
  clients[config.max_windows - 1].win = None;
  clients[config.max_windows - 1].active = 0;
  clients[config.max_windows - 1].ignore_unmap = 0;
  clients[config.max_windows - 1].focus_serial = 0;

  update_client_list();

  if (removed_current) {
    current_client = -1;
    int replacement = most_recent_client();
    if (replacement >= 0) {
      focus_client(replacement);
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
  window_switcher_client_removed(removed_window);
}

void handle_client_message(XClientMessageEvent *e) {
  if (e->message_type == net_active_window) {
    for (int i = 0; i < config.max_windows; i++) {
      if (clients[i].active && clients[i].win == e->window) {
        focus_client(i);
        break;
      }
    }
  } else if (e->message_type == monowm_reload) {
    reload_config();
  }
}

void manage_window(Window w) {
  int idx = add_client(w);
  if (idx == -1) {
    fprintf(stderr, "Maximum windows reached\n");
    return;
  }

  XMoveResizeWindow(dpy, w, 0, get_client_y(), screen_width,
                    screen_height - lemonbar_height);

  XSetWindowBorderWidth(dpy, w, 0);

#if CLIENT_BG_PREVENT_FLASH == 1
  XSetWindowBackground(dpy, w, BlackPixel(dpy, DefaultScreen(dpy)));
#elif CLIENT_BG_PREVENT_FLASH == 2
  XSetWindowBackgroundPixmap(dpy, w, None);
#endif

  window_switcher_client_added(w);
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

  if (argc > 1 && strcmp(argv[1], "--reload") == 0) {
    Display *d = XOpenDisplay(NULL);
    if (!d) {
      fprintf(stderr, "monowm: cannot open display to reload\n");
      return 1;
    }
    Window r = DefaultRootWindow(d);
    Atom reload_atom = XInternAtom(d, "_MONOWM_RELOAD", False);
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = r;
    ev.xclient.message_type = reload_atom;
    ev.xclient.format = 32;
    XSendEvent(d, r, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XSync(d, False);
    XCloseDisplay(d);
    printf("monowm: sent reload event to window manager\n");
    return 0;
  }

  if (argc > 1 && strcmp(argv[1], "--is-bar-enabled") == 0) {
#ifdef NO_BAR
    return 1;
#else
    return config.bar_enabled ? 0 : 1;
#endif
  }

  if (argc > 1 && strcmp(argv[1], "--get-bar-height") == 0) {
#ifdef NO_BAR
    printf("0\n");
    return 0;
#else
    Display *d = XOpenDisplay(NULL);
    if (!d) {
      printf("%d\n", config.bar_font_size * 2);
      return 0;
    }
    printf("%d\n", get_scaled_bar_height(d));
    XCloseDisplay(d);
    return 0;
#endif
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
            XMoveResizeWindow(dpy, clients[i].win, 0, get_client_y(), screen_width,
                              screen_height - lemonbar_height);
          }
        }
        window_switcher_screen_changed();
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
    case KeyRelease:
      keys_handle_release(dpy, &ev.xkey);
      break;
    case Expose:
      window_switcher_handle_expose(&ev.xexpose);
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
