# 第十九课：编写 USB 键盘报告程序

往键盘节点写 8 字节 HID 报告，让被控设备真的收到一次按键。默认只打印报告内容，加 `--send` 才真正写入。

## 文件

| 文件 | 用途 |
| --- | --- |
| `hid_keyboard.c` | 构造并发送键盘报告 |
| `Makefile` | 用 SDK 的交叉编译器编译 |

## 报告格式

| 字节 | 含义 |
| --- | --- |
| 0 | 修饰键位图（左 Ctrl 0x01、左 Shift 0x02、左 Alt 0x04、左 GUI 0x08，右侧在 0x10–0x80） |
| 1 | 保留 |
| 2–7 | 最多 6 个同时按下的键码 |

按下是"某字节出现键码"，释放是把整份报告清零。主机保持上一份报告的状态，所以只按不释放会一直重复输入。

## 编译

```bash
make SDK_ROOT=~/T527_Tina5.0_SDK
```

## 准备键盘节点

用第 18 课的脚本建键鼠 Gadget（它会先释放 KVM 的功能）：

```sh
UDC=4100000.udc-controller ./hid-gadget.sh setup
```

输出里键盘对应 `/dev/hidg0`，`report_length` 为 8。

## 运行

先只打印报告，确认内容：

```sh
./hid_keyboard
```

```text
press  : 00 00 04 00 00 00 00 00
release: 00 00 00 00 00 00 00 00
真正发送：hid_keyboard --send /dev/hidg0 [键码]
```

真正发送 a 键（`0x04`），程序会写一次按下、等 100 ms、再写一次释放：

```sh
./hid_keyboard --send /dev/hidg0
```

## 在被控设备上确认

用 `evtest` 抓键盘事件：

```sh
sudo evtest /dev/input/by-id/usb-100ASK_KVM_tutorial_HID_KVM-TUTORIAL-1-event-kbd
```

实测输出（截取）：

```text
Event: time ..., type 1 (EV_KEY), code 30 (KEY_A), value 1
Event: time ..., -------------- SYN_REPORT ------------
Event: time ..., type 1 (EV_KEY), code 30 (KEY_A), value 0
Event: time ..., -------------- SYN_REPORT ------------
```

`value 1` 是按下，`value 0` 是释放。Linux 把 HID 键码 `0x04` 映射成 `KEY_A`（事件码 30）；具体输入什么字符由被控设备的键盘布局和输入法决定。

## 清理

```sh
./hid-gadget.sh teardown
/etc/init.d/S99kvm start
```

## 验收

- 打印模式输出两行报告，按下报告第 2 字节是 `04`；
- `--send` 退出码为 0，被控设备上出现 KEY_A 的按下和释放各一次；
- 键盘没有卡键；
- teardown 后 KVM 恢复，状态回到 `configured`。

## 常见问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| `HID 节点不可写` | 节点没枚举，或 KVM 服务占着 | 先建教学 Gadget，确认 `configured` |
| 报告只写了 N/8 字节 | 写入长度和 `report_length` 不一致 | 报告固定 8 字节 |
| 被控设备没反应 | 焦点不在接收输入的地方 | 打开空白编辑器或看 `evtest` 输出 |
| 字符一直重复 | 没有发释放报告 | 按下后必须写全零报告 |