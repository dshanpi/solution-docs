SDK_ROOT ?= $(HOME)/T527_Tina5.0_SDK
CROSS_COMPILE ?= $(SDK_ROOT)/out/toolchain/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CFLAGS := -std=c11 -Wall -Wextra -O2

all: hid_keyboard

hid_keyboard: hid_keyboard.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f hid_keyboard

.PHONY: all clean