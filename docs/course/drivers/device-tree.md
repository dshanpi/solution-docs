---
title: "把原理图连接写进设备树"
sidebar_label: "06 适配设备树"
sidebar_position: 6
slug: /course/drivers/device-tree
---

# 把原理图连接写进设备树

I²C 线连在 PE3/PE4 上，Linux 却不会从导线上自动推断连接关系。设备树把板上的总线、器件与引脚配置交给驱动；**它描述硬件，不是执行接线动作的脚本**。

## 从网络名定位板级文件

当前板级文件为 `device/config/chips/t527/configs/demo_linux_aiot/linux-5.15/board.dts`。在 SDK 根目录搜索：

```bash
grep -nE 'twi3|PE3|PE4|sensor_detect|sensor0_mname|sensor0_twi' \
  device/config/chips/t527/configs/demo_linux_aiot/linux-5.15/board.dts
```

`twi3_pins_default` 选择 PE3/PE4，`&twi3` 启用控制器并配置 100000 Hz。`twi4_pins_default` 中出现 PE14 不代表它已经被占用，还要看控制器是否启用、该 pinctrl 是否被引用。当前 TWI4 为 disabled。

## VIN 使用的模块描述

下列是当前 `sensor0` 的关键字段节选：

```dts
sensor0_mname = "lt6911c_mipi";
sensor0_twi_cci_id = <3>;
sensor0_twi_addr = <0x56>;
sensor0_isp_used = <0>;
status = "okay";
```

| 字段 | 由谁使用 | 当前含义 |
| --- | --- | --- |
| `sensor0_mname` | 厂商 VIN 注册路径 | 匹配模块驱动名 |
| `sensor0_twi_cci_id` | VIN 的控制总线选择 | 使用编号 3 的总线 |
| `sensor0_twi_addr` | VIN 注册 I²C 设备的代码 | 厂商地址格式 `0x56` |
| `sensor0_isp_used` | VIN 图像通路 | 当前外部 YUV 输入不使用这里的 ISP 处理 |

这套字段属于全志 VIN 的板级约定，**不能当成通用 Linux I²C binding** 套到所有设备。

### 0x56 与 0x2b 的关系

`bsp/drivers/vin/vin.c` 在注册设备前对地址右移一位。因此当前字段 `0x56 >> 1 = 0x2b`，**Linux I²C 工具使用的是 7 位地址 `0x2b`**。把 `0x56` 原样传给要求 7 位地址的工具，会访问另一个地址。

先用 sysfs 查看已注册设备，不必强制扫描正在运行的总线：

```sh
ls /sys/bus/i2c/devices
cat /sys/bus/i2c/devices/3-002b/name
readlink /sys/bus/i2c/devices/3-002b/driver
```

其中 `3-002b` 适用于当前总线编号；更换系统后先从设备列表确认。设备已被驱动占用时，不要用强制 I²C 访问和驱动竞争寄存器页选择。

## 复位和事件 GPIO 单独核对

当前文件还包含：

```dts
sensor_detect: sensor_detect@2108200 {
    hotplug_gpios = <&pio PE 15 GPIO_ACTIVE_HIGH>;
    lontium,refclk-frequency = <24000000>;
    /* RSTN 固定接 3.3V，不申请复位 GPIO。 */
    // reset_gpios = <&pio PE 14 GPIO_ACTIVE_LOW>;
};
```

这个节点被驱动按名称查找。**GPIO5 接 H1 第 3 脚 PE15，RSTN 接 H1 第 1 脚 3.3V**，所以保留 `hotplug_gpios`，注释 `reset_gpios`。固定供电时不需要填写 `power_gpios` 或 `power1_gpios`；当前驱动将这些供电 GPIO 作为可选资源。

`lontium,refclk-frequency` 对应本模块的 **24 MHz 晶振**，用于换算检测到的输入时钟。更换模块时，应根据实际晶振核对该值，不能仅凭同型号芯片沿用参数。

配套启动脚本 `buildroot/100ask/etc/init.d/S91camera` 先加载 `vin_io`，再用 `reset_tied_high=1 refclk_hz=24000000` 加载 `lt6911c_mipi`，最后加载 `vin_v4l2`。这个顺序保证 VIN 自动加载接收驱动之前，固定高电平复位参数已经生效。

在板端查看实际参数：

```sh
cat /sys/module/lt6911c_mipi/parameters/reset_tied_high
cat /sys/module/lt6911c_mipi/parameters/refclk_hz
```

当前启动配置分别输出 `Y` 和 `24000000`。`Y` 表示驱动不会操作复位 GPIO；模块参数中的参考时钟是默认值，设备树中的 `lontium,refclk-frequency` 若存在，会优先用于时序换算。

## 检查编译后的 DTB

开发主机安装 `dtc` 后，对本次构建得到的 DTB 执行：

```bash
dtc -I dtb -O dts \
  out/t527/demo_linux_aiot/buildroot/sunxi.dtb \
  -o /tmp/kvm-built.dts
grep -nE 'lt6911c|sensor_detect|hotplug_gpios|reset_gpios' /tmp/kvm-built.dts
```

路径对应当前 SDK 已有产物，若构建输出位置不同，使用实际查到的文件。生成的 phandle 数字可能与源代码标签不同，应追踪对应节点，不比较标签文本是否完全一致。

核对**源 DTS、生成 DTB、部署文件和运行设备树四处的一致性**，才能解释一次 GPIO 修改是否真正生效。

<!-- LYNX-LAB:BEGIN -->

## AI 辅助实践闭环

- **稳定步骤**：`t527-kvm-06`
- **学习目标**：理解“把原理图连接写进设备树”在 T527 Web KVM 链路中的职责，并能用证据解释结果。
- **理解检查**：说明本章输入、输出以及失败时最先检查的证据。
- **学生操作**：在锁定的 Avaota A1 SDK 学习工作区完成“把原理图连接写进设备树”实验，不直接修改其他板型。
- **工具范围**：`code`
- **风险级别**：`software-safe`
- **预期现象**：命令退出码为 0，并生成带 SHA-256 的结构化证据。

确定性验证：

```bash
cd tutorial-examples/web-kvm
./scripts/verify_dts.sh
```

必须保存命令退出码、产物摘要和证据来源；模拟结果只能标记为 `simulation`。完成后回答：解释本章结果为何可信，并指出哪些结论仍需要真实硬件。

<details>
<summary>分级提示</summary>

1. 先确认本章输入、输出与证据来源。
2. 检查 ./scripts/verify_dts.sh 的首个失败阶段。
3. 只对当前步骤相关文件做最小修改，并重新生成一次独立 attempt。
4. 参考已签名课程包中的 solution/06，报告标记为 ASSISTED_PASS。

</details>

<!-- LYNX-LAB:END -->
