# 第十八课：让 T527 枚举成 USB HID 设备

用 configfs 把 T527 配成一套 USB 键鼠：键盘、绝对鼠标、相对鼠标。配好后被控主机应同时枚举出这三种输入设备，实验结束再删掉、恢复 KVM。

## 文件

| 文件 | 用途 |
| --- | --- |
| `hid-gadget.sh` | 创建 / 查看 / 清理键鼠 Gadget，三份描述符按 base64 内嵌 |

## 为什么先要动 KVM 的功能

内核 HID 功能实例上限是 4 个（`HIDG_MINORS=4`）。项目 `kvm` Gadget 已占键盘、绝对鼠标、相对鼠标三个，所以脚本在 `setup` 里会停 KVM、解绑 UDC 并删掉这三个功能，然后建教学 Gadget 的三个功能。`teardown` 之后用 `/etc/init.d/S99kvm start` 让服务把功能重建回来。

## 运行

```sh
UDC=4100000.udc-controller ./hid-gadget.sh setup
UDC=4100000.udc-controller ./hid-gadget.sh status
UDC=4100000.udc-controller ./hid-gadget.sh teardown
/etc/init.d/S99kvm start
```

## 实测输出（2026-10-27）

```text
已绑定 4100000.udc-controller
键盘：/dev/hidg0（8 字节）
绝对鼠标：/dev/hidg1（6 字节）
相对鼠标：/dev/hidg2（4 字节）
UDC 状态：configured
```

teardown 后 `/etc/init.d/S99kvm start`，`kvm` 的三个功能重新出现，UDC 状态回到 `configured`，`/dev/hidg0..2` 恢复，四个服务进程都在；被控设备侧从 `KVM tutorial HID` 换回 `KVM USB Emulation Device`。

## 三种报告

| 功能 | report_length | 报告布局 |
| --- | --- | --- |
| 键盘 | 8 | 修饰键位图、保留字节、最多 6 个键码 |
| 绝对鼠标 | 6 | Report ID 1 绝对坐标（0–32767）、Report ID 2 滚轮 |
| 相对鼠标 | 4 | 按键、X 位移、Y 位移、滚轮 |

描述符按 HID 规范编写，脚本以 base64 内嵌。

## 验收

- `report_length` 为 8、6、4，三个节点都能找到；
- 绑定 UDC 后状态为 `configured`，被控设备同时出现键盘和鼠标；
- teardown 后目录清空，`S99kvm start` 后 KVM 状态恢复 `configured`。

## 常见问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| 创建功能报 `No such device` | HID 功能到 4 个上限 | 先释放项目 Gadget 的三个功能 |
| UDC 写入报 `busy` | `kvm` Gadget 还绑着控制器 | 停服务后向 `kvm/UDC` 写换行 |
| 状态一直是 `not attached` | 主机没重新枚举 | 重新插拔 USB，检查被控主机 |
| 只认到部分设备 | 配置没链接或描述符写错 | 检查 `configs/c.1/hid.usbN` 和 `dmesg` |