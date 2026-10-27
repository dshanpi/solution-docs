SDK_ROOT ?= $(HOME)/T527_Tina5.0_SDK
CROSS_COMPILE ?= $(SDK_ROOT)/out/toolchain/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CFLAGS := -std=c11 -Wall -Wextra -O2

all: query_format

query_format: query_format.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f query_format

.PHONY: all clean
