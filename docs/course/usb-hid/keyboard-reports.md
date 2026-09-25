---
title: "编写 USB 键盘报告程序"
sidebar_label: "16 编写键盘程序"
sidebar_position: 16
slug: /course/usb-hid/keyboard-reports
---

# 编写 USB 键盘报告程序

主机枚举到键盘以后，并不接收字符串 “a”。应用发送的是 HID 报告：哪些修饰键按下、哪些普通键仍然保持按下。**字符最后由主机的键盘布局和输入法解释**。

## 描述符规定的 8 字节

USB HID 服务在 `kvm-hid-service/c/descriptors.h` 中定义无 Report ID 的 8 字节键盘输入报告，`gadget.c` 的 `gadget_report()` 负责构造并写入，与配套实验描述符一致。

| 字节 | 内容 | 例子 |
| --- | --- | --- |
| 0 | Ctrl、Shift、Alt、GUI 等修饰键位图 | 左 Shift 为 bit 1 |
| 1 | 保留，填 0 | `00` |
| 2—7 | 最多六个普通键的 Usage ID | `04` 表示 Keyboard a/A |

```text
按下 a：00 00 04 00 00 00 00 00
释放：  00 00 00 00 00 00 00 00
```

图 1：一次按键的两份状态报告。

**`0x04` 是 HID Usage ID，不是 ASCII 字符 `a` 的编码**。

主机持有的按键状态持续到下一份报告改变它。**只有按下、没有释放，可能导致重复输入**；USB 写入返回成功也不表示应用已经观察到正确字符。[Linux HID Gadget 文档](https://docs.kernel.org/usb/gadget_hid.html)说明了通过 `hidgX` 发送和接收报告的接口。

## 先显示字节，再发送报告

下载 [hid_keyboard.c](/examples/kvm/hid_keyboard.c) 到 SDK 下 `tutorial-examples/`。从 SDK 根目录编译：

```bash
"${CROSS_COMPILE}gcc" -std=c11 -Wall -Wextra -O2 \
  tutorial-examples/hid_keyboard.c -o tutorial-examples/hid_keyboard
```

复制到板端实验目录，先运行默认模式：

```sh
./hid_keyboard
```

默认只打印按下和释放的字节，不控制设备。真正发送前，在被控设备打开空白文本编辑器，确认焦点位于编辑区；确认当前节点确实使用上述描述符，且没有其他键盘报告写入者。

```sh
./hid_keyboard --send /dev/hidg0
```

示例非阻塞打开设备，等待可写，发送按下报告，短暂保持，再发送全零释放。它已进行交叉编译检查，实际主机输入和异常恢复仍需板端验证。

## write 的长度也属于协议

Socket 是字节流，可以循环发送剩余字节；**HID 的一次写入对应一份报告**，不能把短写的剩余部分当成另一份正确报告继续发送。示例要求一次返回完整 8 字节，不符就报错。

如果 `open` 报节点不存在，回到 Gadget 配置；如果等待可写超时，检查线缆、USB Device 口、主机枚举和 UDC 绑定。不要通过不断重建整个 Gadget 来掩盖尚未连接主机的问题。

## 修饰键与 LED 是两个方向

左 Shift 加 `0x04` 的报告首字节为 `0x02`，主机通常会按当前布局解释成大写输入。Num Lock、Caps Lock 等 LED 状态则由主机发送回来，项目使用读取路径处理；发送全零输入报告不会自动修改主机的锁定键状态。

验收保留实际输入结果，并确认按键没有持续卡住。下一章把同样的描述符与报告关系应用到鼠标。

<!-- LYNX-LAB:BEGIN -->

## AI 辅助实践闭环

- **稳定步骤**：`t527-kvm-16`
- **学习目标**：理解“编写 USB 键盘报告程序”在 T527 Web KVM 链路中的职责，并能用证据解释结果。
- **理解检查**：说明本章输入、输出以及失败时最先检查的证据。
- **学生操作**：在锁定的 Avaota A1 SDK 学习工作区完成“编写 USB 键盘报告程序”实验，不直接修改其他板型。
- **工具范围**：`code`
- **风险级别**：`software-safe`
- **预期现象**：命令退出码为 0，并生成带 SHA-256 的结构化证据。

确定性验证：

```bash
cd tutorial-examples/web-kvm
./scripts/run_simulation.sh keyboard
```

必须保存命令退出码、产物摘要和证据来源；模拟结果只能标记为 `simulation`。完成后回答：解释本章结果为何可信，并指出哪些结论仍需要真实硬件。

<details>
<summary>分级提示</summary>

1. 先确认本章输入、输出与证据来源。
2. 检查 ./scripts/run_simulation.sh keyboard 的首个失败阶段。
3. 只对当前步骤相关文件做最小修改，并重新生成一次独立 attempt。
4. 参考已签名课程包中的 solution/16，报告标记为 ASSISTED_PASS。

</details>

<!-- LYNX-LAB:END -->
