SDK_ROOT ?= $(abspath ../..)
HOST_CC ?= cc
CROSS_PREFIX ?= $(SDK_ROOT)/out/toolchain/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
CC := $(CROSS_PREFIX)gcc
CXX := $(CROSS_PREFIX)g++
OUT := build
HOST_OUT := $(OUT)/host
ARM_OUT := $(OUT)/arm64
CFLAGS := -O2 -Wall -Wextra -Werror -Iinclude
SDK_LIB := $(SDK_ROOT)/buildroot/package/auto/sdk_lib
TARGET_LIB := $(SDK_ROOT)/out/t527/avaota_a1/buildroot/buildroot/target/usr/lib

.PHONY: all host arm64 clean
all: host arm64
host: $(HOST_OUT)/protocol_test $(HOST_OUT)/hid_reports
arm64: $(ARM_OUT)/v4l2_probe $(ARM_OUT)/hid_reports $(ARM_OUT)/kvm_video
$(HOST_OUT) $(ARM_OUT):
	mkdir -p $@
$(HOST_OUT)/protocol_test: tests/protocol_test.c src/kvm2_protocol.c | $(HOST_OUT)
	$(HOST_CC) $(CFLAGS) $^ -o $@
$(HOST_OUT)/hid_reports: src/hid_reports.c | $(HOST_OUT)
	$(HOST_CC) $(CFLAGS) $< -o $@
$(ARM_OUT)/v4l2_probe: src/v4l2_probe.c | $(ARM_OUT)
	$(CC) $(CFLAGS) $< -o $@
$(ARM_OUT)/hid_reports: src/hid_reports.c | $(ARM_OUT)
	$(CC) $(CFLAGS) $< -o $@
$(ARM_OUT)/kvm_video: src/kvm_video.cpp | $(ARM_OUT)
	$(CXX) -O2 -Wall -Wextra -I$(SDK_LIB)/include -I$(SDK_LIB)/cedarx/include/libcedarc/include $< -o $@ \
	-L$(SDK_LIB)/libs -L$(TARGET_LIB) -lsdk_memory -lMemAdapter -lVE -lcdc_base -lsdk_log \
	-lvencoder -lvenc_jpeg -lvenc_common -lvenc_h264 -lvenc_h265 -lvenc_base -lsdk_encoder -lpthread -lsdk_awnn -lVIPlite -lVIPuser
clean:
	rm -rf $(OUT)
