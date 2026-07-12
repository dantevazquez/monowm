#ifndef CONFIG_H
#define CONFIG_H

#include <X11/X.h>
#include <X11/keysym.h>

// General Settings
#define BAR_ENABLED 0 // 1 to enable bar, 0 to disable
#define MAX_WINDOWS 9 //Number of windows that can be open at the same time
#define BAR_HEIGHT 60 //Height of lemonbar
#define MOD_KEY Mod4Mask //Super

// Keybindings Configuration
#define KEY_QUIT XK_q

//For the window switcher. Will only work if WINDOW_SWITCHER_ENABLED = 1
#define KEY_SWITCHER XK_Tab

// Window Switcher Configuration (Internal Alt-Tab)
// 1 = Enabled (WM grabs and handles KEY_SWITCHER)
// 0 = Disabled (allows external tools like sxhkd/alttab to grab KEY_SWITCHER)
#define WINDOW_SWITCHER_ENABLED 0

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
#define BAR_SHOW_VOLUME 0
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

// Prevent bright white flash when opening windows (I recommend not to change)
// 0 = Disabled
// 1 = Solid black background
// 2 = Undefined background (None) - keeps previous window content visible until client draws
#define CLIENT_BG_PREVENT_FLASH 2

// Keep inactive windows mapped in the background to prevent black flashes when closing windows
// I recommend not to change
#define KEEP_INACTIVE_MAPPED 1

// Icon Configuration
typedef struct {
    const char *name;
    const char *icon;
} AppIcon;

static const char __attribute__((unused)) *DEFAULT_ICON_STR = "";

//Add icons for your programs here
static const AppIcon __attribute__((unused)) APP_ICONS[] = {
    {"firefox", ""},
    {"st", ""},
    {"alacritty", ""},
    {"kitty", ""},
    {"chromium", ""},
    {NULL, NULL} // Terminator
};

#endif
