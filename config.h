#ifndef CONFIG_H
#define CONFIG_H

#include <X11/X.h>
#include <X11/keysym.h>

// Advanced Compile-time Settings (not configurable at runtime)
#define CLIENT_BG_PREVENT_FLASH 2
#define KEEP_INACTIVE_MAPPED 1

#define MAX_APP_ICONS 64
#define MAX_KEYBINDS 128
#define MAX_AUTOSTARTS 64

typedef struct {
    char name[64];
    char icon[32];
} AppIcon;

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
    char bind_cycle[64];
    int cycle_enabled;
    char bind_switch_window_mod[64];
    char bind_reload[64];
    char bind_toggle_bar[64];

    // Autostart Commands
    char autostarts[MAX_AUTOSTARTS][512];
    int autostart_count;

    // Custom Keybindings
    KeyBind keybinds[MAX_KEYBINDS];
    int keybind_count;

    // Bar Settings
    int bar_enabled;
    int bar_height;
    char bar_font_name[128];
    int bar_font_size;
    int bar_update_interval;
    char bar_position;

    // Bar Appearance
    char bar_color_active_fg[32];
    char bar_color_active_bg[32];
    char bar_color_inactive_fg[32];
    char bar_color_inactive_bg[32];
    char bar_color_bg[32];
    char bar_color_fg[32];
    char bar_color_time_fg[32];
    char bar_color_volume_fg[32];
    char bar_color_battery_fg[32];

    // Bar Modules Setup
    int bar_show_windows;
    char bar_windows_position;

    int bar_show_time;
    char bar_time_position;
    char bar_time_format[64];

    int bar_show_battery;
    char bar_battery_position;
    char bar_battery_path[256];
    char bar_battery_icon_full[32];
    char bar_battery_icon_75[32];
    char bar_battery_icon_50[32];
    char bar_battery_icon_25[32];
    char bar_battery_icon_empty[32];
    char bar_battery_icon_charging[32];

    int bar_show_volume;
    char bar_volume_position;
    char bar_volume_cmd[256];
    char bar_volume_mute_cmd[256];
    char bar_volume_icon_high[32];
    char bar_volume_icon_med[32];
    char bar_volume_icon_low[32];
    char bar_volume_icon_mute[32];

    // App Icons
    char default_icon_str[32];
    AppIcon app_icons[MAX_APP_ICONS];
    int app_icon_count;
} Config;

extern Config config;

int parse_key_combo(const char *combo_str, unsigned int *mods_out, KeySym *keysym_out);
void config_load(void);

#endif
