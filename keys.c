#include "keys.h"
#include "config.h"
#include <X11/keysym.h>

#define MOD MOD_KEY

// These are provided by main.c
extern int current_client;
extern int screen_width, screen_height;

// Forward declarations for functions in main.c
void focus_client(int idx);

// Defined in bar.h, but we pull the struct from there
#include "bar.h"
extern Client clients[];

void keys_grab(Display *dpy, Window root) {
  XGrabKey(dpy, XKeysymToKeycode(dpy, XK_q), MOD, root, True, GrabModeAsync,
           GrabModeAsync);
  XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Tab), MOD, root, True, GrabModeAsync,
           GrabModeAsync);
}

void keys_handle(Display *dpy, XKeyEvent *e) {
  (void)dpy;
  KeySym key = XLookupKeysym(e, 0);

  // Super + Q: Close current window
  if (key == XK_q) {
    if (current_client >= 0 && clients[current_client].active) {
      XKillClient(dpy, clients[current_client].win);
    }
  } else if (key == XK_Tab) {

    if (current_client < 0)
      return;

    int next_idx = current_client + 1;
    // Wrap index to 0 if user was in the last tab
    if (next_idx >= MAX_WINDOWS || !clients[next_idx].active) {
      next_idx = 0;
    }
    // If the current client is not active
    if (next_idx != current_client && clients[next_idx].active) {
      focus_client(next_idx);
    }
  }
}
