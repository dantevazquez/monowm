#ifndef WM_H
#define WM_H

#include <X11/Xlib.h>
#include <stdint.h>

typedef struct {
  Window win;
  int active;
  int ignore_unmap;
  uint64_t focus_serial;
} Client;

double get_dpi(Display *d);

#endif
