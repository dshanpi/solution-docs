---
title: "处理 USB 冲突、断线与状态"
sidebar_label: "21 处理 USB 异常"
sidebar_position: 18
slug: /course/usb-hid/usb-recovery
---

# 处理 USB 冲突、断线与状态

USB 键鼠出问题，常见的不是单点故障：控制器被别的 Gadget 占着、服务停了但绑定还在、节点消失，或者主机侧按键一直卡着。这一课先判断状态，再写一个恢复脚本，最后在板子和主机两端验证。

## 判断控制器被谁占着

先只读检查：

```sh
ls /sys/class/udc
for f in /sys/kernel/config/usb_gadget/*/UDC; do
    [ -f "$f" ] || continue
    printf '%s: ' "$f"
    cat "$f"
done
ps | grep -E 'adbd|kvm' | grep -v grep
```

**单个 UDC 同时只能绑定一个 Gadget**。一个 Gadget 可以带多个功能，但 ADB 和 KVM 各自创建的两个 Gadget 不能同时争用同一个控制器，后绑定的会报 `couldn't find an available UDC or it's busy`。

另外，**停止进程不等于解除绑定**：`S50adb_start stop` 只停 adbd，configfs 里的 `UDC` 属性还留着。要向那个 Gadget 的 `UDC` 写一个换行才算解绑：

```sh
printf '\n' > /sys/kernel/config/usb_gadget/<占用者>/UDC
```

## 三层状态一起看

| 层次 | 看哪里 | 成功含义 |
| --- | --- | --- |
| Gadget 绑定 | `kvm/UDC` | 功能集合已交给控制器 |
| 控制器状态 | `/sys/class/udc/4100000.udc-controller/state` | 变成 `configured` |
| 主机枚举与输入 | 主机 `dmesg`、`evtest`、编辑器 | 设备出现，报告被正确解释 |

`configured` 只是控制器状态，不能代替主机侧验证；反过来，主机没新设备时，先回来看绑定和控制器。

## 恢复脚本

把检查、释放、重绑写成三个子命令，出问题时按状态决定用哪个。脚本先定义控制器和 KVM Gadget 路径：

```sh
K=/sys/kernel/config/usb_gadget/kvm
UDC=${UDC:-$(ls /sys/class/udc | head -n 1)}
```

### status：把四类信息一次列出

```sh
cat "/sys/class/udc/$UDC/state"
for f in /sys/kernel/config/usb_gadget/*/UDC; do cat "$f"; done
pidof adbd kvm_hid kvm_video kvm_app kvm_platform
```

再遍历每个 Gadget 的 `functions/hid.usb*`，用功能的 `dev` 属性去 `/sys/class/hidg/` 里匹配，打印节点和 `report_length`。这样节点编号变化也能对上。

### release：向所有节点写全零报告

键盘卡键、按钮按着不放时，按每个功能的 `report_length` 写对应长度的零：

```sh
len=$(cat "$f/report_length")
dd if=/dev/zero of="$node" bs="$len" count=1
```

键盘 8 字节、绝对鼠标 6 字节、相对鼠标 4 字节；长度不对会被节点拒绝或只写一部分。

### rebind：停服务、解绑、再启动

```sh
/etc/init.d/S99kvm stop
for f in /sys/kernel/config/usb_gadget/*/UDC; do printf '\n' > "$f" || true; done
/etc/init.d/S99kvm start
```

然后轮询 `/sys/class/udc/<UDC>/state`，最多等 8 秒看是否回到 `configured`。执行前要确认教学 Gadget 已经 teardown，否则 HID 功能名额不够，KVM 起不来。

完整脚本在课件 [21-usb-recovery](/examples/kvm/05-usb-hid/21-usb-recovery/usb-recover.sh)，可以对照。

## 运行与验证

### 准备

在开发主机进入课件目录起临时 HTTP 服务：

```bash
cd <课件目录>
python3 -m http.server 8000
```

板端下载并和主机 `md5sum` 对照：

```sh
mkdir -p /tmp/recover && cd /tmp/recover
wget -q -O usb-recover.sh http://192.168.1.69:8000/usb-recover.sh
chmod +x usb-recover.sh
md5sum usb-recover.sh
```

### 查状态

KVM 正常运行时执行 `./usb-recover.sh status`，实测输出：

```text
== 控制器 ==
UDC: 4100000.udc-controller
configured
== Gadget 绑定 ==
/sys/kernel/config/usb_gadget/g1/UDC:
/sys/kernel/config/usb_gadget/kvm/UDC: 4100000.udc-controller
== 进程 ==
adbd: 未运行
kvm_hid: 6413
kvm_video: 6421
kvm_app: 6420
kvm_platform: 6384
== HID 节点 ==
kvm:hid.usb0 -> /dev/hidg0，report_length=8
kvm:hid.usb1 -> /dev/hidg1，report_length=6
kvm:hid.usb2 -> /dev/hidg2，report_length=4
```

从这张表就能判断：控制器被 `kvm` 绑着，ADB 没在跑，节点和功能一一对应。

### 释放卡住的按键

先建教学 Gadget（第 18 课的脚本），然后只发一份"按下"报告、不发释放，模拟卡键：

```sh
printf '\000\000\004\000\000\000\000\000' > /tmp/press.bin
cat /tmp/press.bin > /dev/hidg0
```

在被控设备的键盘节点上抓事件，会先看到按下，随后是主机自己产生的自动重复：

```text
type 1 (EV_KEY), code 30 (KEY_A), value 1
...
type 1 (EV_KEY), code 30 (KEY_A), value 2
type 1 (EV_KEY), code 30 (KEY_A), value 2
```

这时执行：

```sh
./usb-recover.sh release
```

```text
向所有 HID 节点写全零报告：
/dev/hidg0: 已释放
/dev/hidg1: 已释放
/dev/hidg2: 已释放
```

主机侧紧接着收到 `value 0`，自动重复停止。

### 重新绑定

先 `./hid-gadget.sh teardown` 清掉教学 Gadget，再执行：

```sh
./usb-recover.sh rebind
```

脚本停 KVM、清空所有 Gadget 的 `UDC`、重新启动，最后打印状态。实测 UDC 回到 `configured`，`kvm_hid`、`kvm_video`、`kvm_app`、`kvm_platform` 都在。主机 `dmesg` 里能看到设备断开后重新枚举：

```text
usb 1-1.2: New USB device found, idVendor=1d6b, idProduct=0104
usb 1-1.2: Product: USB Emulation Device
hid-generic ...: USB HID v1.01 Keyboard [KVM USB Emulation Device] ... /input0
hid-generic ...: USB HID v1.01 Mouse [KVM USB Emulation Device] ... /input1
hid-generic ...: USB HID v1.01 Mouse [KVM USB Emulation Device] ... /input2
```

## 清理与恢复

`rebind` 已经把 KVM 恢复；如果教学 Gadget 还在，先执行第 18 课的 `teardown`。不要把 `release` 和 `rebind` 同时跑，避免两个管理者争同一组节点。

## 验收

- `status` 能指出控制器被哪个 Gadget 占用，节点与功能对应；
- 模拟卡键后，`release` 能让主机收到释放，自动重复停止；
- `rebind` 后 UDC `configured`，四个服务在，主机重新看到键盘和两个鼠标；
- `dmesg` 与 `evtest` 的结果和板端状态一致。

## 常见问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| `rebind` 报教学 Gadget 还在 | 第 18 课的对象没清理 | 先 `hid-gadget.sh teardown` |
| `status` 看到两个 Gadget 都有 UDC | 两个 Gadget 争用同一个控制器 | 只保留一个，另一个写换行解绑 |
| `release` 写入失败 | 节点没枚举或主机断开 | 先看 UDC 状态和主机 `dmesg` |
| 重绑后主机没设备 | 主机没重新枚举 | 重新插拔 USB，再看 `dmesg` |
| 仍有卡键 | 物理断开时释放报告送不到 | 重连后立即执行 `release` |