#define _GNU_SOURCE
#include "config.h"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

Config config;

static char *trim(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == '\0') return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static char *find_comment(char *str) {
    int in_single = 0;
    int in_double = 0;
    int escaped = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (escaped) {
            escaped = 0;
            continue;
        }
        if (str[i] == '\\') {
            escaped = 1;
            continue;
        }
        if (str[i] == '\'' && !in_double) {
            in_single = !in_single;
        } else if (str[i] == '"' && !in_single) {
            in_double = !in_double;
        } else if (str[i] == '#' && !in_single && !in_double) {
            return &str[i];
        }
    }
    return NULL;
}

int parse_key_combo(const char *combo_str, unsigned int *mods_out,
                    KeySym *keysym_out) {
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
        if (strcasecmp(last_token, "super") == 0 ||
            strcasecmp(last_token, "mod4") == 0) {
            this_mod = Mod4Mask;
        } else if (strcasecmp(last_token, "shift") == 0) {
            this_mod = ShiftMask;
        } else if (strcasecmp(last_token, "control") == 0 ||
                   strcasecmp(last_token, "ctrl") == 0) {
            this_mod = ControlMask;
        } else if (strcasecmp(last_token, "alt") == 0 ||
                   strcasecmp(last_token, "mod1") == 0) {
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
    memset(&config, 0, sizeof(config));
    config.max_windows = 9;
#ifndef NO_BAR
    config.bar_enabled = 1;
    strcpy(config.bar_text_color, "#c6d0f5");
    strcpy(config.bar_active_text_color, "#c6d0f5");
    strcpy(config.bar_background_color, "#303446");
    strcpy(config.bar_highlight_color, "#626880");
    strcpy(config.bar_font, "JetBrainsMono Nerd Font");
    config.bar_font_size = 14;
    config.bar_vertical_padding = 5;
    config.bar_position = 't';
    config.bar_show_programs = 1;
    config.bar_show_numbers = 1;
    config.bar_show_icons = 1;
    config.bar_program_padding = 7;
    config.bar_programs_position = 'l';
    config.bar_focused_name_position = 'r';
#endif
}

#ifndef NO_BAR
static int parse_bool(const char *value) {
    return strcmp(value, "1") == 0 || strcasecmp(value, "true") == 0 ||
           strcasecmp(value, "yes") == 0 || strcasecmp(value, "on") == 0;
}

static void load_bar_config_file(const char *path) {
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

        // A leading '#' is a hex color, not a comment.
        if (val[0] != '#') {
            char *comment = find_comment(val);
            if (comment) {
                *comment = '\0';
                val = trim(val);
            }
        }

        if (strcmp(key, "enabled") == 0 ||
            strcmp(key, "bar_enabled") == 0) {
            config.bar_enabled = parse_bool(val);
        } else if (strcmp(key, "text_color") == 0 ||
                   strcmp(key, "bar_text_color") == 0 ||
                   strcmp(key, "bar_color_fg") == 0) {
            strncpy(config.bar_text_color, val,
                    sizeof(config.bar_text_color) - 1);
        } else if (strcmp(key, "active_text_color") == 0 ||
                   strcmp(key, "highlight_text_color") == 0 ||
                   strcmp(key, "bar_active_text_color") == 0 ||
                   strcmp(key, "bar_color_active_fg") == 0) {
            strncpy(config.bar_active_text_color, val,
                    sizeof(config.bar_active_text_color) - 1);
        } else if (strcmp(key, "background_color") == 0 ||
                   strcmp(key, "bar_background_color") == 0 ||
                   strcmp(key, "bar_color_bg") == 0) {
            strncpy(config.bar_background_color, val,
                    sizeof(config.bar_background_color) - 1);
        } else if (strcmp(key, "highlight_color") == 0 ||
                   strcmp(key, "bar_highlight_color") == 0 ||
                   strcmp(key, "bar_color_active_bg") == 0) {
            strncpy(config.bar_highlight_color, val,
                    sizeof(config.bar_highlight_color) - 1);
        } else if (strcmp(key, "font") == 0 ||
                   strcmp(key, "bar_font") == 0 ||
                   strcmp(key, "bar_font_name") == 0) {
            strncpy(config.bar_font, val, sizeof(config.bar_font) - 1);
        } else if (strcmp(key, "font_size") == 0 ||
                   strcmp(key, "bar_font_size") == 0) {
            config.bar_font_size = atoi(val);
            if (config.bar_font_size < 6) config.bar_font_size = 6;
            if (config.bar_font_size > 64) config.bar_font_size = 64;
        } else if (strcmp(key, "vertical_padding") == 0 ||
                   strcmp(key, "bar_vertical_padding") == 0) {
            config.bar_vertical_padding = atoi(val);
            if (config.bar_vertical_padding < 0)
                config.bar_vertical_padding = 0;
            if (config.bar_vertical_padding > 64)
                config.bar_vertical_padding = 64;
        } else if (strcmp(key, "position") == 0 ||
                   strcmp(key, "bar_position") == 0) {
            config.bar_position = strcasecmp(val, "bottom") == 0 ? 'b' : 't';
        } else if (strcmp(key, "programs") == 0 ||
                   strcmp(key, "programs_position") == 0 ||
                   strcmp(key, "bar_programs_position") == 0 ||
                   strcmp(key, "bar_windows_position") == 0) {
            if (strcasecmp(val, "hidden") == 0 ||
                strcasecmp(val, "hide") == 0 ||
                strcasecmp(val, "none") == 0 ||
                strcasecmp(val, "off") == 0) {
                config.bar_show_programs = 0;
            } else if (strcasecmp(val, "middle") == 0 ||
                       strcasecmp(val, "center") == 0 || val[0] == 'c') {
                config.bar_show_programs = 1;
                config.bar_programs_position = 'c';
            } else if (strcasecmp(val, "right") == 0 || val[0] == 'r') {
                config.bar_show_programs = 1;
                config.bar_programs_position = 'r';
            } else {
                config.bar_show_programs = 1;
                config.bar_programs_position = 'l';
            }
        } else if (strcmp(key, "show_programs") == 0 ||
                   strcmp(key, "bar_show_windows") == 0) {
            config.bar_show_programs = parse_bool(val);
        } else if (strcmp(key, "show_numbers") == 0 ||
                   strcmp(key, "bar_show_numbers") == 0) {
            config.bar_show_numbers = parse_bool(val);
        } else if (strcmp(key, "show_icons") == 0 ||
                   strcmp(key, "bar_show_icons") == 0) {
            config.bar_show_icons = parse_bool(val);
        } else if (strcmp(key, "program_padding") == 0 ||
                   strcmp(key, "item_padding") == 0 ||
                   strcmp(key, "bar_program_padding") == 0) {
            config.bar_program_padding = atoi(val);
            if (config.bar_program_padding < 0)
                config.bar_program_padding = 0;
            if (config.bar_program_padding > 64)
                config.bar_program_padding = 64;
        } else if (strcmp(key, "focused_name") == 0 ||
                   strcmp(key, "focused_name_position") == 0 ||
                   strcmp(key, "bar_focused_name_position") == 0) {
            if (strcasecmp(val, "hidden") == 0 ||
                strcasecmp(val, "hide") == 0 ||
                strcasecmp(val, "none") == 0 ||
                strcasecmp(val, "off") == 0 || strcmp(val, "0") == 0) {
                config.bar_focused_name_position = 'h';
            } else if (strcasecmp(val, "left") == 0 || val[0] == 'l') {
                config.bar_focused_name_position = 'l';
            } else {
                config.bar_focused_name_position = 'r';
            }
        } else if (strcmp(key, "show_focused_name") == 0) {
            if (!parse_bool(val)) {
                config.bar_focused_name_position = 'h';
            } else if (config.bar_focused_name_position == 'h') {
                config.bar_focused_name_position = 'r';
            }
        }
    }
    fclose(f);
}
#endif

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

        char *comment = find_comment(val);
        if (comment) {
            *comment = '\0';
            val = trim(val);
        }

        if (strcmp(key, "max_windows") == 0) {
            config.max_windows = atoi(val);
            if (config.max_windows < 1) config.max_windows = 1;
        } else if (strcmp(key, "bind_quit") == 0) {
            strncpy(config.bind_quit, val, sizeof(config.bind_quit) - 1);
        } else if (strcmp(key, "bind_cycle_forward") == 0) {
            strncpy(config.bind_cycle_forward, val,
                    sizeof(config.bind_cycle_forward) - 1);
        } else if (strcmp(key, "bind_cycle_back") == 0) {
            strncpy(config.bind_cycle_back, val,
                    sizeof(config.bind_cycle_back) - 1);
        } else if (strcmp(key, "bind_mru_switcher") == 0 ||
                   strcmp(key, "bind_window_switcher") == 0) {
            strncpy(config.bind_mru_switcher, val,
                    sizeof(config.bind_mru_switcher) - 1);
        } else if (strcmp(key, "bind_switch_window_mod") == 0) {
            strncpy(config.bind_switch_window_mod, val,
                    sizeof(config.bind_switch_window_mod) - 1);
        } else if (strcmp(key, "bind_reload") == 0) {
            strncpy(config.bind_reload, val, sizeof(config.bind_reload) - 1);
        } else if (strcmp(key, "keybind") == 0) {
            char *colon = strchr(val, ':');
            if (colon && config.keybind_count < MAX_KEYBINDS) {
                *colon = '\0';
                char *combo = trim(val);
                char *cmd = trim(colon + 1);
                KeyBind *binding = &config.keybinds[config.keybind_count];
                if (parse_key_combo(combo, &binding->modifiers,
                                    &binding->keysym)) {
                    strncpy(binding->combo, combo, sizeof(binding->combo) - 1);
                    strncpy(binding->cmd, cmd, sizeof(binding->cmd) - 1);
                    config.keybind_count++;
                }
            }
        }
    }
    fclose(f);
}

void config_load(void) {
    config_set_defaults();

    char config_path[1024];
#ifndef NO_BAR
    char bar_path[1024];
#endif
    const char *xdg_config = getenv("XDG_CONFIG_HOME");
    if (xdg_config) {
        snprintf(config_path, sizeof(config_path), "%s/monowm/config.conf",
                 xdg_config);
#ifndef NO_BAR
        snprintf(bar_path, sizeof(bar_path), "%s/monowm/bar.conf", xdg_config);
#endif
    } else {
        const char *home = getenv("HOME");
        snprintf(config_path, sizeof(config_path), "%s/.config/monowm/config.conf",
                 home ? home : "");
#ifndef NO_BAR
        snprintf(bar_path, sizeof(bar_path), "%s/.config/monowm/bar.conf",
                 home ? home : "");
#endif
    }
    load_config_file(config_path);
#ifndef NO_BAR
    load_bar_config_file(bar_path);
#endif
}
