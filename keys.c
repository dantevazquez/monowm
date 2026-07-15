#include "keys.h"
#include "config.h"
#include "bar.h"
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <string.h>

extern int current_client;
extern int screen_width, screen_height;
extern Client *clients;

void focus_client(int idx);
void reload_config(void);
void spawn(const char *cmd);
void toggle_bar(void);


void keys_grab(Display *dpy, Window root) {
  unsigned int mods;
  KeySym sym;

  // 1. Grab quit keybind
  if (parse_key_combo(config.bind_quit, &mods, &sym)) {
    XGrabKey(dpy, XKeysymToKeycode(dpy, sym), mods, root, True, GrabModeAsync,
             GrabModeAsync);
  }

  // 2. Grab cycle keybind
  if (config.cycle_enabled && parse_key_combo(config.bind_cycle, &mods, &sym)) {
    XGrabKey(dpy, XKeysymToKeycode(dpy, sym), mods, root, True, GrabModeAsync,
             GrabModeAsync);
  }

  // 3. Grab reload keybind
  if (parse_key_combo(config.bind_reload, &mods, &sym)) {
    XGrabKey(dpy, XKeysymToKeycode(dpy, sym), mods, root, True, GrabModeAsync,
             GrabModeAsync);
  }

  // 3.5. Grab toggle bar keybind
  if (parse_key_combo(config.bind_toggle_bar, &mods, &sym)) {
    XGrabKey(dpy, XKeysymToKeycode(dpy, sym), mods, root, True, GrabModeAsync,
             GrabModeAsync);
  }

  // 4. Grab switch window modifier keys (1-9)
  if (parse_key_combo(config.bind_switch_window_mod, &mods, &sym)) {
    for (int i = 0; i < 9; i++) {
      XGrabKey(dpy, XKeysymToKeycode(dpy, XK_1 + i), mods, root, True, GrabModeAsync,
               GrabModeAsync);
    }
  }

  // 5. Grab custom keybinds
  for (int i = 0; i < config.keybind_count; i++) {
    XGrabKey(dpy, XKeysymToKeycode(dpy, config.keybinds[i].keysym),
             config.keybinds[i].modifiers, root, True, GrabModeAsync, GrabModeAsync);
  }
}

void keys_handle(Display *dpy, XKeyEvent *e) {
  KeySym key = XLookupKeysym(e, 0);
  unsigned int state = e->state;
  state &= ~(LockMask | Mod2Mask);

  unsigned int mods;
  KeySym sym;

  // 1. Quit Keybind
  if (parse_key_combo(config.bind_quit, &mods, &sym)) {
    if (key == sym && state == mods) {
      if (current_client >= 0 && clients[current_client].active) {
        Window win = clients[current_client].win;
        Atom *protocols = NULL;
        int n = 0;
        int supports_delete = 0;
        Atom proto_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);

        if (XGetWMProtocols(dpy, win, &protocols, &n)) {
          for (int i = 0; i < n; i++) {
            if (protocols[i] == proto_delete) {
              supports_delete = 1;
              break;
            }
          }
          if (protocols) {
            XFree(protocols);
          }
        }

        if (supports_delete) {
          XEvent ev;
          memset(&ev, 0, sizeof(ev));
          ev.type = ClientMessage;
          ev.xclient.window = win;
          ev.xclient.message_type = XInternAtom(dpy, "WM_PROTOCOLS", False);
          ev.xclient.format = 32;
          ev.xclient.data.l[0] = proto_delete;
          ev.xclient.data.l[1] = CurrentTime;
          XSendEvent(dpy, win, False, NoEventMask, &ev);
        } else {
          XKillClient(dpy, win);
        }
      }
      return;
    }
  }

  // 2. Cycle Keybind
  if (config.cycle_enabled && parse_key_combo(config.bind_cycle, &mods, &sym)) {
    if (key == sym && state == mods) {
      if (current_client < 0)
        return;

      int next_idx = current_client + 1;
      if (next_idx >= config.max_windows || !clients[next_idx].active) {
        next_idx = 0;
      }
      if (next_idx != current_client && clients[next_idx].active) {
        focus_client(next_idx);
      }
      return;
    }
  }

  // 3. Reload Keybind
  if (parse_key_combo(config.bind_reload, &mods, &sym)) {
    if (key == sym && state == mods) {
      reload_config();
      return;
    }
  }

  // 3.5. Toggle Bar Keybind
  if (parse_key_combo(config.bind_toggle_bar, &mods, &sym)) {
    if (key == sym && state == mods) {
      toggle_bar();
      return;
    }
  }

  // 4. Switch Modifier Keybinds (1-9)
  if (parse_key_combo(config.bind_switch_window_mod, &mods, &sym)) {
    if (key >= XK_1 && key <= XK_9 && state == mods) {
      int idx = key - XK_1;
      focus_client(idx);
      return;
    }
  }

  // 5. Custom Keybinds
  for (int i = 0; i < config.keybind_count; i++) {
    if (key == config.keybinds[i].keysym && state == config.keybinds[i].modifiers) {
      spawn(config.keybinds[i].cmd);
      return;
    }
  }
}
