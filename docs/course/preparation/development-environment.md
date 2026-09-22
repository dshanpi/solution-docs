---
title: "准备主机、SDK 和调试环境"
sidebar_label: "05 准备开发环境"
sidebar_position: 5
slug: /course/preparation/development-environment
---

# 准备主机、SDK 和调试环境

完成串口、WiFi 和文件传输准备后，才具备稳定修改系统的条件。本课继续检查源码和交叉编译器，确认开发主机能够生成 T527 使用的 ARM64 程序；串口登录、WiFi 连接和文件更新方法见上一课[连接 WiFi 与确认网络](./wifi-network.md)。

先在虚拟机里找到 SDK，确认编译器能生成 T527 使用的 ARM64 程序，再准备一个可以执行板端命令的终端。后续课程中的“开发主机”和“板端”分别指下面两个环境：

| 环境 | 在这里做什么 | 本课如何确认 |
| --- | --- | --- |
| Ubuntu 虚拟机（开发主机） | 查看和修改源码、编译程序、检查生成文件 | 找到 SDK，完成一次最小编译 |
| T527 开发板（板端） | 运行采集、编码和键鼠服务，查看设备与日志 | 通过终端读取内核版本和设备型号 |

## 登录虚拟机并进入 SDK

启动配套 Ubuntu 虚拟机后，可以直接打开虚拟机内的终端，也可以从电脑上的终端通过 SSH 登录。SSH 命令格式为 `ssh ubuntu@虚拟机IP`，执行时将 `虚拟机IP` 替换成虚拟机当前的地址，使用配套环境的登录密码。

登录后，在 **Ubuntu 虚拟机的终端**执行：

```bash
cd ~/T527_Tina5.0_SDK
export SDK_ROOT="$PWD"
pwd
uname -m
```

配套环境的 SDK 位于 `ubuntu` 用户的主目录，`pwd` 应显示以 `T527_Tina5.0_SDK` 结尾的路径，`uname -m` 输出 `x86_64`。如果 SDK 存放在其他目录，先进入实际目录，再设置 `SDK_ROOT`；后续命令通过这个变量定位源码和工具链。

`SDK_ROOT` 只在当前终端会话中生效。重新打开终端后，需要再次进入 SDK 并设置它。

## 找到后续要修改的源码

SDK 不只有应用程序，还包含板级配置、内核驱动和构建工具。后续课程会用到以下文件，路径均相对 SDK 根目录：

| 路径 | 后续用途 |
| --- | --- |
| `.buildconfig` | 查看当前选择的板型、内核架构和工具链 |
| `device/config/chips/t527/configs/demo_linux_aiot/linux-5.15/board.dts` | 描述本板的采集接口、I²C 和 GPIO 配置 |
| `bsp/drivers/vin/modules/sensor/lt6911c_mipi.c` | HDMI 采集模块的驱动 |
| `kvm_video_demo/main.cpp` | 视频采集与编码程序 |
| `kvm_video_demo/Makefile` | 视频程序使用的编译器、头文件和库路径 |
| `kvm/` | 配套 KVM 应用工程 |

在 SDK 根目录读取当前构建配置：

```bash
cd "$SDK_ROOT"
grep -E 'LICHEE_BOARD=|LICHEE_KERNEL_ARCH=|LICHEE_KERNEL_VERSION=|LICHEE_BR_VER=' .buildconfig
ls kvm_video_demo/main.cpp kvm_video_demo/Makefile
ls bsp/drivers/vin/modules/sensor/lt6911c_mipi.c
```

配套 SDK 的配置输出为：

```text
export LICHEE_BOARD=demo_linux_aiot
export LICHEE_KERNEL_ARCH=arm64
export LICHEE_KERNEL_VERSION=5.15.147
export LICHEE_BR_VER=202205
```

其中 `demo_linux_aiot` 是 SDK 使用的板级配置名，`arm64` 是生成内核的目标架构，后两项分别对应 Linux 内核版本和 Buildroot 版本。Buildroot 用来构建板端的根文件系统，板上程序需要的系统库也来自这套环境。

如果文件不存在，先检查当前目录和配套 SDK 是否完整。如果配置值不同，需核对所用 SDK 与课程的板型、版本是否一致，再继续使用后续章节的路径。

## 确认交叉编译器的目标架构

虚拟机使用 x86_64 处理器环境，T527 板使用 ARM64。要在虚拟机中生成板上运行的程序，需要使用 SDK 配套的**交叉编译器**：编译器本身运行在虚拟机里，生成的程序面向 T527。

在同一个虚拟机终端设置工具链前缀：

```bash
export CROSS_COMPILE="$SDK_ROOT/out/toolchain/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-"
"${CROSS_COMPILE}gcc" --version
"${CROSS_COMPILE}gcc" -dumpmachine
```

配套编译器的版本为 **GCC 10.3.1**，第二条命令输出：

```text
aarch64-none-linux-gnu
```

`aarch64` 表示编译目标是 ARM64。`CROSS_COMPILE` 是工具路径的公共前缀，加上 `gcc`、`g++` 或 `readelf`，分别得到 C 编译器、C++ 编译器和可执行文件检查工具的完整路径。这里显式指定编译器，避免误用虚拟机自身的 `gcc`。

注意，设置这个变量不会自动改写现有 Makefile。配套 `kvm_video_demo/Makefile` 已单独指定 `CC`、`CXX` 及 SDK 路径；如果移动了 SDK，编译视频程序前还要核对这些配置。

## 编译一个最小程序

版本命令只能确认编译器能够启动。再编译一个只输出一行文字的 C 程序，可以检查编译、链接和目标文件生成是否正常，不涉及视频库或硬件驱动。

在虚拟机的同一个终端创建临时练习目录，写入 `hello.c`：

```bash
CHECK_DIR=$(mktemp -d /tmp/kvm-toolchain-check.XXXXXX)
cat > "$CHECK_DIR/hello.c" <<'EOF'
#include <stdio.h>

int main(void)
{
    puts("Hello, T527!");
    return 0;
}
EOF

"${CROSS_COMPILE}gcc" -Wall -Wextra -o "$CHECK_DIR/hello" "$CHECK_DIR/hello.c"
file "$CHECK_DIR/hello"
"${CROSS_COMPILE}readelf" -h "$CHECK_DIR/hello" | grep -E 'Class:|Machine:'
```

编译成功后，`file` 输出中应包含 `ELF 64-bit` 和 `ARM aarch64`，`readelf` 的关键输出为：

```text
Class:                             ELF64
Machine:                           AArch64
```

这说明已经生成 ARM64 可执行文件。此处只在虚拟机里检查文件；运行目标是 T527 板，后续部署时还需确认板端的动态加载器和库与程序匹配。

### 查看程序依赖的运行库

交叉编译完成后，程序可能还需要板端提供共享库。以 SDK 中已有的 `kvm_video` 为例，在虚拟机终端检查它：

```bash
cd "$SDK_ROOT"
file kvm_video_demo/kvm_video
"${CROSS_COMPILE}readelf" -l kvm_video_demo/kvm_video | grep 'Requesting program interpreter'
"${CROSS_COMPILE}readelf" -d kvm_video_demo/kvm_video | grep NEEDED
```

| 检查结果 | 与板端的关系 |
| --- | --- |
| `ARM aarch64` | 程序的目标架构应与 T527 一致 |
| `/lib/ld-linux-aarch64.so.1` | 程序启动时使用的动态加载器，板端必须提供 |
| `NEEDED` 中的 `libvencoder.so`、`libsdk_memory.so` 等 | 采集和编码程序依赖的共享库，需由配套系统提供 |

因此，部署视频程序时需要同时核对板端系统与 SDK 的配套关系。最小程序编译通过，说明基础工具链可用；它还不能证明视频程序的依赖库和硬件运行条件已经齐全。若 SDK 中尚未生成 `kvm_video`，这部分检查留到后续视频程序编译完成后进行。

## 打开板端终端并记录系统版本

源码与编译器检查完成后，再打开一个连接 **T527 开发板**的终端。可以使用配套串口终端，或通过板端已启用的 SSH、网页终端进入系统。网络登录使用开发板的地址；前面登录 SDK 虚拟机的连接仍用于源码和编译操作。

串口的接口、电平和波特率按板卡说明设置。串口连接不依赖板端网络，后续网络服务无法访问时，仍可通过它查看启动日志。

在**板端终端**执行以下只读命令：

```sh
uname -r
uname -m
cat /proc/device-tree/model
```

将输出保存为当前板端系统的记录，重点核对内核版本和架构。配套样机此前记录为 Linux `5.15.147`、`aarch64`；以手中开发板的实际输出为准。如果版本不同，下一课检查参考系统时要一并核对，避免直接混用内核模块或应用库。

板端使用 Buildroot 构建的精简系统，开发工具主要放在 Ubuntu 虚拟机中。后续章节需要安装主机工具时，在虚拟机执行；板端是否提供某条命令，应以实际系统为准。

完成本课后，应能找到 SDK 源码并生成一个 ARM64 可执行文件。接下来进入[传输核心程序](./file-transfer.md)，检查板上的 KVM 服务、视频设备和浏览器画面。
