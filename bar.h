#ifndef BAR_H
#define BAR_H

#include "wm.h"
#include <X11/Xlib.h>

#ifdef NO_BAR

static inline void bar_init(Display *display, Window root_window, int width,
                            int height) {
  (void)display;
  (void)root_window;
  (void)width;
  (void)height;
}

static inline void bar_reload(int width, int height) {
  (void)width;
  (void)height;
}

static inline void bar_screen_changed(int width, int height) {
  (void)width;
  (void)height;
}

static inline void bar_redraw(Client *clients, int max_windows,
                              int current_client) {
  (void)clients;
  (void)max_windows;
  (void)current_client;
}

static inline void bar_handle_expose(const XExposeEvent *event,
                                     Client *clients, int max_windows,
                                     int current_client) {
  (void)event;
  (void)clients;
  (void)max_windows;
  (void)current_client;
}

static inline void bar_handle_property(const XPropertyEvent *event,
                                       Client *clients, int max_windows,
                                       int current_client) {
  (void)event;
  (void)clients;
  (void)max_windows;
  (void)current_client;
}

static inline int bar_is_visible(void) { return 0; }
static inline int bar_height(void) { return 0; }
static inline void bar_shutdown(void) {}

#else

void bar_init(Display *display, Window root_window, int width, int height);
void bar_reload(int width, int height);
void bar_screen_changed(int width, int height);
void bar_redraw(Client *clients, int max_windows, int current_client);
void bar_handle_expose(const XExposeEvent *event, Client *clients,
                       int max_windows, int current_client);
void bar_handle_property(const XPropertyEvent *event, Client *clients,
                         int max_windows, int current_client);
int bar_is_visible(void);
int bar_height(void);
void bar_shutdown(void);

#endif

#endif
