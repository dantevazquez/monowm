#ifndef BAR_H
#define BAR_H

#include <X11/Xlib.h>

// Forward declaration of Client (defined in main.c / wm.h)
typedef struct {
  Window win;
  int active;
  int ignore_unmap;
} Client;

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
