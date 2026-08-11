# monowm - simple window manager Makefile

-include config.mk

NOBAR ?= 0
NOSWITCHER ?= 0

ifneq ($(filter-out 0 1,$(NOBAR)),)
$(error NOBAR must be either 0 or 1)
endif
ifneq ($(filter-out 0 1,$(NOSWITCHER)),)
$(error NOSWITCHER must be either 0 or 1)
endif

# Paths
PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin
SESSIONDIR ?= $(PREFIX)/share/xsessions

# Tools
CC ?= gcc
PKG_CONFIG ?= pkg-config

PACKAGES = x11
FALLBACK_LDFLAGS = -L/usr/X11R6/lib -lX11
ifeq ($(NOSWITCHER),0)
PACKAGES += xcomposite xrender xft
FALLBACK_LDFLAGS += -lXcomposite -lXrender -lXft
endif

# Compiler/Linker Flags
CFLAGS += -std=c99 -Wall -Wextra -O2
CFLAGS += $(shell $(PKG_CONFIG) --cflags $(PACKAGES) 2>/dev/null || echo -I/usr/X11R6/include -I/usr/include/freetype2)
LDFLAGS += $(shell $(PKG_CONFIG) --libs $(PACKAGES) 2>/dev/null || echo $(FALLBACK_LDFLAGS))
ifeq ($(NOBAR),0)
LDFLAGS += -lpthread
endif

ifeq ($(NOBAR),1)
FEATURE_CFLAGS += -DNO_BAR
endif
ifeq ($(NOSWITCHER),1)
FEATURE_CFLAGS += -DNO_SWITCHER
endif

BUILD_VARIANT = $(if $(filter 1,$(NOBAR)),nobar,bar)-$(if $(filter 1,$(NOSWITCHER)),noswitcher,switcher)
BUILDDIR = .build/$(BUILD_VARIANT)

# Target and Sources
TARGET = monowm
SRCS = main.c keys.c config.c
ifeq ($(NOBAR),0)
SRCS += appicons.c bar.c
endif
ifeq ($(NOSWITCHER),0)
SRCS += window_switcher.c
endif
OBJS = $(SRCS:%.c=$(BUILDDIR)/%.o)
VARIANT_TARGET = $(BUILDDIR)/$(TARGET)
HEADERS = appicons.h config.h bar.h keys.h window_switcher.h wm.h

LEMONBAR_PACKAGES = xcb xcb-xinerama xcb-randr x11 x11-xcb xft freetype2 fontconfig
LEMONBAR_CFLAGS = -std=c99 -Wall -Wextra -O2 -DVERSION="\"1.4-xft\"" $(shell $(PKG_CONFIG) --cflags $(LEMONBAR_PACKAGES) 2>/dev/null || echo -I/usr/include/freetype2)
LEMONBAR_LDFLAGS = $(shell $(PKG_CONFIG) --libs $(LEMONBAR_PACKAGES) 2>/dev/null || echo -lxcb -lxcb-xinerama -lxcb-randr -lX11 -lX11-xcb -lXft -lfreetype -lz -lfontconfig)

ALL_TARGETS = $(TARGET)
ifeq ($(NOBAR),0)
ALL_TARGETS += lemonbar
endif

# Rules
all: $(ALL_TARGETS)

$(TARGET): FORCE $(VARIANT_TARGET)
	cp $(VARIANT_TARGET) $@

$(VARIANT_TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

lemonbar: lemonbar.c
	$(CC) $(LEMONBAR_CFLAGS) lemonbar.c -o lemonbar $(LEMONBAR_LDFLAGS)

$(BUILDDIR)/%.o: %.c $(HEADERS) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(FEATURE_CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $@

clean:
	rm -f $(TARGET) lemonbar *.o
	rm -rf .build

install: $(ALL_TARGETS)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
ifeq ($(NOBAR),0)
	install -m 755 lemonbar $(DESTDIR)$(BINDIR)/lemonbar
endif
	install -m 755 monowm-start $(DESTDIR)$(BINDIR)/monowm-start
	install -m 755 monowm-volume $(DESTDIR)$(BINDIR)/monowm-volume
	install -m 755 monowm-brightness $(DESTDIR)$(BINDIR)/monowm-brightness
	install -d $(DESTDIR)$(SESSIONDIR)
	install -m 644 monowm.desktop $(DESTDIR)$(SESSIONDIR)/monowm.desktop
	install -d $(HOME)/.config/monowm
	test -f $(HOME)/.config/monowm/autostart || install -m 755 autostart $(HOME)/.config/monowm/autostart
	test -f $(HOME)/.config/monowm/config.conf || install -m 644 templates/config.conf $(HOME)/.config/monowm/config.conf
ifeq ($(NOBAR),0)
	test -f $(HOME)/.config/monowm/bar.conf || install -m 644 templates/bar.conf $(HOME)/.config/monowm/bar.conf
endif
ifeq ($(NOSWITCHER),0)
	test -f $(HOME)/.config/monowm/switcher.conf || install -m 644 templates/switcher.conf $(HOME)/.config/monowm/switcher.conf
endif
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

FORCE:

.PHONY: all clean install uninstall FORCE
