# wm - simple window manager Makefile

# Paths
PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin

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
TARGET = wm
SRCS = main.c appicons.c bar.c keys.c
OBJS = $(SRCS:.c=.o)
HEADERS = appicons.h config.h bar.h keys.h

# Rules
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

.PHONY: all clean install uninstall
