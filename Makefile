# monowm - simple window manager Makefile

-include config.mk

NOBAR ?= 0

ifneq ($(filter-out 0 1,$(NOBAR)),)
$(error NOBAR must be either 0 or 1)
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
ifeq ($(NOBAR),0)
PACKAGES += xft
FALLBACK_LDFLAGS += -lXft
else
FEATURE_CFLAGS += -DNO_BAR
endif

# Compiler/Linker Flags
CFLAGS += -std=c99 -Wall -Wextra -O2
CFLAGS += $(shell $(PKG_CONFIG) --cflags $(PACKAGES) 2>/dev/null || echo -I/usr/X11R6/include -I/usr/include/freetype2)
LDFLAGS += $(shell $(PKG_CONFIG) --libs $(PACKAGES) 2>/dev/null || echo $(FALLBACK_LDFLAGS))

BUILD_VARIANT = $(if $(filter 1,$(NOBAR)),nobar,bar)
BUILDDIR = .build/$(BUILD_VARIANT)

# Target and Sources
TARGET = monowm
SRCS = main.c keys.c config.c
ifeq ($(NOBAR),0)
SRCS += bar.c
endif
OBJS = $(SRCS:%.c=$(BUILDDIR)/%.o)
VARIANT_TARGET = $(BUILDDIR)/$(TARGET)
HEADERS = bar.h config.h keys.h wm.h

# Rules
all: $(TARGET)

$(TARGET): FORCE $(VARIANT_TARGET)
	cp $(VARIANT_TARGET) $@

$(VARIANT_TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(BUILDDIR)/%.o: %.c $(HEADERS) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(FEATURE_CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $@

clean:
	rm -f $(TARGET) *.o
	rm -rf .build

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
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
	test -f $(HOME)/.config/monowm/bg.png || install -m 644 bg.png $(HOME)/.config/monowm/bg.png
	echo '#!/bin/sh' > $(HOME)/.xinitrc
	echo 'exec $(BINDIR)/monowm-start' >> $(HOME)/.xinitrc
	chmod +x $(HOME)/.xinitrc

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(BINDIR)/monowm-start
	rm -f $(DESTDIR)$(BINDIR)/monowm-volume
	rm -f $(DESTDIR)$(BINDIR)/monowm-brightness
	rm -f $(DESTDIR)$(SESSIONDIR)/monowm.desktop

FORCE:

.PHONY: all clean install uninstall FORCE
