#define _GNU_SOURCE
#include "bar.h"
#include "config.h"
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xutil.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define BAR_PADDING 8
#define FOCUSED_NAME_PADDING 7
#define ITEM_GAP 2
#define MODULE_GAP 8
#define STATUS_MAX 2048

typedef struct {
  const char *name;
  const char *icon;
} AppIcon;

/*
 * Nerd Font application icons live here rather than in the user's config.
 * Add the lowercase WM_CLASS (run `xprop WM_CLASS`) and its glyph to extend
 * the list. More specific class names should precede shorter ones.
 */
static const AppIcon app_icons[] = {
    {"firefox", ""},          {"librewolf", ""},
    {"google-chrome", ""},    {"chromium", ""},
    {"brave-browser", "󰖟"},    {"qutebrowser", ""},
    {"gnome-terminal", ""},   {"xfce4-terminal", ""},
    {"alacritty", ""},        {"kitty", "󰄛"},
    {"wezterm", ""},          {"konsole", ""},
    {"xterm", ""},            {"foot", ""},
    {"st", ""},               {"neovide", ""},
    {"nvim", ""},             {"vim", ""},
    {"emacs", ""},            {"code-oss", ""},
    {"vscodium", ""},         {"code", ""},
    {"sublime_text", ""},     {"jetbrains-idea", ""},
    {"thunar", ""},           {"nautilus", ""},
    {"pcmanfm", ""},          {"dolphin", ""},
    {"nemo", ""},             {"ranger", ""},
    {"lf", ""},               {"vlc", ""},
    {"mpv", ""},              {"spotify", ""},
    {"cmus", ""},             {"audacious", ""},
    {"pavucontrol", ""},      {"obs", ""},
    {"discord", ""},          {"telegram-desktop", ""},
    {"slack", ""},            {"signal", ""},
    {"thunderbird", ""},      {"evolution", ""},
    {"gimp", ""},             {"inkscape", ""},
    {"blender", "󰂫"},          {"libreoffice-writer", ""},
    {"libreoffice-calc", ""}, {"libreoffice-impress", ""},
    {"libreoffice", ""},      {"zathura", ""},
    {"evince", ""},           {"okular", ""},
    {"nsxiv", ""},            {"sxiv", ""},
    {"feh", ""},              {"steam", ""},
    {"lutris", ""},           {"virtualbox", "󰆧"},
    {"qemu", "󰍹"},             {"keepassxc", ""},
    {"bitwarden", ""},        {"transmission", ""},
    {"qbittorrent", ""},
};

/* nf-md-application: used whenever no WM_CLASS entry matches. */
static const char *fallback_icon = "󰣆";

static Display *bar_display;
static Window bar_root;
static Window bar_window;
static Pixmap bar_buffer;
static GC bar_gc;
static XftDraw *bar_draw;
static XftFont *bar_font;
static XftColor text_color;
static XftColor active_text_color;
static XftColor background_color;
static XftColor highlight_color;
static int text_color_ready;
static int active_text_color_ready;
static int background_color_ready;
static int highlight_color_ready;
static int bar_width_px;
static int bar_screen_height;
static int bar_height_px;
static int buffer_valid;
static char status_text[STATUS_MAX];

static int configured_bar_height(void) {
  int font_height = config.bar_font_size;
  int height;

  if (bar_font && bar_font->ascent + bar_font->descent > 0)
    font_height = bar_font->ascent + bar_font->descent;
  height = font_height + config.bar_vertical_padding * 2;
  if (height > bar_screen_height)
    height = bar_screen_height;
  return height > 0 ? height : 1;
}

static int name_matches(const char *class_name, const char *candidate) {
  size_t candidate_len;
  if (!class_name || !candidate)
    return 0;
  if (strcasecmp(class_name, candidate) == 0)
    return 1;

  candidate_len = strlen(candidate);
  return candidate_len >= 4 && strcasestr(class_name, candidate) != NULL;
}

static const char *icon_for_name(const char *class_name) {
  for (size_t i = 0; i < sizeof(app_icons) / sizeof(app_icons[0]); i++) {
    if (name_matches(class_name, app_icons[i].name))
      return app_icons[i].icon;
  }
  return NULL;
}

static void format_program_name(const char *class_name, char *output,
                                size_t output_size) {
  const char *name;
  const char *last_dot;
  size_t offset = 0;
  int capitalize = 1;

  if (!class_name || class_name[0] == '\0') {
    snprintf(output, output_size, "Program");
    return;
  }

  last_dot = strrchr(class_name, '.');
  name = last_dot && last_dot[1] != '\0' ? last_dot + 1 : class_name;
  while (isspace((unsigned char)*name))
    name++;

  for (; *name != '\0' && offset + 1 < output_size; name++) {
    unsigned char character = (unsigned char)*name;
    if (character == '-' || character == '_' || isspace(character)) {
      if (offset > 0 && output[offset - 1] != ' ' &&
          offset + 1 < output_size)
        output[offset++] = ' ';
      capitalize = 1;
      continue;
    }
    output[offset++] = capitalize ? (char)toupper(character) : (char)character;
    capitalize = 0;
  }
  while (offset > 0 && output[offset - 1] == ' ')
    offset--;
  output[offset] = '\0';
  if (offset == 0)
    snprintf(output, output_size, "Program");
}

static void load_client_identity(Client *client) {
  XClassHint hint;
  const char *icon = NULL;
  const char *class_name = NULL;

  if (client->bar_icon && client->bar_name[0] != '\0')
    return;

  memset(&hint, 0, sizeof(hint));
  if (XGetClassHint(bar_display, client->win, &hint)) {
    icon = icon_for_name(hint.res_class);
    if (!icon)
      icon = icon_for_name(hint.res_name);
    class_name = hint.res_class && hint.res_class[0] != '\0' ? hint.res_class
                                                              : hint.res_name;
    format_program_name(class_name, client->bar_name,
                        sizeof(client->bar_name));
    if (hint.res_name)
      XFree(hint.res_name);
    if (hint.res_class)
      XFree(hint.res_class);
  } else {
    format_program_name(NULL, client->bar_name, sizeof(client->bar_name));
  }
  client->bar_icon = icon ? icon : fallback_icon;
}

static const char *client_icon(Client *client) {
  load_client_identity(client);
  return client->bar_icon;
}

static const char *client_program_name(Client *client) {
  load_client_identity(client);
  return client->bar_name;
}

static int allocate_color(const char *requested, const char *fallback,
                          XftColor *color) {
  Visual *visual = DefaultVisual(bar_display, DefaultScreen(bar_display));
  Colormap colormap = DefaultColormap(bar_display, DefaultScreen(bar_display));

  if (XftColorAllocName(bar_display, visual, colormap, requested, color))
    return 1;
  return XftColorAllocName(bar_display, visual, colormap, fallback, color);
}

static void read_root_status(void) {
  Atom actual_type;
  int actual_format;
  unsigned long item_count;
  unsigned long bytes_after;
  unsigned char *value = NULL;

  status_text[0] = '\0';
  if (XGetWindowProperty(bar_display, bar_root, XA_WM_NAME, 0,
                         STATUS_MAX / 4, False, AnyPropertyType, &actual_type,
                         &actual_format, &item_count, &bytes_after,
                         &value) != Success ||
      !value)
    return;

  if (actual_format == 8 && actual_type != None) {
    size_t length = item_count < STATUS_MAX - 1 ? item_count : STATUS_MAX - 1;
    memcpy(status_text, value, length);
    status_text[length] = '\0';
    for (size_t i = 0; i < length; i++) {
      if ((unsigned char)status_text[i] < 32 && status_text[i] != '\t')
        status_text[i] = ' ';
    }
  }
  XFree(value);
}

static int text_width(const char *text) {
  XGlyphInfo extents;
  if (!bar_font || !text || text[0] == '\0')
    return 0;
  XftTextExtentsUtf8(bar_display, bar_font, (const FcChar8 *)text,
                     (int)strlen(text), &extents);
  return extents.xOff;
}

static void draw_text_with_color(int x, const char *text, XftColor *color) {
  XGlyphInfo extents;
  int baseline;
  if (!bar_draw || !bar_font || !color || !text)
    return;
  XftTextExtentsUtf8(bar_display, bar_font, (const FcChar8 *)text,
                     (int)strlen(text), &extents);
  baseline = (bar_height_px - extents.height) / 2 + extents.y;
  XftDrawStringUtf8(bar_draw, color, bar_font, x, baseline,
                    (const FcChar8 *)text, (int)strlen(text));
}

static void draw_text(int x, const char *text) {
  if (text_color_ready)
    draw_text_with_color(x, text, &text_color);
}

static void draw_status(int left, int right, int align_right) {
  char displayed[STATUS_MAX + 4];
  const char *ellipsis = "…";
  int available = right - left;
  int width;
  int truncated = 0;

  if (status_text[0] == '\0' || available <= 0)
    return;

  strncpy(displayed, status_text, STATUS_MAX - 1);
  displayed[STATUS_MAX - 1] = '\0';
  width = text_width(displayed);
  while (displayed[0] != '\0' && width > available) {
    size_t length = strlen(displayed);
    do {
      length--;
    } while (length > 0 &&
             ((unsigned char)displayed[length] & 0xc0) == 0x80);
    displayed[length] = '\0';
    truncated = 1;
    width = text_width(displayed) + text_width(ellipsis);
  }

  if (truncated) {
    size_t displayed_length;
    size_t ellipsis_length;
    if (text_width(ellipsis) > available)
      return;
    displayed_length = strlen(displayed);
    ellipsis_length = strlen(ellipsis);
    if (displayed_length + ellipsis_length >= sizeof(displayed))
      return;
    memcpy(displayed + displayed_length, ellipsis, ellipsis_length + 1);
    width = text_width(displayed);
  }
  draw_text(align_right ? right - width : left, displayed);
}

static void destroy_buffer(void) {
  if (bar_draw) {
    XftDrawDestroy(bar_draw);
    bar_draw = NULL;
  }
  if (bar_gc) {
    XFreeGC(bar_display, bar_gc);
    bar_gc = 0;
  }
  if (bar_buffer) {
    XFreePixmap(bar_display, bar_buffer);
    bar_buffer = None;
  }
  buffer_valid = 0;
}

static int create_buffer(void) {
  Visual *visual;
  Colormap colormap;

  destroy_buffer();
  if (!bar_window || bar_width_px < 1 || bar_height_px < 1)
    return 0;

  visual = DefaultVisual(bar_display, DefaultScreen(bar_display));
  colormap = DefaultColormap(bar_display, DefaultScreen(bar_display));
  bar_buffer = XCreatePixmap(bar_display, bar_window, (unsigned)bar_width_px,
                             (unsigned)bar_height_px,
                             DefaultDepth(bar_display,
                                          DefaultScreen(bar_display)));
  if (!bar_buffer)
    return 0;
  bar_gc = XCreateGC(bar_display, bar_buffer, 0, NULL);
  bar_draw = XftDrawCreate(bar_display, bar_buffer, visual, colormap);
  return bar_gc != 0 && bar_draw != NULL;
}

static void destroy_bar(void) {
  Visual *visual;
  Colormap colormap;

  if (!bar_display)
    return;

  visual = DefaultVisual(bar_display, DefaultScreen(bar_display));
  colormap = DefaultColormap(bar_display, DefaultScreen(bar_display));
  destroy_buffer();
  if (bar_window) {
    XDestroyWindow(bar_display, bar_window);
    bar_window = None;
  }
  if (bar_font) {
    XftFontClose(bar_display, bar_font);
    bar_font = NULL;
  }
  if (text_color_ready)
    XftColorFree(bar_display, visual, colormap, &text_color);
  if (active_text_color_ready)
    XftColorFree(bar_display, visual, colormap, &active_text_color);
  if (background_color_ready)
    XftColorFree(bar_display, visual, colormap, &background_color);
  if (highlight_color_ready)
    XftColorFree(bar_display, visual, colormap, &highlight_color);
  text_color_ready = 0;
  active_text_color_ready = 0;
  background_color_ready = 0;
  highlight_color_ready = 0;
  bar_height_px = 0;
}

static void create_bar(void) {
  XSetWindowAttributes attributes;
  unsigned long attribute_mask;
  char font_pattern[256];
  int y;
  Atom type_atom;
  Atom dock_atom;
  Atom name_atom;
  Atom utf8_atom;
  const char *name = "monowm bar";

  if (!config.bar_enabled || bar_width_px < 1 || bar_screen_height < 1)
    return;

  snprintf(font_pattern, sizeof(font_pattern), "%s:pixelsize=%d",
           config.bar_font, config.bar_font_size);
  bar_font = XftFontOpenName(bar_display, DefaultScreen(bar_display),
                             font_pattern);
  if (!bar_font) {
    snprintf(font_pattern, sizeof(font_pattern), "monospace:pixelsize=%d",
             config.bar_font_size);
    bar_font = XftFontOpenName(bar_display, DefaultScreen(bar_display),
                               font_pattern);
    fprintf(stderr, "monowm: could not load bar font '%s'; using monospace\n",
            config.bar_font);
  }

  text_color_ready =
      allocate_color(config.bar_text_color, "#ffffff", &text_color);
  active_text_color_ready = allocate_color(config.bar_active_text_color,
                                           "#ffffff", &active_text_color);
  background_color_ready = allocate_color(config.bar_background_color,
                                          "#000000", &background_color);
  highlight_color_ready = allocate_color(config.bar_highlight_color,
                                         "#666666", &highlight_color);
  if (!bar_font || !text_color_ready || !active_text_color_ready ||
      !background_color_ready || !highlight_color_ready) {
    fprintf(stderr, "monowm: could not initialize built-in bar resources\n");
    destroy_bar();
    return;
  }

  bar_height_px = configured_bar_height();
  y = config.bar_position == 'b' ? bar_screen_height - bar_height_px : 0;

  memset(&attributes, 0, sizeof(attributes));
  attributes.override_redirect = True;
  attributes.background_pixel = background_color.pixel;
  attributes.event_mask = ExposureMask;
  attribute_mask = CWOverrideRedirect | CWBackPixel | CWEventMask;
  bar_window = XCreateWindow(
      bar_display, bar_root, 0, y, (unsigned)bar_width_px,
      (unsigned)bar_height_px, 0, CopyFromParent, InputOutput, CopyFromParent,
      attribute_mask, &attributes);

  type_atom = XInternAtom(bar_display, "_NET_WM_WINDOW_TYPE", False);
  dock_atom = XInternAtom(bar_display, "_NET_WM_WINDOW_TYPE_DOCK", False);
  XChangeProperty(bar_display, bar_window, type_atom, XA_ATOM, 32,
                  PropModeReplace, (unsigned char *)&dock_atom, 1);
  name_atom = XInternAtom(bar_display, "_NET_WM_NAME", False);
  utf8_atom = XInternAtom(bar_display, "UTF8_STRING", False);
  XChangeProperty(bar_display, bar_window, name_atom, utf8_atom, 8,
                  PropModeReplace, (const unsigned char *)name,
                  (int)strlen(name));

  if (!create_buffer()) {
    fprintf(stderr, "monowm: could not create built-in bar buffer\n");
    destroy_bar();
    return;
  }
  read_root_status();
  XMapRaised(bar_display, bar_window);
}

void bar_init(Display *display, Window root_window, int width, int height) {
  bar_display = display;
  bar_root = root_window;
  bar_width_px = width;
  bar_screen_height = height;
  create_bar();
}

void bar_reload(int width, int height) {
  destroy_bar();
  bar_width_px = width;
  bar_screen_height = height;
  create_bar();
}

void bar_screen_changed(int width, int height) {
  int y;
  if (!bar_window)
    return;

  bar_width_px = width;
  bar_screen_height = height;
  bar_height_px = configured_bar_height();
  y = config.bar_position == 'b' ? bar_screen_height - bar_height_px : 0;
  XMoveResizeWindow(bar_display, bar_window, 0, y, (unsigned)bar_width_px,
                    (unsigned)bar_height_px);
  create_buffer();
}

static void format_program_label(char *label, size_t label_size, int index,
                                 const char *icon) {
  if (config.bar_show_numbers && config.bar_show_icons) {
    snprintf(label, label_size, "%d %s", index + 1, icon);
  } else if (config.bar_show_numbers) {
    snprintf(label, label_size, "%d", index + 1);
  } else if (config.bar_show_icons) {
    snprintf(label, label_size, "%s", icon);
  } else {
    label[0] = '\0';
  }
}

void bar_redraw(Client *clients, int max_windows, int current_client) {
  int item_widths[128] = {0};
  int item_text_offsets[128] = {0};
  const char *icons[128] = {0};
  const char *focused_name = NULL;
  int program_list_width = 0;
  int focused_name_width = 0;
  int focused_name_offset = 0;
  int programs_width = 0;
  int programs_x = BAR_PADDING;
  int status_left = BAR_PADDING;
  int status_right = bar_width_px - BAR_PADDING;
  int status_align_right = 1;
  int x;
  char label[64];

  if (!bar_window || !bar_buffer || !bar_gc || !bar_draw || !bar_font)
    return;

  XSetForeground(bar_display, bar_gc, background_color.pixel);
  XFillRectangle(bar_display, bar_buffer, bar_gc, 0, 0,
                 (unsigned)bar_width_px, (unsigned)bar_height_px);

  int limit = max_windows < 128 ? max_windows : 128;
  if (config.bar_focused_name_position != 'h' && current_client >= 0 &&
      current_client < limit && clients[current_client].active) {
    XGlyphInfo extents;
    focused_name = client_program_name(&clients[current_client]);
    XftTextExtentsUtf8(bar_display, bar_font,
                       (const FcChar8 *)focused_name,
                       (int)strlen(focused_name), &extents);
    focused_name_width = extents.width + FOCUSED_NAME_PADDING * 2;
    focused_name_offset = FOCUSED_NAME_PADDING + extents.x;
  }

  if (config.bar_show_programs &&
      (config.bar_show_numbers || config.bar_show_icons)) {
    for (int i = 0; i < limit; i++) {
      XGlyphInfo extents;
      if (!clients[i].active)
        continue;
      if (config.bar_show_icons)
        icons[i] = client_icon(&clients[i]);
      format_program_label(label, sizeof(label), i, icons[i]);
      XftTextExtentsUtf8(bar_display, bar_font, (const FcChar8 *)label,
                         (int)strlen(label), &extents);
      item_widths[i] = extents.width + config.bar_program_padding * 2;
      item_text_offsets[i] = config.bar_program_padding + extents.x;
      program_list_width += item_widths[i] + ITEM_GAP;
    }
    if (program_list_width > 0)
      program_list_width -= ITEM_GAP;
  }

  programs_width = program_list_width + focused_name_width;
  if (program_list_width > 0 && focused_name_width > 0)
    programs_width += MODULE_GAP;

  if (programs_width > 0) {
    if (config.bar_programs_position == 'c') {
      programs_x = (bar_width_px - programs_width) / 2;
      status_left = programs_x + programs_width + BAR_PADDING;
    } else if (config.bar_programs_position == 'r') {
      programs_x = bar_width_px - BAR_PADDING - programs_width;
      status_right = programs_x - BAR_PADDING;
      status_align_right = 0;
    } else {
      programs_x = BAR_PADDING;
      status_left = programs_x + programs_width + BAR_PADDING;
    }

    x = programs_x;
    if (focused_name_width > 0 &&
        config.bar_focused_name_position == 'l') {
      draw_text(x + focused_name_offset, focused_name);
      x += focused_name_width;
      if (program_list_width > 0)
        x += MODULE_GAP;
    }

    if (program_list_width > 0) {
      int program_list_x = x;
      for (int i = 0; i < limit; i++) {
        if (!clients[i].active)
          continue;
        if (i == current_client) {
          XSetForeground(bar_display, bar_gc, highlight_color.pixel);
          XFillRectangle(bar_display, bar_buffer, bar_gc, x, 0,
                         (unsigned)item_widths[i], (unsigned)bar_height_px);
        }
        format_program_label(label, sizeof(label), i, icons[i]);
        if (i == current_client)
          draw_text_with_color(x + item_text_offsets[i], label,
                               &active_text_color);
        else
          draw_text(x + item_text_offsets[i], label);
        x += item_widths[i] + ITEM_GAP;
      }
      x = program_list_x + program_list_width;
    }

    if (focused_name_width > 0 &&
        config.bar_focused_name_position == 'r') {
      if (program_list_width > 0)
        x += MODULE_GAP;
      draw_text(x + focused_name_offset, focused_name);
    }
  }

  if (status_left < BAR_PADDING)
    status_left = BAR_PADDING;
  if (status_right > bar_width_px - BAR_PADDING)
    status_right = bar_width_px - BAR_PADDING;
  draw_status(status_left, status_right, status_align_right);

  buffer_valid = 1;
  XRaiseWindow(bar_display, bar_window);
  XCopyArea(bar_display, bar_buffer, bar_window, bar_gc, 0, 0,
            (unsigned)bar_width_px, (unsigned)bar_height_px, 0, 0);
  XFlush(bar_display);
}

void bar_handle_expose(const XExposeEvent *event, Client *clients,
                       int max_windows, int current_client) {
  if (!bar_window || event->window != bar_window || event->count != 0)
    return;
  if (!buffer_valid) {
    bar_redraw(clients, max_windows, current_client);
    return;
  }
  XCopyArea(bar_display, bar_buffer, bar_window, bar_gc, 0, 0,
            (unsigned)bar_width_px, (unsigned)bar_height_px, 0, 0);
}

void bar_handle_property(const XPropertyEvent *event, Client *clients,
                         int max_windows, int current_client) {
  if (!bar_window)
    return;
  if (event->window == bar_root && event->atom == XA_WM_NAME) {
    read_root_status();
    bar_redraw(clients, max_windows, current_client);
  } else if (event->atom == XA_WM_CLASS) {
    for (int i = 0; i < max_windows; i++) {
      if (clients[i].active && clients[i].win == event->window) {
        clients[i].bar_icon = NULL;
        clients[i].bar_name[0] = '\0';
        break;
      }
    }
    bar_redraw(clients, max_windows, current_client);
  }
}

int bar_is_visible(void) { return bar_window != None; }

int bar_height(void) { return bar_is_visible() ? bar_height_px : 0; }

void bar_shutdown(void) {
  destroy_bar();
  bar_display = NULL;
  bar_root = None;
}
