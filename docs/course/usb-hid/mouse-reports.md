---
title: "编写相对鼠标和绝对鼠标程序"
sidebar_label: "20 编写鼠标程序"
sidebar_position: 17
slug: /course/usb-hid/mouse-reports
---

# 编写相对鼠标和绝对鼠标程序

上一课往键盘节点写了报告。鼠标要复杂一点：远程操作既需要"向右移动一点"，也需要"指向画面里的某个位置"。前者用相对鼠标，后者用绝对鼠标，两种报告布局不同，不能混用。

## 两种鼠标的区别

| 鼠标 | 报告内容 | 主机怎么用 | 节点 | report_length |
| --- | --- | --- | --- | --- |
| 相对 | 每次移动了多少 | 累加到当前位置 | `/dev/hidg2` | 4 |
| 绝对 | 应该在哪里（0–32767） | 直接跳到该位置 | `/dev/hidg1` | 6 |

相对位移适合连续拖动，绝对坐标适合点画面里的固定位置；浏览器窗口缩放或留黑边时，绝对坐标要按实际视频区域换算。

## 两份报告长什么样

**绝对鼠标（6 字节）**：

| 字节 | 含义 |
| --- | --- |
| 0 | Report ID = `0x01` |
| 1 | 按键位图：左 `0x01`、右 `0x02`、中 `0x04` |
| 2–3 | X 坐标，16 位小端，0–32767 |
| 4–5 | Y 坐标，16 位小端，0–32767 |

滚轮走另一份报告：`Report ID 2` 加一字节滚轮值，共 2 字节。`report_length=6` 是这一功能**最大**输入报告长度，滚轮只用 2 字节也合法。

**相对鼠标（4 字节，无 Report ID）**：

| 字节 | 含义 |
| --- | --- |
| 0 | 按键位图 |
| 1 | X 位移，8 位有符号（-127–127） |
| 2 | Y 位移，8 位有符号 |
| 3 | 滚轮，8 位有符号 |

## 写这个程序

两种鼠标各写一条路径，都先组报告再写节点。

### 打开对应的节点

```c
fd = open(path, O_WRONLY | O_NONBLOCK);
```

绝对鼠标用 `hidg1`，相对鼠标用 `hidg2`；写错节点主机会按错误的格式解析。

### 组绝对鼠标报告

```c
uint8_t report[6] = {0x01, buttons, 0, 0, 0, 0};
report[2] = (uint8_t)(x & 0xff);
report[3] = (uint8_t)((x >> 8) & 0xff);
report[4] = (uint8_t)(y & 0xff);
report[5] = (uint8_t)((y >> 8) & 0xff);
```

坐标是小端：低字节在前。中心点是 16384。

### 组相对鼠标报告

```c
uint8_t report[4] = {buttons, (uint8_t)(int8_t)dx, (uint8_t)(int8_t)dy,
                     (uint8_t)(int8_t)wheel};
```

位移是有符号数，向左/向上是负值。

### 写整份报告

```c
struct pollfd p = {.fd = fd, .events = POLLOUT};
poll(&p, 1, 2000);
write(fd, report, sizeof(report));
```

长度要和该功能的报告长度对应；短写不会被补成完整报告。

### 点击和滚轮

点击 = 按键位写 `0x01` 发一次，等 100 ms，再把按键位清 0 发一次；否则主机侧会一直按着左键。滚轮在相对鼠标里是第 4 字节，在绝对鼠标里是 `Report ID 2` 的两字节报告。

完整程序在课件 [20-mouse](/examples/kvm/05-usb-hid/20-mouse/hid_mouse.c)，可以对照。

## 运行与验证

### 准备文件

板端需要两样东西：第 18 课的 Gadget 脚本和这一课的 `hid_mouse`。在开发主机进入课件目录，起一个临时 HTTP 服务：

```bash
cd <课件目录>
python3 -m http.server 8000
```

另开一个板端终端，下载并和主机的 `md5sum` 对照：

```sh
mkdir -p /tmp/mouse && cd /tmp/mouse
wget -q -O hid-gadget.sh http://192.168.1.69:8000/hid-gadget.sh
wget -q -O hid_mouse http://192.168.1.69:8000/hid_mouse
chmod +x hid-gadget.sh hid_mouse
md5sum hid-gadget.sh hid_mouse
```

把地址换成开发主机的实际 IP；散列值不一致就先别继续。

### 建键鼠 Gadget

脚本会停 KVM、解绑并释放它的三个 HID 功能，再建键盘和两个鼠标：

```sh
UDC=4100000.udc-controller ./hid-gadget.sh setup
```

```text
已绑定 4100000.udc-controller
键盘：/dev/hidg0（8 字节）
绝对鼠标：/dev/hidg1（6 字节）
相对鼠标：/dev/hidg2（4 字节）
UDC 状态：configured
```

确认节点和报告长度：

```sh
ls -l /dev/hidg1 /dev/hidg2
cat /sys/kernel/config/usb_gadget/kvm_tutorial/functions/hid.usb1/report_length
cat /sys/kernel/config/usb_gadget/kvm_tutorial/functions/hid.usb2/report_length
```

两条 `cat` 应依次输出 `6` 和 `4`。

### 运行

先看示例报告和用法：

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

分别发一次：

```sh
./hid_mouse --abs /dev/hidg1 16384 16384
./hid_mouse --rel /dev/hidg2 100 50
./hid_mouse --wheel /dev/hidg1 1
./hid_mouse --click /dev/hidg1 16384 16384
```

在被控设备上，两个鼠标要分别抓（实测输出）：

```sh
sudo evtest /dev/input/by-id/usb-100ASK_KVM_tutorial_HID_KVM-TUTORIAL-1-if01-event-mouse
sudo evtest /dev/input/by-id/usb-100ASK_KVM_tutorial_HID_KVM-TUTORIAL-1-if02-event-mouse
```

```text
# 绝对鼠标
Event: type 3 (EV_ABS), code 0 (ABS_X), value 16384
Event: type 3 (EV_ABS), code 1 (ABS_Y), value 16384
Event: type 2 (EV_REL), code 8 (REL_WHEEL), value 1
Event: type 2 (EV_REL), code 11 (REL_WHEEL_HI_RES), value 120
Event: type 1 (EV_KEY), code 272 (BTN_LEFT), value 1
Event: type 1 (EV_KEY), code 272 (BTN_LEFT), value 0

# 相对鼠标
Event: type 2 (EV_REL), code 0 (REL_X), value 100
Event: type 2 (EV_REL), code 1 (REL_Y), value 50
```

绝对鼠标的事件值就是发送的坐标；相对鼠标每次报的是位移，主机自己累加。`REL_WHEEL_HI_RES 120` 是内核对一格滚轮的换算。绝对坐标最终落在屏幕哪个像素，由主机的输入系统按 0–32767 映射，浏览器里还要按视频内容区域再算一次。

## 清理与恢复

```sh
cd /tmp/mouse
./hid-gadget.sh teardown
/etc/init.d/S99kvm start
```

## 验收

- 打印模式输出两种报告，绝对报告第 1 字节是 `01`，相对报告是 4 字节；
- 绝对坐标事件值等于发送值，相对位移正确，滚轮和左键都有事件；
- 没有卡住的按钮；
- teardown 后 KVM 恢复，UDC 状态 `configured`。

## 常见问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| 鼠标不动 | 写到了键盘节点，或节点没枚举 | 绝对用 `hidg1`、相对用 `hidg2` |
| 坐标跳变 | 用相对节点发绝对坐标 | 两种报告不能混用 |
| 点击没反应 | 写完按下就退出 | 延迟 100 ms 再写释放 |
| 滚轮无效 | 相对鼠标第 4 字节写了 0 | 滚轮放第 4 字节，或走绝对鼠标 Report ID 2 |
| 指针位置和画面不符 | 浏览器缩放或留黑边 | 按实际视频内容区域换算坐标 |