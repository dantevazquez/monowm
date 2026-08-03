#ifndef WINDOW_SWITCHER_H
#define WINDOW_SWITCHER_H

#include <X11/Xlib.h>

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
