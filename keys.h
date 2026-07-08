#ifndef KEYS_H
#define KEYS_H

#include <X11/Xlib.h>

// Grab all configured keybindings on the root window
void keys_grab(Display *dpy, Window root);

// Handle a key press event
void keys_handle(Display *dpy, XKeyEvent *e);

#endif
