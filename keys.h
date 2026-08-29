#ifndef KEYS_H
#define KEYS_H

#include <X11/Xlib.h>

// Grab all configured keybindings on the root window
void keys_grab(Display *dpy, Window root);

// Handle a key press event
void keys_handle(Display *dpy, XKeyEvent *e);

// Handle modifier release and finish an active MRU switch.
void keys_handle_release(Display *dpy, XKeyEvent *e);

// Commit and close an active MRU session before reconfiguring the keyboard.
void keys_cancel_mru(Display *dpy);

#endif
