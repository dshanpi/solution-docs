SDK_ROOT ?= $(HOME)/T527_Tina5.0_SDK
CROSS_COMPILE ?= $(SDK_ROOT)/out/toolchain/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
CXX := $(CROSS_COMPILE)g++
SDK_LIB := $(SDK_ROOT)/buildroot/package/auto/sdk_lib
TARGET_LIB := $(SDK_ROOT)/out/t527/demo_linux_aiot/buildroot/buildroot/target/usr/lib
WRAPPER := $(SDK_LIB)/sdk_encoder/AWVidoeEncoder.cpp
DEFINES := -DUSE_LOGCAT -DLOG_LEVEL=3 -DKVM_H264_LEVEL=VENC_H264Level42 \
           -DMODULE_VERSION=\"KVM-1080p60\"
CXXFLAGS := -std=c++17 -Wall -Wextra -Wno-missing-field-initializers -O2
INCS := -I$(SDK_LIB)/include -I$(SDK_LIB)/cedarx/include/libcedarc/include
LIBS := -lsdk_memory -lMemAdapter -lVE -lcdc_base -lsdk_log \
        -lvencoder -lvenc_jpeg -lvenc_common -lvenc_h264 -lvenc_h265 -lvenc_base \
        -lpthread
LDFLAGS := -L$(SDK_LIB)/libs -L$(TARGET_LIB)

all: encode_buffer

encode_buffer: encode_buffer.cpp awvencoder.o
	$(CXX) $(CXXFLAGS) $(DEFINES) $(INCS) -o $@ encode_buffer.cpp awvencoder.o $(LIBS) $(LDFLAGS)

awvencoder.o: $(WRAPPER)
	$(CXX) -std=c++17 -O2 -w $(DEFINES) $(INCS) -c -o $@ $<

clean:
	rm -f encode_buffer awvencoder.o

.PHONY: all clean
