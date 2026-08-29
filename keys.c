#include "keys.h"
#include "config.h"
#include "wm.h"
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <string.h>

extern int current_client;
extern int screen_width, screen_height;
extern Client *clients;

void focus_client(int idx);
void focus_client_preview(int idx);
void commit_client_focus(int idx);
void reload_config(void);
void spawn(const char *cmd);
void request_close_client(int idx);

typedef struct {
  Window window;
  uint64_t focus_serial;
} MruEntry;

static MruEntry mru_entries[128];
static int mru_count;
static int mru_position;
static int mru_active;
static unsigned int mru_modifiers;
static KeySym mru_key = NoSymbol;

static int client_index_for_window(Window window) {
  for (int i = 0; i < config.max_windows; i++) {
    if (clients[i].active && clients[i].win == window)
      return i;
  }
  return -1;
}

static void mru_finish(Display *dpy, Time time) {
  if (!mru_active)
    return;

  commit_client_focus(current_client);
  mru_active = 0;
  mru_count = 0;
  mru_position = 0;
  mru_modifiers = 0;
  mru_key = NoSymbol;
  XUngrabKeyboard(dpy, time);
}

void keys_cancel_mru(Display *dpy) {
  mru_finish(dpy, CurrentTime);
}

static int mru_begin(Display *dpy, XKeyEvent *event,
                     unsigned int modifiers, KeySym key) {
  mru_count = 0;
  for (int i = 0; i < config.max_windows && mru_count < 128; i++) {
    if (!clients[i].active)
      continue;
    mru_entries[mru_count].window = clients[i].win;
    mru_entries[mru_count].focus_serial = clients[i].focus_serial;
    mru_count++;
  }

  if (mru_count < 2) {
    mru_count = 0;
    return 0;
  }

  // Stable insertion sort keeps opening order as the tie breaker for windows
  // that have never received focus.
  for (int i = 1; i < mru_count; i++) {
    MruEntry entry = mru_entries[i];
    int j = i;
    while (j > 0 &&
           mru_entries[j - 1].focus_serial < entry.focus_serial) {
      mru_entries[j] = mru_entries[j - 1];
      j--;
    }
    mru_entries[j] = entry;
  }

  // The current window is the starting point even if focus history was reset.
  if (current_client >= 0 && current_client < config.max_windows &&
      clients[current_client].active) {
    Window current_window = clients[current_client].win;
    for (int i = 0; i < mru_count; i++) {
      if (mru_entries[i].window == current_window) {
        MruEntry entry = mru_entries[i];
        memmove(&mru_entries[1], &mru_entries[0],
                (size_t)i * sizeof(mru_entries[0]));
        mru_entries[0] = entry;
        break;
      }
    }
  }

  if (XGrabKeyboard(dpy, DefaultRootWindow(dpy), False, GrabModeAsync,
                    GrabModeAsync, event->time) != GrabSuccess) {
    mru_count = 0;
    return 0;
  }

  mru_position = 0;
  mru_active = 1;
  mru_modifiers = modifiers;
  mru_key = key;
  return 1;
}

static void mru_advance(void) {
  if (!mru_active || mru_count < 2)
    return;

  for (int checked = 0; checked < mru_count; checked++) {
    mru_position = (mru_position + 1) % mru_count;
    int index = client_index_for_window(mru_entries[mru_position].window);
    if (index >= 0) {
      focus_client_preview(index);
      return;
    }
  }
}

static unsigned int modifier_mask_for_key(KeySym key) {
  switch (key) {
  case XK_Shift_L:
  case XK_Shift_R:
    return ShiftMask;
  case XK_Control_L:
  case XK_Control_R:
    return ControlMask;
  case XK_Alt_L:
  case XK_Alt_R:
  case XK_Meta_L:
  case XK_Meta_R:
    return Mod1Mask;
  case XK_Super_L:
  case XK_Super_R:
  case XK_Hyper_L:
  case XK_Hyper_R:
    return Mod4Mask;
  default:
    return 0;
  }
}

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

  // Grab direct window cycling keybinds.
  if (parse_key_combo(config.bind_cycle_forward, &mods, &sym)) {
    KeyCode code = XKeysymToKeycode(dpy, sym);
    if (code != 0)
      grab_key(dpy, root, code, mods, True);
  }
  if (parse_key_combo(config.bind_cycle_back, &mods, &sym)) {
    KeyCode code = XKeysymToKeycode(dpy, sym);
    if (code != 0)
      grab_key(dpy, root, code, mods, True);
  }

  // Grab the key that starts the held-modifier MRU session.
  if (parse_key_combo(config.bind_mru_switcher, &mods, &sym)) {
    KeyCode code = XKeysymToKeycode(dpy, sym);
    if (code != 0)
      grab_key(dpy, root, code, mods, True);
  }

  // 3. Grab reload keybind
  if (parse_key_combo(config.bind_reload, &mods, &sym)) {
    KeyCode code = XKeysymToKeycode(dpy, sym);
    if (code != 0) {
      grab_key(dpy, root, code, mods, True);
    }
  }

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
  KeySym key = XLookupKeysym(e, 0);
  unsigned int state = e->state;
  state &= ~(LockMask | Mod2Mask);

  unsigned int mods;
  KeySym sym;

  // MRU switching takes precedence if a custom binding uses the same combo.
  if (parse_key_combo(config.bind_mru_switcher, &mods, &sym) &&
      key == sym && state == mods) {
    if (!mru_active && !mru_begin(dpy, e, mods, sym))
      return;
    mru_advance();
    return;
  }

  // 1. Quit Keybind
  if (parse_key_combo(config.bind_quit, &mods, &sym)) {
    if (key == sym && state == mods) {
      mru_finish(dpy, e->time);
      request_close_client(current_client);
      return;
    }
  }

  // 2. Direct window cycling
  if (parse_key_combo(config.bind_cycle_forward, &mods, &sym)) {
    if (key == sym && state == mods) {
      mru_finish(dpy, e->time);
      cycle_client(1);
      return;
    }
  }
  if (parse_key_combo(config.bind_cycle_back, &mods, &sym)) {
    if (key == sym && state == mods) {
      mru_finish(dpy, e->time);
      cycle_client(-1);
      return;
    }
  }

  // 3. Reload Keybind
  if (parse_key_combo(config.bind_reload, &mods, &sym)) {
    if (key == sym && state == mods) {
      mru_finish(dpy, e->time);
      reload_config();
      return;
    }
  }

  // 4. Switch Modifier Keybinds (1-9)
  if (parse_key_combo(config.bind_switch_window_mod, &mods, &sym)) {
    if (key >= XK_1 && key <= XK_9 && state == mods) {
      mru_finish(dpy, e->time);
      int idx = key - XK_1;
      focus_client(idx);
      return;
    }
  }

  // 5. Custom Keybinds
  for (int i = 0; i < config.keybind_count; i++) {
    if (key == config.keybinds[i].keysym && state == config.keybinds[i].modifiers) {
      mru_finish(dpy, e->time);
      spawn(config.keybinds[i].cmd);
      return;
    }
  }
}

void keys_handle_release(Display *dpy, XKeyEvent *e) {
  if (!mru_active)
    return;

  KeySym key = XLookupKeysym(e, 0);
  unsigned int released_modifier = modifier_mask_for_key(key);
  if ((mru_modifiers != 0 && (released_modifier & mru_modifiers) != 0) ||
      (mru_modifiers == 0 && key == mru_key)) {
    mru_finish(dpy, e->time);
  }
}
