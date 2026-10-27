---
title: "编写 USB 键盘报告程序"
sidebar_label: "19 编写键盘程序"
sidebar_position: 16
slug: /course/usb-hid/keyboard-reports
---

# 编写 USB 键盘报告程序

上一课被控设备认到了键盘，但还没收到过按键。这一课写一个程序，往键盘节点写报告，让被控设备上真的出现一次按键。

## 一份报告长什么样

键盘报告固定 8 字节，就是描述符里声明的布局：

| 字节 | 含义 |
| --- | --- |
| 0 | 修饰键位图 |
| 1 | 保留 |
| 2–7 | 最多 6 个同时按下的键码 |

- 修饰键位：左 Ctrl `0x01`、左 Shift `0x02`、左 Alt `0x04`、左 GUI `0x08`，右侧分别是 `0x10`、`0x20`、`0x40`、`0x80`。
- 键码用 HID 用法号：a 到 z 是 `0x04`–`0x1D`，数字 1–0 是 `0x1E`–`0x27`，回车 `0x28`、Esc `0x29`、空格 `0x2C`。
- **按下**就是某个字节里出现键码，**释放**是把整份报告清零。主机保持上一份报告的状态，只按不释放会一直重复输入。
- 写入长度必须是 8。HID 是报告接口，短写不会被拆成下一份报告。

## 写这个程序

程序分五步，默认只打印报告内容，加 `--send` 才真正写节点。

### 打开设备节点

```c
fd = open(argv[2], O_WRONLY | O_NONBLOCK);
```

### 等节点可写

```c
struct pollfd p = {.fd = fd, .events = POLLOUT};
do {
	ready = poll(&p, 1, 2000);
} while (ready < 0 && errno == EINTR);
if (ready <= 0 || !(p.revents & POLLOUT)) {
	fprintf(stderr, "HID 节点不可写，检查 USB 是否枚举成功\n");
	return -1;
}
```

节点不可写，通常说明 Gadget 没绑定好，或者主机还没枚举完。

### 写按下报告

```c
uint8_t press[8] = {0};
press[2] = keycode;      /* 0x04 是键盘上的 a */
write(fd, press, 8);
```

### 等一会儿再写释放报告

```c
uint8_t release[8] = {0};
struct timespec pause = {.tv_sec = 0, .tv_nsec = 100 * 1000 * 1000};
nanosleep(&pause, &pause);
write(fd, release, 8);
```

间隔 100 ms 是给主机区分"按下"和"释放"两次报告。

### 失败也要释放

如果按下写入失败，仍要尝试写释放报告，否则主机侧会留下一个按下的键。程序最后把两次写入的结果一起作为退出码。

完整程序在课件 [19-keyboard](/examples/kvm/05-usb-hid/19-keyboard/hid_keyboard.c)，可以对照。

## 运行与验证

### 准备文件

板端需要第 18 课的 Gadget 脚本和这一课的 `hid_keyboard`。在开发主机进入课件目录，起一个临时 HTTP 服务：

```bash
cd <课件目录>
python3 -m http.server 8000
```

板端另开终端下载，和主机 `md5sum` 对照：

```sh
mkdir -p /tmp/kbd && cd /tmp/kbd
wget -q -O hid-gadget.sh http://192.168.1.69:8000/hid-gadget.sh
wget -q -O hid_keyboard http://192.168.1.69:8000/hid_keyboard
chmod +x hid-gadget.sh hid_keyboard
md5sum hid-gadget.sh hid_keyboard
```

### 建键鼠 Gadget

```sh
UDC=4100000.udc-controller ./hid-gadget.sh setup
```

输出里的键盘就是 `/dev/hidg0`。确认节点和报告长度：

```sh
ls -l /dev/hidg0
cat /sys/kernel/config/usb_gadget/kvm_tutorial/functions/hid.usb0/report_length
```

第二条应输出 `8`。

### 运行

先只打印，确认报告内容：

```sh
./hid_keyboard
```

```text
press  : 00 00 04 00 00 00 00 00
release: 00 00 00 00 00 00 00 00
真正发送：hid_keyboard --send /dev/hidg0 [键码]
```

再真正发一次：

```sh
./hid_keyboard --send /dev/hidg0
```

```text
press  : 00 00 04 00 00 00 00 00
release: 00 00 00 00 00 00 00 00
已发送键码 0x04 并释放
```

在被控设备上抓键盘事件，实测输出：

```sh
sudo evtest /dev/input/by-id/usb-100ASK_KVM_tutorial_HID_KVM-TUTORIAL-1-event-kbd
```

```text
Event: time ..., type 1 (EV_KEY), code 30 (KEY_A), value 1
Event: time ..., -------------- SYN_REPORT ------------
Event: time ..., type 1 (EV_KEY), code 30 (KEY_A), value 0
Event: time ..., -------------- SYN_REPORT ------------
```

`value 1` 是按下，`value 0` 是释放。Linux 把 HID 键码 `0x04` 映射成 `KEY_A`（事件码 30）；最终打出什么字符由被控设备的键盘布局和输入法决定。`evtest` 开头还会列出描述符声明的 LED 能力（`EV_LED`），那是主机初始化时写的一次输出报告。

也可以在被控设备打开空白编辑器，直接看是不是打出了 `a`。

## 清理

```sh
cd /tmp/kbd
./hid-gadget.sh teardown
/etc/init.d/S99kvm start
```

## 验收

- 打印模式输出两行报告，按下报告第 2 字节是 `04`；
- `--send` 退出码为 0，被控设备上出现 KEY_A 的按下和释放各一次；
- 键盘没有卡键；
- teardown 后 KVM 恢复，UDC 状态回到 `configured`。

## 常见问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| `HID 节点不可写` | 节点没枚举，或 KVM 服务占着 | 先建教学 Gadget，确认 `configured` |
| 报告只写了 N/8 字节 | 写入长度和 `report_length` 不一致 | 报告固定 8 字节 |
| 被控设备没反应 | 焦点不在接收输入的地方 | 打开空白编辑器，或看 `evtest` 输出 |
| 字符一直重复 | 没有发释放报告 | 按下后必须写全零报告 |
| 打出的字符不对 | 键码或修饰键位写错 | 对照报告布局和键码表检查 |