#ifndef CONFIG_H
#define CONFIG_H

#include <X11/X.h>

// General Settings
#define BAR_ENABLED 1
#define MAX_WINDOWS 9
#define BAR_HEIGHT 24
#define MOD_KEY Mod4Mask

// Bar Appearance
#define BAR_COLOR_ACTIVE_FG "#ffffff"
#define BAR_COLOR_ACTIVE_BG "#555555"
#define BAR_COLOR_INACTIVE_FG "#aaaaaa"
#define BAR_COLOR_INACTIVE_BG "-"

// Bar Update Interval (seconds) - how often time/battery refresh
#define BAR_UPDATE_INTERVAL 5

// ============================================================
// Bar Module Configuration
// ============================================================
// Each module can be enabled/disabled and positioned.
// Positions: 'l' = left, 'c' = center, 'r' = right
// ============================================================

// --- Windows Module (app icons) ---
#define BAR_SHOW_WINDOWS 1
#define BAR_WINDOWS_POSITION 'l'

// --- Time Module ---
#define BAR_SHOW_TIME 1
#define BAR_TIME_POSITION 'r'
#define BAR_TIME_FORMAT "%I:%M %p"  // strftime format (e.g. "03:45 PM")

// --- Battery Module ---
#define BAR_SHOW_BATTERY 1
#define BAR_BATTERY_POSITION 'r'
#define BAR_BATTERY_PATH "/sys/class/power_supply/BAT0"

// Battery Icons (Font Awesome - Nerd Font)
#define BAR_BATTERY_ICON_FULL ""
#define BAR_BATTERY_ICON_75 ""
#define BAR_BATTERY_ICON_50 ""
#define BAR_BATTERY_ICON_25 ""
#define BAR_BATTERY_ICON_EMPTY ""
#define BAR_BATTERY_ICON_CHARGING ""

// --- Volume Module ---
#define BAR_SHOW_VOLUME 1
#define BAR_VOLUME_POSITION 'r'

// Command that prints volume percentage as a number (0-100) to stdout
#define BAR_VOLUME_CMD "wpctl get-volume @DEFAULT_AUDIO_SINK@ | awk '{print int($2 * 100)}'"
// Command that prints "yes" if muted, anything else if not
#define BAR_VOLUME_MUTE_CMD "wpctl get-volume @DEFAULT_AUDIO_SINK@ | grep -q MUTED && echo \"yes\" || echo \"no\""

// Volume Icons (Nerd Font)
#define BAR_VOLUME_ICON_HIGH "󰕾"
#define BAR_VOLUME_ICON_MED "󰖀"
#define BAR_VOLUME_ICON_LOW "󰕿"
#define BAR_VOLUME_ICON_MUTE "󰝟"

// Icon Configuration
typedef struct {
    const char *name;
    const char *icon;
} AppIcon;

static const char __attribute__((unused)) *DEFAULT_ICON_STR = "󰣆";

static const AppIcon __attribute__((unused)) APP_ICONS[] = {
    {"firefox", ""},
    {"st", ""},
    {"alacritty", ""},
    {"chromium", ""},
    {NULL, NULL} // Terminator
};

#endif
