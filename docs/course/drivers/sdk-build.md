---
title: "核对 SDK 与开发板版本"
sidebar_label: "08 检查 SDK 与镜像"
sidebar_position: 5
slug: /course/drivers/sdk-build
---

# 核对 SDK 与开发板版本

SDK 和开发板镜像已经准备好。这一课确认两者属于同一套版本，为后面的设备树、驱动和 HDMI 检查建立可靠基线；**不重新构建镜像，也不烧录**。只有版本不一致时，才回第 05 课恢复 SDK，或到第 25 课按部署流程处理。

## 核对 SDK 侧

进入 SDK 根目录，读取板型、内核和 Buildroot 版本：

```bash
cd ~/T527_Tina5.0_SDK
grep -E 'LICHEE_BOARD=|LICHEE_KERNEL_ARCH=|LICHEE_KERNEL_VERSION=|LICHEE_BR_VER=' .buildconfig
```

配套 SDK 的配置为：

```text
export LICHEE_BOARD=demo_linux_aiot
export LICHEE_KERNEL_ARCH=arm64
export LICHEE_KERNEL_VERSION=5.15.147
export LICHEE_BR_VER=202205
```

确认交叉编译器能启动：

```bash
"${CROSS_COMPILE}gcc" --version | head -1
```

应为 GCC 10.3.1。`CROSS_COMPILE` 的完整设置见第 05 课；如果当前终端没有设置，先回到那一课设置 `SDK_ROOT` 和 `CROSS_COMPILE`。

再看预编译的用户态程序：

```bash
ls -l out/t527/demo_linux_aiot/buildroot/buildroot/target/userdata/100ask_kvm/bin
sha256sum out/t527/demo_linux_aiot/buildroot/buildroot/target/userdata/100ask_kvm/bin/kvm_app
```

目录中应有 `kvm_app`、`kvm_hid`、`kvm_platform`、`kvm_video`。这些是 SDK 自带的参考程序，本课只核对，不重新编译。

## 核对板端镜像

在板端读取内核、板型和运行状态：

```sh
uname -r
cat /proc/device-tree/model; echo
pidof kvm_app kvm_video kvm_hid kvm_platform
cat /sys/class/udc/4100000.udc-controller/state
```

本次样机输出内核 `5.15.147`、板型 `sun55iw3`，四个 KVM 进程都在，UDC 为 `configured`。板端 `uname -r` 必须和 SDK 的 `LICHEE_KERNEL_VERSION` 一致；内核版本不同，后面不能直接套用 SDK 编译出的 `.ko`。

再核对安装目录和程序：

```sh
ls -l /userdata/100ask_kvm/bin
sha256sum /userdata/100ask_kvm/bin/kvm_app
cat /userdata/100ask_kvm/platform.env
```

在开发主机对同一程序执行 `sha256sum`，两边结果应一致。本次这套镜像与 SDK target 的四个程序哈希全部一致。若不一致，说明板端镜像不是由当前 SDK 生成的，先确认版本，不要继续替换驱动或程序。

## 版本不一致时怎么办

| 情况 | 处理 |
| --- | --- |
| `~/T527_Tina5.0_SDK` 不存在或不完整 | 回到第 05 课按分卷归档恢复并校验 |
| 板端 `uname -r` 与 SDK 的 `5.15.147` 不同 | 不要混用 SDK 编译的 `.ko`；换回匹配镜像，或按第 25 课重新部署 |
| 四个程序哈希不一致 | 确认板端镜像来源；不要用本课强行覆盖生产程序 |
| 四个进程缺失或 UDC 不是 `configured` | 先检查 `S99kvm` 和 USB 状态；这属于运行问题，不是版本问题 |

本课不执行 `./build.sh`，也不烧录；构建环境已经在第 05 课确认，需要重新打包或烧录时进入第 25 课。

## 验收

- SDK 的板型、内核和 Buildroot 版本与上表一致，交叉编译器能启动。
- target 目录中有四个参考程序。
- 板端内核为 `5.15.147`，四个 KVM 进程在，UDC 为 `configured`。
- 板端与 SDK target 的程序哈希一致；不一致已被记录并停止后续部署。
- 没有重新构建或烧录。

下一课开始核对设备树里的采集配置；版本基线一致后，才能判断后续检查结果是否可信。
