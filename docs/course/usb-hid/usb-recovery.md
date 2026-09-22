---
title: "处理 USB 冲突、断线与状态"
sidebar_label: "18 处理 USB 异常"
sidebar_position: 18
slug: /course/usb-hid/usb-recovery
---

# 处理 USB 冲突、断线与状态

创建了 HID function，绑定 UDC 时仍可能失败。当前项目真实出现过以下错误，随后又出现 `/dev/hidg0` 不存在：

```text
udc-core: couldn't find an available UDC or it's busy
write /sys/kernel/config/usb_gadget/kvm/UDC: device or resource busy
open /dev/hidg0: no such file or directory
```

**先解决 UDC 为什么忙，再检查下游 HID 节点**，不能把三个错误当成三个独立故障。

## 找出控制器的占用者

在串口或网络终端只读检查：

```sh
ls /sys/class/udc
for f in /sys/kernel/config/usb_gadget/*/UDC; do
    [ -f "$f" ] || continue
    printf '%s: ' "$f"
    cat "$f"
done
ps | grep -E 'adbd|kvm' | grep -v grep
```

**单个 UDC 同时只能绑定一个 Gadget**。这个 Gadget 可以包含多个 function，但 ADB 和 KVM 分别创建的两个 Gadget 不能同时争用同一个控制器。

## 停止进程不等于解除绑定

当前板端 `S50adb_start` 的 stop 分支仅停止 adbd，没有完整清理 Gadget。因此**停掉 ADB 后，仍需核对其 `UDC` 属性是否为空**。

确认具体占用者、停止它的管理服务后，向**那个 Gadget** 的 UDC 写入换行可解除绑定。以下 `adb` 只是说明路径角色，实际使用必须替换为前一步查到的目录：

```sh
printf '\n' > /sys/kernel/config/usb_gadget/adb/UDC
```

这会让对应 USB 功能从主机断开，因此不能在依赖同一 ADB 的唯一终端中操作。随后让 KVM 正常初始化，并检查主机重新枚举。当前 USB HID 服务通过 Gadget 的 UDC 属性绑定与解绑，不通过卸载平台 USB 控制器驱动恢复。配套 `S99kvm start` 会停止 ADB，并释放约定的 `g1` Gadget；若固件更改了 ADB 目录，应核对启动脚本，不能由 HID 服务抢占其他 Gadget。

## 三层状态一起检查

| 层次 | 检查位置 | 成功含义 |
| --- | --- | --- |
| Gadget 绑定 | `kvm/UDC` | 功能集合已交给该控制器 |
| USB 主机枚举 | 主机设备管理器或 Linux USB 日志 | 主机识别了设备与接口 |
| 实际输入 | 空白编辑器、鼠标移动 | 报告被主机正确解释 |

USB HID 服务上报绑定、控制器状态和键盘灯。本板通过 `KVM_USB_STATE_QUIRK=bound-is-configured` 兼容控制器状态，因此网页的 configured 文本不能独立证明物理连接与枚举成功。读取 `/sys/class/udc/实际控制器/state` 时，也要与主机实际输入结合。

## 服务如何处理断线与释放按键

服务在连接 EOF、异常消息、输入租约超时和正常退出时尝试释放键盘与鼠标按钮。输入写入最多等待 100 ms；更新 Gadget 配置前先释放输入并关闭 HID 文件，再解绑控制器。物理断开后无法保证释放报告送达，恢复时还需检查主机状态和重新发现的节点。避免后台恢复与人工配置同时进行，否则刚修复的状态可能又被另一个管理者覆盖。

验收记录应区分**启动冲突、物理断线、网络会话关闭和应用重启**。每种情况都检查恢复后是否能正常输入，是否存在卡键或重复 Gadget。
