SDK_ROOT ?= $(HOME)/T527_Tina5.0_SDK
CROSS_COMPILE ?= $(SDK_ROOT)/out/toolchain/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CFLAGS := -std=c11 -Wall -Wextra -Wno-unused-parameter -O2

all: kvm_hid_service

kvm_hid_service: kvm_hid_service.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f kvm_hid_service

.PHONY: all clean
