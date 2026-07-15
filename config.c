#define _GNU_SOURCE
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

Config config;

static char *trim(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

int parse_key_combo(const char *combo_str, unsigned int *mods_out, KeySym *keysym_out) {
    char buf[128];
    strncpy(buf, combo_str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    unsigned int mods = 0;
    char *token = strtok(buf, "+");
    char *last_token = NULL;
    unsigned int this_mod = 0;

    while (token != NULL) {
        last_token = trim(token);
        this_mod = 0;
        if (strcasecmp(last_token, "super") == 0 || strcasecmp(last_token, "mod4") == 0) {
            this_mod = Mod4Mask;
        } else if (strcasecmp(last_token, "shift") == 0) {
            this_mod = ShiftMask;
        } else if (strcasecmp(last_token, "control") == 0 || strcasecmp(last_token, "ctrl") == 0) {
            this_mod = ControlMask;
        } else if (strcasecmp(last_token, "alt") == 0 || strcasecmp(last_token, "mod1") == 0) {
            this_mod = Mod1Mask;
        }

        char *next_token = strtok(NULL, "+");
        if (next_token != NULL || this_mod != 0) {
            mods |= this_mod;
            token = next_token;
        } else {
            break;
        }
    }

    if (!last_token) return 0;

    if (this_mod != 0) {
        // The last token was a modifier, so the entire combo is modifiers only
        *mods_out = mods;
        *keysym_out = NoSymbol;
        return 1;
    }

    KeySym sym = XStringToKeysym(last_token);
    if (sym == NoSymbol) {
        if (strlen(last_token) == 1) {
            sym = last_token[0];
        } else {
            return 0;
        }
    }

    *mods_out = mods;
    *keysym_out = sym;
    return 1;
}

static void config_set_defaults(void) {
    // config.conf defaults
    config.max_windows = 9;
    strcpy(config.bind_quit, "super+q");
    strcpy(config.bind_cycle, "super+Tab");
    config.cycle_enabled = 0;
    strcpy(config.bind_switch_window_mod, "super+Shift");
    strcpy(config.bind_reload, "super+Shift+r");
    strcpy(config.bind_toggle_bar, "super+Shift+b");
    config.autostart_count = 0;
    config.keybind_count = 0;

    // bar.conf defaults
    config.bar_enabled = 1;
    config.bar_height = 24;
    strcpy(config.bar_font_name, "JetBrainsMono Nerd Font");
    config.bar_font_size = 6;
    config.bar_update_interval = 5;

    strcpy(config.bar_color_active_fg, "#ffffff");
    strcpy(config.bar_color_active_bg, "#555555");
    strcpy(config.bar_color_inactive_fg, "#aaaaaa");
    strcpy(config.bar_color_inactive_bg, "-");

    config.bar_show_windows = 1;
    config.bar_windows_position = 'l';

    config.bar_show_time = 1;
    config.bar_time_position = 'r';
    strcpy(config.bar_time_format, "%I:%M %p");

    config.bar_show_battery = 1;
    config.bar_battery_position = 'r';
    strcpy(config.bar_battery_path, "/sys/class/power_supply/BAT0");
    strcpy(config.bar_battery_icon_full, "");
    strcpy(config.bar_battery_icon_75, "");
    strcpy(config.bar_battery_icon_50, "");
    strcpy(config.bar_battery_icon_25, "");
    strcpy(config.bar_battery_icon_empty, "");
    strcpy(config.bar_battery_icon_charging, "");

    config.bar_show_volume = 0;
    config.bar_volume_position = 'r';
    strcpy(config.bar_volume_cmd, "wpctl get-volume @DEFAULT_AUDIO_SINK@ | awk '{print int($2 * 100)}'");
    strcpy(config.bar_volume_mute_cmd, "wpctl get-volume @DEFAULT_AUDIO_SINK@ | grep -q MUTED && echo \"yes\" || echo \"no\"");
    strcpy(config.bar_volume_icon_high, "󰕾");
    strcpy(config.bar_volume_icon_med, "󰖀");
    strcpy(config.bar_volume_icon_low, "󰕿");
    strcpy(config.bar_volume_icon_mute, "󰝟");

    strcpy(config.default_icon_str, "");
    config.app_icon_count = 0;
}

static void load_config_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char *ptr = trim(line);
        if (ptr[0] == '\0' || ptr[0] == '#') continue;

        char *eq = strchr(ptr, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = trim(ptr);
        char *val = trim(eq + 1);

        // General settings
        if (strcmp(key, "max_windows") == 0) {
            config.max_windows = atoi(val);
            if (config.max_windows < 1) config.max_windows = 1;
        }
        // Binds
        else if (strcmp(key, "bind_quit") == 0) {
            strncpy(config.bind_quit, val, sizeof(config.bind_quit) - 1);
        } else if (strcmp(key, "bind_cycle") == 0) {
            strncpy(config.bind_cycle, val, sizeof(config.bind_cycle) - 1);
        } else if (strcmp(key, "cycle_enabled") == 0) {
            config.cycle_enabled = atoi(val);
        } else if (strcmp(key, "bind_switch_window_mod") == 0) {
            strncpy(config.bind_switch_window_mod, val, sizeof(config.bind_switch_window_mod) - 1);
        } else if (strcmp(key, "bind_reload") == 0) {
            strncpy(config.bind_reload, val, sizeof(config.bind_reload) - 1);
        } else if (strcmp(key, "bind_toggle_bar") == 0) {
            strncpy(config.bind_toggle_bar, val, sizeof(config.bind_toggle_bar) - 1);
        }
        // Run commands
        else if (strcmp(key, "run") == 0) {
            if (config.autostart_count < MAX_AUTOSTARTS) {
                strncpy(config.autostarts[config.autostart_count], val, sizeof(config.autostarts[config.autostart_count]) - 1);
                config.autostart_count++;
            }
        }
        // Keybinds
        else if (strcmp(key, "keybind") == 0) {
            char *colon = strchr(val, ':');
            if (colon && config.keybind_count < MAX_KEYBINDS) {
                *colon = '\0';
                char *combo = trim(val);
                char *cmd = trim(colon + 1);
                unsigned int mods;
                KeySym sym;
                if (parse_key_combo(combo, &mods, &sym)) {
                    strncpy(config.keybinds[config.keybind_count].combo, combo, sizeof(config.keybinds[config.keybind_count].combo) - 1);
                    strncpy(config.keybinds[config.keybind_count].cmd, cmd, sizeof(config.keybinds[config.keybind_count].cmd) - 1);
                    config.keybinds[config.keybind_count].modifiers = mods;
                    config.keybinds[config.keybind_count].keysym = sym;
                    config.keybind_count++;
                }
            }
        }
        // Bar Settings
        else if (strcmp(key, "bar_enabled") == 0) {
            config.bar_enabled = atoi(val);
        } else if (strcmp(key, "bar_height") == 0) {
            config.bar_height = atoi(val);
        } else if (strcmp(key, "bar_font_name") == 0) {
            strncpy(config.bar_font_name, val, sizeof(config.bar_font_name) - 1);
        } else if (strcmp(key, "bar_font_size") == 0) {
            config.bar_font_size = atoi(val);
        } else if (strcmp(key, "bar_update_interval") == 0) {
            config.bar_update_interval = atoi(val);
        }
        // Bar Colors
        else if (strcmp(key, "bar_color_active_fg") == 0) {
            strncpy(config.bar_color_active_fg, val, sizeof(config.bar_color_active_fg) - 1);
        } else if (strcmp(key, "bar_color_active_bg") == 0) {
            strncpy(config.bar_color_active_bg, val, sizeof(config.bar_color_active_bg) - 1);
        } else if (strcmp(key, "bar_color_inactive_fg") == 0) {
            strncpy(config.bar_color_inactive_fg, val, sizeof(config.bar_color_inactive_fg) - 1);
        } else if (strcmp(key, "bar_color_inactive_bg") == 0) {
            strncpy(config.bar_color_inactive_bg, val, sizeof(config.bar_color_inactive_bg) - 1);
        }
        // Bar Modules
        else if (strcmp(key, "bar_show_windows") == 0) {
            config.bar_show_windows = atoi(val);
        } else if (strcmp(key, "bar_windows_position") == 0) {
            config.bar_windows_position = val[0];
        } else if (strcmp(key, "bar_show_time") == 0) {
            config.bar_show_time = atoi(val);
        } else if (strcmp(key, "bar_time_position") == 0) {
            config.bar_time_position = val[0];
        } else if (strcmp(key, "bar_time_format") == 0) {
            strncpy(config.bar_time_format, val, sizeof(config.bar_time_format) - 1);
        } else if (strcmp(key, "bar_show_battery") == 0) {
            config.bar_show_battery = atoi(val);
        } else if (strcmp(key, "bar_battery_position") == 0) {
            config.bar_battery_position = val[0];
        } else if (strcmp(key, "bar_battery_path") == 0) {
            strncpy(config.bar_battery_path, val, sizeof(config.bar_battery_path) - 1);
        } else if (strcmp(key, "bar_battery_icon_full") == 0) {
            strncpy(config.bar_battery_icon_full, val, sizeof(config.bar_battery_icon_full) - 1);
        } else if (strcmp(key, "bar_battery_icon_75") == 0) {
            strncpy(config.bar_battery_icon_75, val, sizeof(config.bar_battery_icon_75) - 1);
        } else if (strcmp(key, "bar_battery_icon_50") == 0) {
            strncpy(config.bar_battery_icon_50, val, sizeof(config.bar_battery_icon_50) - 1);
        } else if (strcmp(key, "bar_battery_icon_25") == 0) {
            strncpy(config.bar_battery_icon_25, val, sizeof(config.bar_battery_icon_25) - 1);
        } else if (strcmp(key, "bar_battery_icon_empty") == 0) {
            strncpy(config.bar_battery_icon_empty, val, sizeof(config.bar_battery_icon_empty) - 1);
        } else if (strcmp(key, "bar_battery_icon_charging") == 0) {
            strncpy(config.bar_battery_icon_charging, val, sizeof(config.bar_battery_icon_charging) - 1);
        } else if (strcmp(key, "bar_show_volume") == 0) {
            config.bar_show_volume = atoi(val);
        } else if (strcmp(key, "bar_volume_position") == 0) {
            config.bar_volume_position = val[0];
        } else if (strcmp(key, "bar_volume_cmd") == 0) {
            strncpy(config.bar_volume_cmd, val, sizeof(config.bar_volume_cmd) - 1);
        } else if (strcmp(key, "bar_volume_mute_cmd") == 0) {
            strncpy(config.bar_volume_mute_cmd, val, sizeof(config.bar_volume_mute_cmd) - 1);
        } else if (strcmp(key, "bar_volume_icon_high") == 0) {
            strncpy(config.bar_volume_icon_high, val, sizeof(config.bar_volume_icon_high) - 1);
        } else if (strcmp(key, "bar_volume_icon_med") == 0) {
            strncpy(config.bar_volume_icon_med, val, sizeof(config.bar_volume_icon_med) - 1);
        } else if (strcmp(key, "bar_volume_icon_low") == 0) {
            strncpy(config.bar_volume_icon_low, val, sizeof(config.bar_volume_icon_low) - 1);
        } else if (strcmp(key, "bar_volume_icon_mute") == 0) {
            strncpy(config.bar_volume_icon_mute, val, sizeof(config.bar_volume_icon_mute) - 1);
        }
        // App icon mappings
        else if (strcmp(key, "default_icon") == 0) {
            strncpy(config.default_icon_str, val, sizeof(config.default_icon_str) - 1);
        } else if (strcmp(key, "icon") == 0) {
            char *colon = strchr(val, ':');
            if (colon && config.app_icon_count < MAX_APP_ICONS) {
                *colon = '\0';
                char *app_name = trim(val);
                char *app_icon = trim(colon + 1);
                strncpy(config.app_icons[config.app_icon_count].name, app_name, sizeof(config.app_icons[config.app_icon_count].name) - 1);
                strncpy(config.app_icons[config.app_icon_count].icon, app_icon, sizeof(config.app_icons[config.app_icon_count].icon) - 1);
                config.app_icon_count++;
            }
        }
    }
    fclose(f);
}

void config_load(void) {
    config_set_defaults();

    char config_path[1024];
    char bar_path[1024];
    const char *xdg_config = getenv("XDG_CONFIG_HOME");
    if (xdg_config) {
        snprintf(config_path, sizeof(config_path), "%s/monowm/config.conf", xdg_config);
        snprintf(bar_path, sizeof(bar_path), "%s/monowm/bar.conf", xdg_config);
    } else {
        const char *home = getenv("HOME");
        snprintf(config_path, sizeof(config_path), "%s/.config/monowm/config.conf", home ? home : "");
        snprintf(bar_path, sizeof(bar_path), "%s/.config/monowm/bar.conf", home ? home : "");
    }

    load_config_file(config_path);
    load_config_file(bar_path);

    // Fallback: If no icons were parsed at all, load defaults
    if (config.app_icon_count == 0) {
        config.app_icon_count = 5;
        strcpy(config.app_icons[0].name, "firefox");
        strcpy(config.app_icons[0].icon, "");
        strcpy(config.app_icons[1].name, "st");
        strcpy(config.app_icons[1].icon, "");
        strcpy(config.app_icons[2].name, "alacritty");
        strcpy(config.app_icons[2].icon, "");
        strcpy(config.app_icons[3].name, "kitty");
        strcpy(config.app_icons[3].icon, "");
        strcpy(config.app_icons[4].name, "chromium");
        strcpy(config.app_icons[4].icon, "");
    }
}
