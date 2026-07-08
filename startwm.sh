#!/bin/bash

# Display Settings
DISPLAY_OUTPUT="eDP-1"
RESOLUTION="1920x1200"

# Define the idle timeout in seconds (300 seconds = 5 minutes)
IDLE_TIME=300

xset s on
xset s $IDLE_TIME
xset +dpms
xset dpms $IDLE_TIME $IDLE_TIME $IDLE_TIME
xrandr --output $DISPLAY_OUTPUT --mode $RESOLUTION

/run/current-system/sw/libexec/polkit-gnome-authentication-agent-1 &
sxhkd &

exec ~/.local/bin/wm | lemonbar -p -f "JetBrainsMono Nerd Font:size=12"
