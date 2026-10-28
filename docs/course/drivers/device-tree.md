---
title: "核对设备树里的采集配置"
sidebar_label: "09 检查设备树"
sidebar_position: 6
slug: /course/drivers/device-tree
---

# 核对设备树里的采集配置

SDK 已经为 LT6911C 写好 I²C、GPIO、参考时钟和 VIN 参数。这一课只做核对：确认这些配置在源码和 DTB 里都存在，并和板端实际状态一致。正常流程不需要改设备树；只有换板、换模块或接线不同，才进入最后一节的修改分支。

## 先确认 SDK 里的配置存在

在 SDK 根目录读取板级文件：

```bash
grep -nE 'twi3|PE3|PE4|sensor_detect|sensor0_mname|sensor0_twi|sensor0_isp|hotplug_gpios|refclk-frequency|reset_gpios' \
  device/config/chips/t527/configs/demo_linux_aiot/linux-5.15/board.dts
```

配套 SDK 中与本板相关的字段是：

```text
sensor0_mname = "lt6911c_mipi";
sensor0_twi_cci_id = <3>;
sensor0_twi_addr = <0x56>;
sensor0_isp_used = <0>;
```

| 字段 | 由谁使用 | 当前含义 |
| --- | --- | --- |
| `sensor0_mname` | 厂商 VIN 注册路径 | 匹配模块驱动名 |
| `sensor0_twi_cci_id` | VIN 的控制总线选择 | 使用编号 3 的 TWI |
| `sensor0_twi_addr` | VIN 注册 I²C 设备的代码 | 厂商地址格式 `0x56` |
| `sensor0_isp_used` | VIN 图像通路 | 外部 YUV 输入不使用这里的 ISP 处理 |

同时确认节点附近是 `status = "okay"`，并且 `twi3` 已启用、`twi4` 仍为 disabled。看到 `twi4_pins_default` 里出现 PE14 不代表 PE14 被占用，要看对应控制器是否启用、pinctrl 是否被引用。

这套字段属于全志 VIN 的板级约定，**不能当成通用 Linux I²C binding** 套到其他设备。

### 0x56 与 0x2b 的关系

VIN 注册 I²C 设备前会把地址右移一位，所以设备树里的 `0x56` 在 Linux 侧对应 7 位地址 `0x2b`。板端 sysfs 里应出现 `3-002b`：

```sh
ls /sys/bus/i2c/devices | grep 002b
cat /sys/bus/i2c/devices/3-002b/name
readlink /sys/bus/i2c/devices/3-002b/driver
```

本次样机输出为 `lt6911c_mipi`，驱动链接指向 `lt6911c_mipi`。把 `0x56` 原样传给要求 7 位地址的工具会访问另一个地址；设备已被驱动占用时，也不要用强制 I²C 访问和驱动争用寄存器页。

## 核对引脚、时钟和启动参数

当前 `twi3` 使用 PE3/PE4，总线频率 100 kHz。`sensor_detect` 节点包含：

```dts
sensor_detect: sensor_detect@2108200 {
    hotplug_gpios = <&pio PE 15 GPIO_ACTIVE_HIGH>;
    lontium,refclk-frequency = <24000000>;
    /* RSTN 固定接 3.3V，不申请复位 GPIO。 */
    // reset_gpios = <&pio PE 14 GPIO_ACTIVE_LOW>;
};
```

| 配置 | 本板含义 | 核对点 |
| --- | --- | --- |
| `hotplug_gpios` | GPIO5 接 H1 第 3 脚 PE15 | 输入状态事件 |
| `lontium,refclk-frequency` | 模块 24 MHz 晶振 | 驱动用它换算输入时钟 |
| `reset_gpios` 注释掉 | RSTN 固定接 3.3V | 驱动不要操作复位脚 |
| `twi3` 频率 | 100 kHz | 与模块和走线匹配 |

配套启动脚本 `buildroot/100ask/etc/init.d/S91camera` 先加载 `vin_io`，再用 `reset_tied_high=1 refclk_hz=24000000` 加载 `lt6911c_mipi`，最后加载 `vin_v4l2`。板端核对：

```sh
cat /sys/module/lt6911c_mipi/parameters/reset_tied_high
cat /sys/module/lt6911c_mipi/parameters/refclk_hz
```

本次样机分别输出 `Y` 和 `24000000`。`Y` 表示驱动不操作复位 GPIO；设备树中的 `lontium,refclk-frequency` 如果存在，会优先用于时序换算。

## 检查编译后的 DTB

在开发主机对本次构建的 DTB 反编译，确认字段真的进了产物：

```bash
cd ~/T527_Tina5.0_SDK
dtc -I dtb -O dts \
  out/t527/demo_linux_aiot/buildroot/sunxi.dtb \
  -o /tmp/kvm-built.dts
grep -nE 'lt6911c|sensor_detect|hotplug_gpios|reset_gpios' /tmp/kvm-built.dts
```

生成的 phandle 数字可能和源代码标签不同，应追踪对应节点，不要比较标签文本。核对的是三处一致性：源码字段、构建出的 DTB、板端驱动实际读到的参数。板端 `/proc/device-tree` 不一定导出所有节点，节点缺失时以 DTB 和驱动日志为准。

## 只有换硬件才需要改

正常核对通过后，不要修改设备树。出现下列差异才进入修改分支：

| 变化 | 修改位置 |
| --- | --- |
| I²C 总线或地址变化 | `sensor0_twi_cci_id`、`sensor0_twi_addr` |
| 检测 GPIO 或复位方式变化 | `hotplug_gpios`、`reset_gpios`、`S91camera` 参数 |
| 模块参考时钟从 24 MHz 改为 27 MHz | `lontium,refclk-frequency` 和 `S91camera` 的 `refclk_hz` |
| 引脚复用变化 | `twi3_pins_default`、`twi3_pins_sleep` |

改前备份原文件并记录改动，重新构建 DTB 后再核对。重新构建见[准备开发环境](../preparation/development-environment.md)，部署见[部署与开机启动](../deployment/startup.md)；驱动本身的检查放到下一课。

## 验收

- 板级文件中 `sensor0` 的模块名、TWI 编号、地址和 `status` 与上表一致。
- 反编译 DTB 能找到 `sensor_detect` 和 `hotplug_gpios`，没有意外的 `reset_gpios`。
- 板端 `3-002b` 存在，`reset_tied_high=Y`、`refclk_hz=24000000`。
- 全程没有为了“跑通”修改源码；只有实际硬件不同才走修改分支。

下一课检查 LT6911C 驱动是否按这份配置加载、绑定并创建采集节点。
