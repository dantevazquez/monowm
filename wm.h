#ifndef WM_H
#define WM_H

#include <X11/Xlib.h>
#include <stdint.h>

typedef struct {
  Window win;
  int active;
  int ignore_unmap;
  uint64_t focus_serial;
#ifndef NO_BAR
  const char *bar_icon;
  char bar_name[64];
#endif
} Client;

#endif
