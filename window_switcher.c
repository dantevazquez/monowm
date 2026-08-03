#define _GNU_SOURCE
#include "window_switcher.h"

#include "config.h"
#include "wm.h"
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#include <X11/keysym.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SWITCHER_MAX_WINDOWS 128

extern Client *clients;
extern int current_client;
extern int screen_width;
extern int screen_height;

void focus_client(int idx);
void request_close_client(int idx);

typedef struct {
  Display *dpy;
  Window root;
  Window overlay;
  GC gc;
  XftDraw *xft_draw;
  XftFont *label_font;
  XftColor text_color;
  XftColor muted_color;
  int text_color_allocated;
  int muted_color_allocated;
  unsigned long bg_pixel;
  unsigned long card_pixel;
  unsigned long border_pixel;
  unsigned long selected_pixel;
  unsigned long preview_bg_pixel;
  int composite_available;
  int active;
  int keyboard_grabbed;
  Window order[SWITCHER_MAX_WINDOWS];
  int count;
  int selected;
  unsigned int trigger_modifiers;
  unsigned int primary_modifier;
  KeySym trigger;
  Window redirected[SWITCHER_MAX_WINDOWS];
  int redirected_count;
} Switcher;

static Switcher switcher;

static unsigned long alloc_pixel(const char *name, unsigned long fallback) {
  XColor exact;
  XColor screen;
  Colormap colormap = DefaultColormap(switcher.dpy, DefaultScreen(switcher.dpy));
  if (XAllocNamedColor(switcher.dpy, colormap, name, &screen, &exact))
    return screen.pixel;
  return fallback;
}

static void set_xft_color(const char *name, XftColor *color, int *allocated) {
  int screen = DefaultScreen(switcher.dpy);
  if (*allocated) {
    XftColorFree(switcher.dpy, DefaultVisual(switcher.dpy, screen),
                 DefaultColormap(switcher.dpy, screen), color);
  }
  *allocated = XftColorAllocName(
      switcher.dpy, DefaultVisual(switcher.dpy, screen),
      DefaultColormap(switcher.dpy, screen), name, color);
}

static XftFont *open_switcher_font(void) {
  int screen = DefaultScreen(switcher.dpy);
  const char *families[] = {config.switcher_font_name, "monospace", "sans"};
  char pattern[256];

  for (size_t i = 0; i < sizeof(families) / sizeof(families[0]); i++) {
    if (!families[i] || families[i][0] == '\0')
      continue;
    snprintf(pattern, sizeof(pattern), "%s:size=%d", families[i],
             config.switcher_font_size);
    XftFont *font = XftFontOpenName(switcher.dpy, screen, pattern);
    if (font)
      return font;
  }
  return NULL;
}

void window_switcher_reload_config(void) {
  int screen = DefaultScreen(switcher.dpy);
  unsigned long black = BlackPixel(switcher.dpy, screen);
  unsigned long white = WhitePixel(switcher.dpy, screen);
  switcher.bg_pixel =
      alloc_pixel(config.switcher_color_background, black);
  switcher.card_pixel = alloc_pixel(config.switcher_color_card, black);
  switcher.border_pixel = alloc_pixel(config.switcher_color_border, white);
  switcher.selected_pixel = alloc_pixel(config.switcher_color_selected, white);
  switcher.preview_bg_pixel =
      alloc_pixel(config.switcher_color_preview_background, black);
  set_xft_color(config.switcher_color_text, &switcher.text_color,
                &switcher.text_color_allocated);
  set_xft_color(config.switcher_color_muted, &switcher.muted_color,
                &switcher.muted_color_allocated);
  XftFont *font = open_switcher_font();
  if (font) {
    if (switcher.label_font)
      XftFontClose(switcher.dpy, switcher.label_font);
    switcher.label_font = font;
  }
  if (switcher.overlay != None)
    XSetWindowBackground(switcher.dpy, switcher.overlay, switcher.bg_pixel);
}

static int client_index_for_window(Window window) {
  for (int i = 0; i < config.max_windows; i++) {
    if (clients[i].active && clients[i].win == window)
      return i;
  }
  return -1;
}

static unsigned int modifier_for_keysym(KeySym key) {
  switch (key) {
  case XK_Super_L:
  case XK_Super_R:
  case XK_Hyper_L:
  case XK_Hyper_R:
    return Mod4Mask;
  case XK_Alt_L:
  case XK_Alt_R:
  case XK_Meta_L:
  case XK_Meta_R:
    return Mod1Mask;
  case XK_Control_L:
  case XK_Control_R:
    return ControlMask;
  case XK_Shift_L:
  case XK_Shift_R:
    return ShiftMask;
  default:
    return 0;
  }
}

static unsigned int choose_primary_modifier(unsigned int modifiers) {
  if (modifiers & Mod4Mask)
    return Mod4Mask;
  if (modifiers & Mod1Mask)
    return Mod1Mask;
  if (modifiers & ControlMask)
    return ControlMask;
  if (modifiers & ShiftMask)
    return ShiftMask;
  return 0;
}

static void get_window_title(Window window, char *buf, size_t size) {
  Atom utf8 = XInternAtom(switcher.dpy, "UTF8_STRING", False);
  Atom net_name = XInternAtom(switcher.dpy, "_NET_WM_NAME", False);
  Atom actual_type = None;
  int actual_format = 0;
  unsigned long items = 0;
  unsigned long remaining = 0;
  unsigned char *property = NULL;

  buf[0] = '\0';
  if (XGetWindowProperty(switcher.dpy, window, net_name, 0, 1024, False, utf8,
                         &actual_type, &actual_format, &items, &remaining,
                         &property) == Success &&
      property) {
    size_t length = items < size - 1 ? items : size - 1;
    memcpy(buf, property, length);
    buf[length] = '\0';
    XFree(property);
  }

  if (buf[0] == '\0') {
    char *legacy_name = NULL;
    if (XFetchName(switcher.dpy, window, &legacy_name) && legacy_name) {
      snprintf(buf, size, "%s", legacy_name);
      XFree(legacy_name);
    }
  }

  if (buf[0] == '\0') {
    XClassHint hint;
    if (XGetClassHint(switcher.dpy, window, &hint)) {
      snprintf(buf, size, "%s",
               hint.res_class ? hint.res_class
                              : (hint.res_name ? hint.res_name : "Window"));
      if (hint.res_name)
        XFree(hint.res_name);
      if (hint.res_class)
        XFree(hint.res_class);
    } else {
      snprintf(buf, size, "Window");
    }
  }
}

static void draw_text(XftFont *font, XftColor *color, int x, int baseline,
                      const char *text) {
  if (!switcher.xft_draw || !font || !text)
    return;
  XftDrawStringUtf8(switcher.xft_draw, color, font, x, baseline,
                    (const FcChar8 *)text, (int)strlen(text));
}

static int text_width(XftFont *font, const char *text) {
  XGlyphInfo extents;
  if (!font || !text)
    return 0;
  XftTextExtentsUtf8(switcher.dpy, font, (const FcChar8 *)text,
                     (int)strlen(text), &extents);
  return extents.xOff;
}

static void ellipsize(char *text, size_t size, int max_width) {
  const char *ellipsis = "...";
  size_t length = strlen(text);
  if (text_width(switcher.label_font, text) <= max_width)
    return;

  while (length > 0) {
    length--;
    while (length > 0 && ((unsigned char)text[length] & 0xc0) == 0x80)
      length--;
    text[length] = '\0';
    if (text_width(switcher.label_font, text) +
            text_width(switcher.label_font, ellipsis) <=
        max_width) {
      if (length + strlen(ellipsis) < size)
        memcpy(text + length, ellipsis, strlen(ellipsis) + 1);
      return;
    }
  }
}

static void draw_preview(Window window, int x, int y, int width, int height) {
  XSetForeground(switcher.dpy, switcher.gc, switcher.preview_bg_pixel);
  XFillRectangle(switcher.dpy, switcher.overlay, switcher.gc, x, y,
                 (unsigned int)width, (unsigned int)height);

  if (!switcher.composite_available || width <= 0 || height <= 0)
    return;

  XWindowAttributes attributes;
  if (!XGetWindowAttributes(switcher.dpy, window, &attributes) ||
      attributes.width <= 0 || attributes.height <= 0 ||
      attributes.map_state != IsViewable)
    return;

  Pixmap pixmap = XCompositeNameWindowPixmap(switcher.dpy, window);
  if (pixmap == None)
    return;

  XRenderPictFormat *source_format =
      XRenderFindVisualFormat(switcher.dpy, attributes.visual);
  XRenderPictFormat *target_format = XRenderFindVisualFormat(
      switcher.dpy, DefaultVisual(switcher.dpy, DefaultScreen(switcher.dpy)));
  if (!source_format || !target_format) {
    XFreePixmap(switcher.dpy, pixmap);
    return;
  }

  int target_width = width;
  int target_height = (int)((long long)attributes.height * width /
                            attributes.width);
  if (target_height > height) {
    target_height = height;
    target_width = (int)((long long)attributes.width * height /
                         attributes.height);
  }
  if (target_width < 1)
    target_width = 1;
  if (target_height < 1)
    target_height = 1;

  int target_x = x + (width - target_width) / 2;
  int target_y = y + (height - target_height) / 2;

  Picture source =
      XRenderCreatePicture(switcher.dpy, pixmap, source_format, 0, NULL);
  Picture target = XRenderCreatePicture(switcher.dpy, switcher.overlay,
                                        target_format, 0, NULL);

  XTransform transform = {{{
      XDoubleToFixed((double)attributes.width / target_width),
      XDoubleToFixed(0), XDoubleToFixed(0)},
                           {XDoubleToFixed(0),
                            XDoubleToFixed((double)attributes.height /
                                           target_height),
                            XDoubleToFixed(0)},
                           {XDoubleToFixed(0), XDoubleToFixed(0),
                            XDoubleToFixed(1)}}};
  XRenderSetPictureTransform(switcher.dpy, source, &transform);
  XRenderSetPictureFilter(switcher.dpy, source, FilterBilinear, NULL, 0);
  XRenderComposite(switcher.dpy, PictOpSrc, source, None, target, 0, 0, 0, 0,
                   target_x, target_y, (unsigned int)target_width,
                   (unsigned int)target_height);

  XRenderFreePicture(switcher.dpy, target);
  XRenderFreePicture(switcher.dpy, source);
  XFreePixmap(switcher.dpy, pixmap);
}

static int fit_score(int width, int height) {
  if (width <= 0 || height <= 0)
    return 0;
  int preview_width = width;
  int preview_height = preview_width * 9 / 16;
  if (preview_height > height) {
    preview_height = height;
    preview_width = preview_height * 16 / 9;
  }
  return preview_width * preview_height;
}

static void choose_grid(int count, int available_width, int available_height,
                        int gap, int *columns_out, int *rows_out,
                        int *cell_width_out, int *cell_height_out) {
  int best_columns = 1;
  int best_rows = count;
  int best_width = available_width;
  int best_height = count > 0 ? available_height / count : available_height;
  int best_score = -1;
  int max_columns = count < 6 ? count : 6;

  for (int columns = 1; columns <= max_columns; columns++) {
    int rows = (count + columns - 1) / columns;
    int cell_width = (available_width - gap * (columns - 1)) / columns;
    int cell_height = (available_height - gap * (rows - 1)) / rows;
    int score = fit_score(cell_width - 28, cell_height - 70);
    if (score > best_score) {
      best_score = score;
      best_columns = columns;
      best_rows = rows;
      best_width = cell_width;
      best_height = cell_height;
    }
  }

  int fit_width = best_width;
  int fit_height = best_height;
  if (best_width > 560)
    best_width = 560;
  if (best_height > 390)
    best_height = 390;
  best_width = best_width * config.switcher_tile_scale / 100;
  best_height = best_height * config.switcher_tile_scale / 100;
  if (best_width > fit_width)
    best_width = fit_width;
  if (best_height > fit_height)
    best_height = fit_height;
  if (best_width < 1)
    best_width = 1;
  if (best_height < 1)
    best_height = 1;
  *columns_out = best_columns;
  *rows_out = best_rows;
  *cell_width_out = best_width;
  *cell_height_out = best_height;
}

static void draw_switcher(void) {
  if (!switcher.active || switcher.overlay == None)
    return;

  XSetForeground(switcher.dpy, switcher.gc, switcher.bg_pixel);
  XFillRectangle(switcher.dpy, switcher.overlay, switcher.gc, 0, 0,
                 (unsigned int)screen_width, (unsigned int)screen_height);

  if (switcher.count <= 0) {
    const char *empty = "No windows";
    draw_text(switcher.label_font, &switcher.muted_color,
              (screen_width - text_width(switcher.label_font, empty)) / 2,
              screen_height / 2, empty);
    XFlush(switcher.dpy);
    return;
  }

  const int outer_margin = 36;
  const int header_height = 24;
  const int footer_height = 50;
  const int gap = 18;
  const int padding = 12;
  int available_width = screen_width - outer_margin * 2;
  int available_height = screen_height - header_height - footer_height - 20;
  int columns;
  int rows;
  int cell_width;
  int cell_height;
  choose_grid(switcher.count, available_width, available_height, gap, &columns,
              &rows, &cell_width, &cell_height);

  int grid_height = rows * cell_height + (rows - 1) * gap;
  int grid_y = header_height + (available_height - grid_height) / 2;

  for (int i = 0; i < switcher.count; i++) {
    int row = i / columns;
    int column = i % columns;
    int row_start = row * columns;
    int row_count = switcher.count - row_start;
    if (row_count > columns)
      row_count = columns;
    int row_width = row_count * cell_width + (row_count - 1) * gap;
    int x = (screen_width - row_width) / 2 + column * (cell_width + gap);
    int y = grid_y + row * (cell_height + gap);
    int selected = i == switcher.selected;

    XSetForeground(switcher.dpy, switcher.gc, switcher.card_pixel);
    XFillRectangle(switcher.dpy, switcher.overlay, switcher.gc, x, y,
                   (unsigned int)cell_width, (unsigned int)cell_height);
    XSetForeground(switcher.dpy, switcher.gc,
                   selected ? switcher.selected_pixel : switcher.border_pixel);
    XSetLineAttributes(switcher.dpy, switcher.gc, selected ? 4 : 1, LineSolid,
                       CapButt, JoinMiter);
    XDrawRectangle(switcher.dpy, switcher.overlay, switcher.gc, x, y,
                   (unsigned int)(cell_width - 1),
                   (unsigned int)(cell_height - 1));

    int preview_height = cell_height - 58;
    draw_preview(switcher.order[i], x + padding, y + padding,
                 cell_width - padding * 2, preview_height - padding);

    char title[512];
    get_window_title(switcher.order[i], title, sizeof(title));
    title[sizeof(title) - 4] = '\0';
    ellipsize(title, sizeof(title), cell_width - padding * 2 - 30);
    draw_text(switcher.label_font, &switcher.text_color, x + padding,
              y + cell_height - 18, title);

    char position[16];
    snprintf(position, sizeof(position), "%d", i + 1);
    int position_width = text_width(switcher.label_font, position);
    draw_text(switcher.label_font, &switcher.muted_color,
              x + cell_width - padding - position_width,
              y + cell_height - 18, position);
  }

  const char *help = "Tab / arrows: select     Q: close     Esc: cancel";
  draw_text(switcher.label_font, &switcher.muted_color,
            (screen_width - text_width(switcher.label_font, help)) / 2,
            screen_height - 20, help);
  XFlush(switcher.dpy);
}

static int has_external_compositor(void) {
  char selection_name[32];
  snprintf(selection_name, sizeof(selection_name), "_NET_WM_CM_S%d",
           DefaultScreen(switcher.dpy));
  Atom compositor_selection =
      XInternAtom(switcher.dpy, selection_name, False);
  return XGetSelectionOwner(switcher.dpy, compositor_selection) != None;
}

void window_switcher_client_added(Window window) {
  if (!switcher.composite_available)
    return;
  if (has_external_compositor())
    return;
  for (int i = 0; i < switcher.redirected_count; i++) {
    if (switcher.redirected[i] == window)
      return;
  }
  if (switcher.redirected_count >= SWITCHER_MAX_WINDOWS)
    return;
  XCompositeRedirectWindow(switcher.dpy, window, CompositeRedirectAutomatic);
  switcher.redirected[switcher.redirected_count++] = window;
}

static void finish_switcher(int commit) {
  if (!switcher.active)
    return;

  Window target = None;
  if (commit && switcher.selected >= 0 && switcher.selected < switcher.count)
    target = switcher.order[switcher.selected];

  switcher.active = 0;
  XUnmapWindow(switcher.dpy, switcher.overlay);
  if (switcher.keyboard_grabbed) {
    XUngrabKeyboard(switcher.dpy, CurrentTime);
    switcher.keyboard_grabbed = 0;
  }

  if (target != None) {
    int index = client_index_for_window(target);
    if (index >= 0)
      focus_client(index);
  }
  XFlush(switcher.dpy);
}

static void remove_order_entry(int position) {
  if (position < 0 || position >= switcher.count)
    return;
  for (int i = position; i < switcher.count - 1; i++)
    switcher.order[i] = switcher.order[i + 1];
  switcher.count--;
  if (switcher.count == 0) {
    switcher.selected = -1;
  } else if (switcher.selected >= switcher.count) {
    switcher.selected = 0;
  }
}

static void close_selected(void) {
  if (switcher.selected < 0 || switcher.selected >= switcher.count)
    return;
  Window target = switcher.order[switcher.selected];
  int index = client_index_for_window(target);
  remove_order_entry(switcher.selected);
  if (index >= 0)
    request_close_client(index);
  if (switcher.count == 0)
    finish_switcher(0);
  else
    draw_switcher();
}

static int compare_clients_by_focus(const void *left, const void *right) {
  Window left_window = *(const Window *)left;
  Window right_window = *(const Window *)right;
  int left_index = client_index_for_window(left_window);
  int right_index = client_index_for_window(right_window);
  uint64_t left_serial = left_index >= 0 ? clients[left_index].focus_serial : 0;
  uint64_t right_serial =
      right_index >= 0 ? clients[right_index].focus_serial : 0;
  if (left_serial < right_serial)
    return 1;
  if (left_serial > right_serial)
    return -1;
  return left_index - right_index;
}

void window_switcher_init(Display *display, Window root_window) {
  memset(&switcher, 0, sizeof(switcher));
  switcher.dpy = display;
  switcher.root = root_window;
  switcher.selected = -1;

  int event_base;
  int error_base;
  switcher.composite_available =
      XCompositeQueryExtension(display, &event_base, &error_base);

  int screen = DefaultScreen(display);
  window_switcher_reload_config();

  XSetWindowAttributes attributes;
  memset(&attributes, 0, sizeof(attributes));
  attributes.override_redirect = True;
  attributes.background_pixel = switcher.bg_pixel;
  attributes.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask;
  switcher.overlay = XCreateWindow(
      display, root_window, 0, 0, (unsigned int)screen_width,
      (unsigned int)screen_height, 0, DefaultDepth(display, screen), InputOutput,
      DefaultVisual(display, screen),
      CWOverrideRedirect | CWBackPixel | CWEventMask, &attributes);
  XStoreName(display, switcher.overlay, "monowm-switcher");
  switcher.gc = XCreateGC(display, switcher.overlay, 0, NULL);
  switcher.xft_draw = XftDrawCreate(display, switcher.overlay,
                                    DefaultVisual(display, screen),
                                    DefaultColormap(display, screen));
}

int window_switcher_is_active(void) { return switcher.active; }

void window_switcher_start(unsigned int modifiers, KeySym trigger,
                           int direction) {
  if (switcher.active) {
    if (switcher.count > 1) {
      switcher.selected =
          (switcher.selected + (direction < 0 ? -1 : 1) + switcher.count) %
          switcher.count;
      draw_switcher();
    }
    return;
  }

  switcher.count = 0;
  for (int i = 0;
       i < config.max_windows && switcher.count < SWITCHER_MAX_WINDOWS; i++) {
    if (clients[i].active)
      switcher.order[switcher.count++] = clients[i].win;
  }
  if (switcher.count == 0)
    return;

  qsort(switcher.order, (size_t)switcher.count, sizeof(switcher.order[0]),
        compare_clients_by_focus);
  switcher.selected =
      switcher.count == 1 ? 0 : (direction < 0 ? switcher.count - 1 : 1);
  switcher.trigger_modifiers = modifiers;
  switcher.primary_modifier = choose_primary_modifier(modifiers);
  switcher.trigger = trigger;
  switcher.active = 1;

  for (int i = 0; i < switcher.count; i++)
    window_switcher_client_added(switcher.order[i]);
  XSync(switcher.dpy, False);
  XMoveResizeWindow(switcher.dpy, switcher.overlay, 0, 0,
                    (unsigned int)screen_width, (unsigned int)screen_height);
  XMapRaised(switcher.dpy, switcher.overlay);
  switcher.keyboard_grabbed =
      XGrabKeyboard(switcher.dpy, switcher.root, False, GrabModeAsync,
                    GrabModeAsync, CurrentTime) == GrabSuccess;
  draw_switcher();
}

void window_switcher_handle_key_press(XKeyEvent *event) {
  if (!switcher.active)
    return;

  KeySym key = XLookupKeysym(event, 0);
  unsigned int state = event->state & ~(LockMask | Mod2Mask);
  if (key == switcher.trigger) {
    int backwards = !(switcher.trigger_modifiers & ShiftMask) &&
                    (state & ShiftMask);
    if (switcher.count > 1) {
      switcher.selected =
          (switcher.selected + (backwards ? -1 : 1) + switcher.count) %
          switcher.count;
      draw_switcher();
    }
  } else if (key == XK_Left || key == XK_Up) {
    if (switcher.count > 1) {
      switcher.selected =
          (switcher.selected - 1 + switcher.count) % switcher.count;
      draw_switcher();
    }
  } else if (key == XK_Right || key == XK_Down) {
    if (switcher.count > 1) {
      switcher.selected = (switcher.selected + 1) % switcher.count;
      draw_switcher();
    }
  } else if (key == XK_q || key == XK_Q) {
    close_selected();
  } else if (key == XK_Escape) {
    finish_switcher(0);
  } else if (key == XK_Return || key == XK_KP_Enter) {
    finish_switcher(1);
  }
}

void window_switcher_handle_key_release(XKeyEvent *event) {
  if (!switcher.active)
    return;
  KeySym key = XLookupKeysym(event, 0);
  if (switcher.primary_modifier != 0) {
    if (modifier_for_keysym(key) == switcher.primary_modifier)
      finish_switcher(1);
  } else if (key == switcher.trigger) {
    finish_switcher(1);
  }
}

void window_switcher_handle_expose(XExposeEvent *event) {
  if (switcher.active && event->window == switcher.overlay && event->count == 0)
    draw_switcher();
}

void window_switcher_client_removed(Window window) {
  for (int i = 0; i < switcher.redirected_count; i++) {
    if (switcher.redirected[i] == window) {
      XCompositeUnredirectWindow(switcher.dpy, window,
                                 CompositeRedirectAutomatic);
      for (int j = i; j < switcher.redirected_count - 1; j++)
        switcher.redirected[j] = switcher.redirected[j + 1];
      switcher.redirected_count--;
      break;
    }
  }
  if (!switcher.active)
    return;
  for (int i = 0; i < switcher.count; i++) {
    if (switcher.order[i] == window) {
      remove_order_entry(i);
      if (switcher.count == 0)
        finish_switcher(0);
      else {
        XRaiseWindow(switcher.dpy, switcher.overlay);
        draw_switcher();
      }
      return;
    }
  }
  XRaiseWindow(switcher.dpy, switcher.overlay);
  draw_switcher();
}

void window_switcher_screen_changed(void) {
  if (switcher.overlay == None)
    return;
  XMoveResizeWindow(switcher.dpy, switcher.overlay, 0, 0,
                    (unsigned int)screen_width, (unsigned int)screen_height);
  if (switcher.active) {
    XRaiseWindow(switcher.dpy, switcher.overlay);
    draw_switcher();
  }
}

void window_switcher_cancel(void) { finish_switcher(0); }
