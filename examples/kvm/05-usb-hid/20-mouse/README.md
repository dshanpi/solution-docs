# 第二十课：编写相对鼠标和绝对鼠标程序

往两个鼠标节点写报告：绝对鼠标报坐标（0–32767），相对鼠标报位移（-127–127）。默认只打印示例报告，加参数才真正写入。

## 文件

| 文件 | 用途 |
| --- | --- |
| `hid_mouse.c` | 构造并发送绝对、相对鼠标报告 |
| `Makefile` | 用 SDK 的交叉编译器编译 |

## 两种报告

| 鼠标 | 节点 | report_length | 报告布局 |
| --- | --- | --- | --- |
| 绝对 | `/dev/hidg1` | 6 | Report ID 1：按键、X 低/高、Y 低/高；Report ID 2：滚轮 |
| 相对 | `/dev/hidg2` | 4 | 按键、X、Y、滚轮，各一字节有符号 |

按键位图：左键 `0x01`、右键 `0x02`、中键 `0x04`。绝对坐标范围 0–32767，是 16 位小端；相对位移是 8 位有符号。

## 编译

```bash
make SDK_ROOT=~/T527_Tina5.0_SDK
file hid_mouse
"${CROSS_COMPILE}readelf" -h hid_mouse | grep Machine
```

应显示 `ELF 64-bit` 和 `ARM aarch64`。板端没有编译器，程序在开发主机编好后传上去。

## 准备节点

用第 18 课的脚本建键鼠 Gadget：

```sh
UDC=4100000.udc-controller ./hid-gadget.sh setup
```

绝对鼠标是 `/dev/hidg1`，相对鼠标是 `/dev/hidg2`。

## 运行

先打印示例报告和用法：

```sh
./hid_mouse
```

```text
绝对鼠标（6 字节，hidg1）：
  abs  : 01 00 00 40 00 40
  wheel: 02 01
相对鼠标（4 字节，hidg2）：
  rel  : 00 64 32 00
```

发一次绝对坐标（画面中心是 16384,16384）：

```sh
./hid_mouse --abs /dev/hidg1 16384 16384
```

发一次相对位移和滚轮：

```sh
./hid_mouse --rel /dev/hidg2 100 50
./hid_mouse --wheel /dev/hidg1 1
```

点一下左键：

```sh
./hid_mouse --click /dev/hidg1 16384 16384
```

## 在被控设备上确认

绝对鼠标和相对鼠标是两个输入设备，分别抓：

```sh
sudo evtest /dev/input/by-id/usb-100ASK_KVM_tutorial_HID_KVM-TUTORIAL-1-if01-event-mouse
sudo evtest /dev/input/by-id/usb-100ASK_KVM_tutorial_HID_KVM-TUTORIAL-1-if02-event-mouse
```

实测输出（截取）：

```text
# 绝对鼠标：坐标、滚轮、点击
Event: type 3 (EV_ABS), code 0 (ABS_X), value 16384
Event: type 3 (EV_ABS), code 1 (ABS_Y), value 16384
Event: type 2 (EV_REL), code 8 (REL_WHEEL), value 1
Event: type 2 (EV_REL), code 11 (REL_WHEEL_HI_RES), value 120
Event: type 1 (EV_KEY), code 272 (BTN_LEFT), value 1
Event: type 1 (EV_KEY), code 272 (BTN_LEFT), value 0

# 相对鼠标：位移
Event: type 2 (EV_REL), code 0 (REL_X), value 100
Event: type 2 (EV_REL), code 1 (REL_Y), value 50
```

绝对鼠标的事件值是发送值本身；相对鼠标是每次位移，主机自己累加。`REL_WHEEL_HI_RES 120` 是内核对一格滚轮的换算。

## 清理

```sh
./hid-gadget.sh teardown
/etc/init.d/S99kvm start
```

## 验收

- 打印模式输出两种报告，绝对报告第 1 字节是 `01`、相对报告为 4 字节；
- 绝对坐标事件值等于发送值，相对位移在主机侧累加，滚轮和左键都有事件；
- 没有卡键或卡住的按钮；
- teardown 后 KVM 恢复，状态 `configured`。

## 常见问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| 鼠标不动 | 写到了键盘节点，或节点没枚举 | 绝对用 `hidg1`、相对用 `hidg2` |
| 坐标跳变 | 用了相对鼠标节点发绝对坐标 | 两种报告不能混用 |
| 点击没反应 | 只写了按下就退出 | 按下后要延迟再写释放 |
| 滚轮无效 | 相对鼠标的第 4 字节写了 0 | 滚轮值放在第 4 字节，或走绝对鼠标 Report ID 2 |