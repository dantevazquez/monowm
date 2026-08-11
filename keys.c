#include "keys.h"
#include "config.h"
#include "bar.h"
#include "window_switcher.h"
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
void request_close_client(int idx);

#ifdef NO_SWITCHER
static void cycle_client(int direction) {
  int index = current_client;
  if (index < 0 || index >= config.max_windows)
    index = direction < 0 ? 0 : -1;

  for (int checked = 0; checked < config.max_windows; checked++) {
    index = (index + (direction < 0 ? -1 : 1) + config.max_windows) %
            config.max_windows;
    if (clients[index].active) {
      focus_client(index);
      return;
    }
  }
}
#endif

static void grab_key(Display *dpy, Window root, KeyCode code,
                     unsigned int modifiers, Bool owner_events) {
  const unsigned int ignored_modifiers[] = {
      0, LockMask, Mod2Mask, LockMask | Mod2Mask};
  for (size_t i = 0;
       i < sizeof(ignored_modifiers) / sizeof(ignored_modifiers[0]); i++) {
    XGrabKey(dpy, code, modifiers | ignored_modifiers[i], root, owner_events,
             GrabModeAsync, GrabModeAsync);
  }
}

void keys_grab(Display *dpy, Window root) {
  unsigned int mods;
  KeySym sym;

  // 1. Grab quit keybind
  if (parse_key_combo(config.bind_quit, &mods, &sym)) {
    KeyCode code = XKeysymToKeycode(dpy, sym);
    if (code != 0) {
      grab_key(dpy, root, code, mods, True);
    }
  }

  // 2. Grab the window switch binding. The graphical switcher needs a modal
  // grab; the minimal switcher-free cycler handles each press immediately.
  if (parse_key_combo(config.bind_window_switcher, &mods, &sym)) {
    KeyCode code = XKeysymToKeycode(dpy, sym);
    if (code != 0) {
#ifdef NO_SWITCHER
      grab_key(dpy, root, code, mods, True);
      if (!(mods & ShiftMask))
        grab_key(dpy, root, code, mods | ShiftMask, True);
#else
      grab_key(dpy, root, code, mods, False);
      if (!(mods & ShiftMask))
        grab_key(dpy, root, code, mods | ShiftMask, False);
#endif
    }
  }

  // 3. Grab reload keybind
  if (parse_key_combo(config.bind_reload, &mods, &sym)) {
    KeyCode code = XKeysymToKeycode(dpy, sym);
    if (code != 0) {
      grab_key(dpy, root, code, mods, True);
    }
  }

  // 3.5. Grab toggle bar keybind
#ifndef NO_BAR
  if (parse_key_combo(config.bind_toggle_bar, &mods, &sym)) {
    KeyCode code = XKeysymToKeycode(dpy, sym);
    if (code != 0) {
      grab_key(dpy, root, code, mods, True);
    }
  }
#endif

  // 4. Grab switch window modifier keys (1-9)
  if (parse_key_combo(config.bind_switch_window_mod, &mods, &sym)) {
    for (int i = 0; i < 9; i++) {
      KeyCode code = XKeysymToKeycode(dpy, XK_1 + i);
      if (code != 0) {
        grab_key(dpy, root, code, mods, True);
      }
    }
  }

  // 5. Grab custom keybinds
  for (int i = 0; i < config.keybind_count; i++) {
    KeyCode code = XKeysymToKeycode(dpy, config.keybinds[i].keysym);
    if (code != 0) {
      grab_key(dpy, root, code, config.keybinds[i].modifiers, True);
    }
  }
}

void keys_handle(Display *dpy, XKeyEvent *e) {
  (void)dpy;
  if (window_switcher_is_active()) {
    window_switcher_handle_key_press(e);
    return;
  }

  KeySym key = XLookupKeysym(e, 0);
  unsigned int state = e->state;
  state &= ~(LockMask | Mod2Mask);

  unsigned int mods;
  KeySym sym;

  // 1. Quit Keybind
  if (parse_key_combo(config.bind_quit, &mods, &sym)) {
    if (key == sym && state == mods) {
      request_close_client(current_client);
      return;
    }
  }

  // 2. Native MRU window switcher
  if (parse_key_combo(config.bind_window_switcher, &mods, &sym)) {
    int backwards = !(mods & ShiftMask) && state == (mods | ShiftMask);
    if (key == sym && (state == mods || backwards)) {
#ifdef NO_SWITCHER
      cycle_client(backwards ? -1 : 1);
#else
      window_switcher_start(mods, sym, backwards ? -1 : 1);
#endif
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
#ifndef NO_BAR
  if (parse_key_combo(config.bind_toggle_bar, &mods, &sym)) {
    if (key == sym && state == mods) {
      toggle_bar();
      return;
    }
  }
#endif

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

void keys_handle_release(Display *dpy, XKeyEvent *e) {
  (void)dpy;
  if (window_switcher_is_active())
    window_switcher_handle_key_release(e);
}
