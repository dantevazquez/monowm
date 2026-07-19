# wm - simple window manager Makefile

# Paths
PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin
SESSIONDIR ?= $(PREFIX)/share/xsessions

# Tools
CC ?= gcc
PKG_CONFIG ?= pkg-config

# Find dependencies using pkg-config
PACKAGES = x11

# Compiler/Linker Flags
CFLAGS += -std=c99 -Wall -Wextra -O2
CFLAGS += $(shell $(PKG_CONFIG) --cflags $(PACKAGES) 2>/dev/null || echo -I/usr/X11R6/include)
LDFLAGS += $(shell $(PKG_CONFIG) --libs $(PACKAGES) 2>/dev/null || echo -L/usr/X11R6/lib -lX11 -lXft -lXinerama -lXext) -lpthread

# Target and Sources
TARGET = monowm
SRCS = main.c appicons.c bar.c keys.c config.c
OBJS = $(SRCS:.c=.o)
HEADERS = appicons.h config.h bar.h keys.h

LEMONBAR_PACKAGES = xcb xcb-xinerama xcb-randr x11 x11-xcb xft freetype2 fontconfig
LEMONBAR_CFLAGS = -std=c99 -Wall -Wextra -O2 -DVERSION="\"1.4-xft\"" $(shell $(PKG_CONFIG) --cflags $(LEMONBAR_PACKAGES) 2>/dev/null || echo -I/usr/include/freetype2)
LEMONBAR_LDFLAGS = $(shell $(PKG_CONFIG) --libs $(LEMONBAR_PACKAGES) 2>/dev/null || echo -lxcb -lxcb-xinerama -lxcb-randr -lX11 -lX11-xcb -lXft -lfreetype -lz -lfontconfig)

# Rules
all: $(TARGET) lemonbar

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

lemonbar: lemonbar.c
	$(CC) $(LEMONBAR_CFLAGS) lemonbar.c -o lemonbar $(LEMONBAR_LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) lemonbar

install: $(TARGET) lemonbar
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -m 755 lemonbar $(DESTDIR)$(BINDIR)/lemonbar
	install -m 755 monowm-start $(DESTDIR)$(BINDIR)/monowm-start
	install -m 755 monowm-volume $(DESTDIR)$(BINDIR)/monowm-volume
	install -m 755 monowm-brightness $(DESTDIR)$(BINDIR)/monowm-brightness
	install -d $(DESTDIR)$(SESSIONDIR)
	install -m 644 monowm.desktop $(DESTDIR)$(SESSIONDIR)/monowm.desktop
	install -d $(HOME)/.config/monowm
	test -f $(HOME)/.config/monowm/autostart || install -m 755 autostart $(HOME)/.config/monowm/autostart
	test -f $(HOME)/.config/monowm/config.conf || install -m 644 templates/config.conf $(HOME)/.config/monowm/config.conf
	test -f $(HOME)/.config/monowm/bar.conf || install -m 644 templates/bar.conf $(HOME)/.config/monowm/bar.conf
	test -f $(HOME)/.config/monowm/bg.png || install -m 644 bg.png $(HOME)/.config/monowm/bg.png
	echo '#!/bin/sh' > $(HOME)/.xinitrc
	echo 'exec $(BINDIR)/monowm-start' >> $(HOME)/.xinitrc
	chmod +x $(HOME)/.xinitrc

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(BINDIR)/lemonbar
	rm -f $(DESTDIR)$(BINDIR)/monowm-start
	rm -f $(DESTDIR)$(BINDIR)/monowm-volume
	rm -f $(DESTDIR)$(BINDIR)/monowm-brightness
	rm -f $(DESTDIR)$(SESSIONDIR)/monowm.desktop

.PHONY: all clean install uninstall
