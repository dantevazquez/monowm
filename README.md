# Monowm 🙉

Lightweight window manger for x that uses under 3mb of ram. This window manager follows the mobile workflow where one app/window always occupies the entire screen.

## How to install

### Inside tty, do the following

1. Install build dependencies: libx11, a compiler like gcc, make, and pkg-config. If youre on nix, you can run nix-shell to build.
2. Install recommened dependencies: sxhkd (Needed to use keybinds), xorg-server (or another X server)
3. Clone using git clone https://github.com/dantevazquez/monowm.git
4. Run make install
5. Run startx

Optional dependencies:
* `alttab` (to switch tabs)
* `dmenu` (to launch applications)
* `pipewire` (for volume)
* `brightnessctl` (to control screen brightness)
* `dunst` (to see volume changes and low battery notifications)
* lemonbarxft (Built in bar)
* A nerdfont of your choice (for the bar)

## Configuration

In config.h, you may configure the followng:

| Configuration Macro | Value | Description |
| :--- | :--- | :--- |
| `BAR_ENABLED` | `1` | Enables (`1`) or disables (`0`) the status bar. |
| `MAX_WINDOWS` | `9` | Specifies the maximum number of windows supported or displayed. |
| `BAR_HEIGHT` | `24` | Sets the height of the status bar in pixels. |
| `MOD_KEY` | `Mod4Mask` | Defines the modifier key used for window manager shortcuts. `Mod4Mask` corresponds to the **Super / Windows** key. |
| `KEY_QUIT` | `XK_q` | Keybinding to quit the window manager (associated with the `q` key). |
| `KEY_SWITCHER` | `XK_Tab` | Keybinding used for the internal window switcher (associated with the `Tab` key). Only functional if `WINDOW_SWITCHER_ENABLED` is set to `1`. |
| `WINDOW_SWITCHER_ENABLED` | `0` | Controls the internal Alt-Tab window switcher.<br>• `1`: Enabled (WM grabs and handles `KEY_SWITCHER`).<br>• `0`: Disabled (allows external tools like `sxhkd` or `alttab` to grab the key). |

If you wish to use the included bar it can also be configured in config.h, the code explains how. Once changes are made, compile with make install and restart the window manger using pkill monowm

To add application icons for the bar edit the following

```c
//Add icons for your programs here
static const AppIcon __attribute__((unused)) APP_ICONS[] = {
    {"firefox", ""},
    {"st", ""},
    {"alacritty", ""},
    {"kitty", ""},
    {"chromium", ""},
    {NULL, NULL} // Terminator
};
```

In .config/sxhkd/sxhkdrc you can edit keybinds. Here are the default:

```ini
# Terminal emulator
super + Return
    alacritty

# App launcher
super + space
    dmenu_run -fn 'monospace-14'

# Web browser
super + b
    chromium

# Volume Control (using custom script for notification popups)
XF86AudioRaiseVolume
    ~/.local/bin/monowm-volume up

XF86AudioLowerVolume
    ~/.local/bin/monowm-volume down

XF86AudioMute
    ~/.local/bin/monowm-volume mute

# Mic Mute
XF86AudioMicMute
    wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle

# Brightness Control
XF86MonBrightnessUp
    brightnessctl set +5%

XF86MonBrightnessDown
    brightnessctl set 5%-
```

To add startup programs and change resolution, add them in ~/.config/monowm/autostart

```bash
#!/bin/bash

# Display Settings
DISPLAY_OUTPUT="eDP-1"
RESOLUTION="1920x1200"

# Define the idle timeout in seconds (300 seconds = 5 minutes)
IDLE_TIME=300

# auto starts
xset s on
xset s $IDLE_TIME
xset +dpms
xset dpms $IDLE_TIME $IDLE_TIME $IDLE_TIME
xrandr --output $DISPLAY_OUTPUT --mode $RESOLUTION
eval $(gnome-keyring-daemon --start --components=pkcs11,secrets,ssh)
export SSH_AUTH_SOCK
export SECRET_SERVICE_SOCK
/run/current-system/sw/libexec/polkit-gnome-authentication-agent-1 &
sxhkd &
alttab -w 1 -mk Super_L &
dbus-update-activation-environment --systemd --all
dunst &
# add more startup commands here
```
