#ifndef BAR_H
#define BAR_H

#include "wm.h"
#include <X11/Xlib.h>
#include <sys/types.h>

#ifdef NO_BAR

static inline const char *get_client_icon(Display *dpy, Window w) {
  (void)dpy;
  (void)w;
  return "";
}

static inline void update_bar(Client *clients, int max_windows,
                              int current_client, Display *dpy) {
  (void)clients;
  (void)max_windows;
  (void)current_client;
  (void)dpy;
}

static inline void bar_start_refresh_thread(Client *clients, int max_windows,
                                            int *current_client_ptr,
                                            Display *dpy) {
  (void)clients;
  (void)max_windows;
  (void)current_client_ptr;
  (void)dpy;
}

static inline void bar_trigger_update(void) {}

static inline void spawn_lemonbar(Display *d) { (void)d; }

static inline void kill_lemonbar(void) {}

#else

// Get the icon string for a client window
const char *get_client_icon(Display *dpy, Window w);

// Update bar output (call when windows change)
void update_bar(Client *clients, int max_windows, int current_client,
                Display *dpy);

// Start the periodic bar refresh thread
void bar_start_refresh_thread(Client *clients, int max_windows,
                              int *current_client_ptr, Display *dpy);

// Trigger an immediate update of the bar (async-signal-safe)
void bar_trigger_update(void);

void spawn_lemonbar(Display *d);
void kill_lemonbar(void);

extern int runtime_bar_enabled;
extern int lemonbar_pipe_fd;
extern pid_t lemonbar_pid;

#endif

#endif
