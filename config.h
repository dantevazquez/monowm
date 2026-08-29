#ifndef CONFIG_H
#define CONFIG_H

#include <X11/X.h>
#include <X11/keysym.h>

#define CLIENT_BG_PREVENT_FLASH 2
#define KEEP_INACTIVE_MAPPED 1

#define MAX_KEYBINDS 128

typedef struct {
    char combo[128];
    char cmd[512];
    unsigned int modifiers;
    KeySym keysym;
} KeyBind;

typedef struct {
    // General Settings
    int max_windows;

    // Internal Window Manager Keybindings
    char bind_quit[64];
    char bind_cycle_forward[64];
    char bind_cycle_back[64];
    char bind_mru_switcher[64];
    char bind_switch_window_mod[64];
    char bind_reload[64];

    // Custom Keybindings
    KeyBind keybinds[MAX_KEYBINDS];
    int keybind_count;

    // Built-in bar (loaded from bar.conf)
    int bar_enabled;
    char bar_text_color[32];
    char bar_active_text_color[32];
    char bar_background_color[32];
    char bar_highlight_color[32];
    char bar_font[128];
    int bar_font_size;
    int bar_vertical_padding;
    char bar_position;
    int bar_show_programs;
    int bar_show_numbers;
    int bar_show_icons;
    int bar_program_padding;
    char bar_programs_position;
    char bar_focused_name_position;

} Config;

extern Config config;

int parse_key_combo(const char *combo_str, unsigned int *mods_out, KeySym *keysym_out);
void config_load(void);

#endif
