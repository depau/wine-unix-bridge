SHELL := /bin/bash
.SHELLFLAGS := -euo pipefail -c

WINEGCC ?= winegcc
MINGW64_CC ?= x86_64-w64-mingw32-gcc
MINGW32_CC ?= i686-w64-mingw32-gcc

CFLAGS_COMMON := -O2 -Wall -Wextra -Werror -Wno-error=attributes
LDFLAGS_COMMON :=

BUILD32 := build/x86
BUILD64 := build/x64

.PHONY: all clean

# Do not build for x86 by default, since newer WoW64 Wine releases lack support for building 32-bit DLLs
all: x64

x86: $(BUILD32)/unixbridge.exe $(BUILD32)/unixbridge.dll
x64: $(BUILD64)/unixbridge.exe $(BUILD64)/unixbridge.dll


$(BUILD32) $(BUILD64):
	mkdir -p "$@"

# --- Wrapper EXE: PE built with MinGW ---

$(BUILD32)/unixbridge.exe: unixwrap.c | $(BUILD32)
	$(MINGW32_CC) $(CFLAGS_COMMON) -municode -o "$@" $< -lshell32

$(BUILD64)/unixbridge.exe: unixwrap.c | $(BUILD64)
	$(MINGW64_CC) $(CFLAGS_COMMON) -municode -o "$@" $< -lshell32

# --- Winelib DLL: built with winegcc (Wine-only) ---

$(BUILD32)/unixbridge.dll: unixbridge.c unixbridge.def | $(BUILD32)
	$(WINEGCC) -m32 -shared $(CFLAGS_COMMON) -o "$@" unixbridge.c unixbridge.def $(LDFLAGS_COMMON)
	mv "$@.so" "$@"

$(BUILD64)/unixbridge.dll: unixbridge.c unixbridge.def | $(BUILD64)
	$(WINEGCC) -m64 -shared $(CFLAGS_COMMON) -o "$@" unixbridge.c unixbridge.def $(LDFLAGS_COMMON)
	mv "$@.so" "$@"

clean:
	rm -rf build
