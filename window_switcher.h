#ifndef WINDOW_SWITCHER_H
#define WINDOW_SWITCHER_H

#include <X11/Xlib.h>

#ifdef NO_SWITCHER

static inline void window_switcher_init(Display *display, Window root_window) {
  (void)display;
  (void)root_window;
}

static inline void window_switcher_reload_config(void) {}

static inline int window_switcher_is_active(void) { return 0; }

static inline void window_switcher_start(unsigned int modifiers,
                                         KeySym trigger, int direction) {
  (void)modifiers;
  (void)trigger;
  (void)direction;
}

static inline void window_switcher_handle_key_press(XKeyEvent *event) {
  (void)event;
}

static inline void window_switcher_handle_key_release(XKeyEvent *event) {
  (void)event;
}

static inline void window_switcher_handle_expose(XExposeEvent *event) {
  (void)event;
}

static inline void window_switcher_client_added(Window window) {
  (void)window;
}

static inline void window_switcher_client_removed(Window window) {
  (void)window;
}

static inline void window_switcher_screen_changed(void) {}

static inline void window_switcher_cancel(void) {}

#else

void window_switcher_init(Display *display, Window root_window);
void window_switcher_reload_config(void);
int window_switcher_is_active(void);
void window_switcher_start(unsigned int modifiers, KeySym trigger, int direction);
void window_switcher_handle_key_press(XKeyEvent *event);
void window_switcher_handle_key_release(XKeyEvent *event);
void window_switcher_handle_expose(XExposeEvent *event);
void window_switcher_client_added(Window window);
void window_switcher_client_removed(Window window);
void window_switcher_screen_changed(void);
void window_switcher_cancel(void);

#endif

#endif
