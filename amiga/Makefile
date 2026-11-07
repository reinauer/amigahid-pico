# Native AmigaOS 2.04+, 68000-compatible Hunk executable.
AMIGA_PREFIX ?= m68k-amigaos-
CC = $(AMIGA_PREFIX)gcc
PYTHON ?= python3
OUT ?= ../build/amiga-prefs
CFLAGS = -std=c99 -Os -m68000 -Wall -Wextra -Werror -I../common -I../src

all: $(OUT)/AmigaHIDPrefs $(OUT)/AmigaHIDPrefs.info

$(OUT)/AmigaHIDPrefs: AmigaHIDPrefs.c transport.c transport.h ../common/prefs_protocol.c ../common/prefs_protocol.h ../src/settings.h
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) -noixemul AmigaHIDPrefs.c transport.c ../common/prefs_protocol.c -o $@

$(OUT)/AmigaHIDPrefs.info: make_icon.py
	mkdir -p $(OUT)
	$(PYTHON) make_icon.py $@

.PHONY: all
