---
title: "让 T527 枚举成 USB HID 设备"
sidebar_label: "18 配置 USB Gadget"
sidebar_position: 15
slug: /course/usb-hid/gadget-config
---

# 让 T527 枚举成 USB HID 设备

这一课把 T527 配成一套 USB 键鼠：键盘、绝对鼠标、相对鼠标。配好后被控设备重新枚举，会同时出现这三种输入设备。配套 KVM 就是这么配的，这一课自己建一套、验证，再删掉恢复。

## 目标与结果

- 板端出现三个 HID 节点：`/dev/hidg0` 键盘、`/dev/hidg1` 绝对鼠标、`/dev/hidg2` 相对鼠标，UDC 状态变成 `configured`。
- 被控设备出现一个键盘和两个鼠标：产品名 `KVM tutorial HID`，厂商 `100ASK`；Linux 上 `/dev/input/by-id/` 会有 `usb-100ASK_KVM_tutorial_HID_KVM-TUTORIAL-1-event-kbd` 和两个 `...-event-mouse`。
- 清理后 `S99kvm start`，KVM 自己的键鼠接回来。

## HID 功能的数量限制

内核里 HID 功能实例总数上限是 4 个（`drivers/usb/gadget/function/f_hid.c` 里的 `HIDG_MINORS`）。配套 KVM 的 `kvm` Gadget 已经占了键盘、绝对鼠标、相对鼠标三个，所以教学 Gadget 建三个功能之前，**必须停 KVM、解绑并删掉它那三个功能**。这也是为什么这一课不能直接"再叠加一套"。

实验结束执行 `/etc/init.d/S99kvm start`，服务会把自己的三个功能重建回来。

## 描述符与报告格式

每个 HID 功能都要一份报告描述符，它定义主机如何解析报告。这一课用三份描述符，脚本把它们以 base64 内嵌：

| 功能 | protocol | subclass | report_length | 报告布局 |
| --- | --- | --- | --- | --- |
| 键盘 | 1 | 1 | 8 | 修饰键位图、保留字节、最多 6 个键码 |
| 绝对鼠标 | 2 | 0 | 6 | Report ID 1 绝对坐标（0–32767）、Report ID 2 滚轮 |
| 相对鼠标 | 2 | 1 | 4 | 按键、X 位移、Y 位移、滚轮 |

`report_length` 是这一功能最大输入报告的字节数，必须和描述符一致。

### 键盘描述符在说什么

描述符由一组「项」组成，每项声明用途、报告字段大小和数量。键盘这份大致是：

| 片段 | 含义 |
| --- | --- |
| `05 01 09 06 A1 01` | 用途页为通用桌面，用途为键盘，开始一个应用集合 |
| `05 07 19 E0 29 E7 75 01 95 08 81 02` | 8 个修饰键（Ctrl/Shift/Alt/GUI），每键 1 位，作为输入 |
| `95 01 75 08 81 03` | 1 字节保留 |
| `05 08 19 01 29 05 91 02` + `95 01 75 03 91 03` | 5 位 LED 输出报告（Num/Caps/Scroll 等）加 3 位填充 |
| `95 06 75 08 15 00 25 65 05 07 19 00 29 65 81 00` | 最多 6 个键码，取值 0–0x65 |
| `C0` | 结束集合 |

这些片段拼起来，就是 8 字节报告的字段顺序：第 0 字节修饰键，第 1 字节保留，第 2–7 字节是键码。鼠标描述符同理，只是字段换成按键位图、坐标和滚轮；绝对鼠标用 Report ID 区分「绝对坐标」和「滚轮」两种报告。

## 配置脚本逐段说明

描述符和属性都定了，接下来要写一个脚本，把这些操作一次做完。为什么不直接在终端敲？因为创建 Gadget 要十几条命令，还要按顺序释放项目 Gadget 的功能、建对象、加功能、最后绑定，漏一步就要从头清理。写成脚本可以反复执行，出错也好重来。

脚本分四步：先释放 HID 功能名额，再建 Gadget 的基础对象，然后添加三个功能并链接到配置，最后绑定 UDC。下面一段段写。

### 第一步：定义路径和描述符

```sh
G=/sys/kernel/config/usb_gadget/kvm_tutorial   # 要建的 Gadget
K=/sys/kernel/config/usb_gadget/kvm            # 项目 Gadget，待释放
KBD_B64=... ABS_B64=... REL_B64=...            # 三份描述符，base64 内嵌
```

描述符按上一节的 HID 项写成字节，再 base64 编码成一行字符串放进脚本。这样脚本不依赖外部文件，复制到哪里都能跑。

路径和描述符就绪，但 HID 功能名额还被 KVM 占着，所以第二步先把它释放出来。

### 第二步：释放项目 Gadget 的 HID 功能

```sh
/etc/init.d/S99kvm stop
printf '\n' > "$K/UDC"          # 解绑：写空行，停服务不会自动解绑
free_service_functions           # 删掉 kvm 的三个链接和功能目录
```

`free_service_functions` 先 `rm` 配置里的链接，再 `rmdir` 功能目录，顺序不能反——链接还在时目录删不掉。这一步腾出 HID 功能名额。

名额空出来后，第三步给新 Gadget 立身份，再往里放功能。

### 第三步：建立 Gadget 基础对象

```sh
mkdir -p "$G"
printf '0x1d6b' > "$G/idVendor"      # 主机看到的 VID
printf '0x0104' > "$G/idProduct"     # 主机看到的 PID
printf '0x0200' > "$G/bcdUSB"        # USB 2.0
printf '0x0100' > "$G/bcdDevice"     # 设备版本
mkdir -p "$G/strings/0x409"          # 主机显示的名字
mkdir -p "$G/configs/c.1/strings/0x409"
```

| 对象 | 作用 |
| --- | --- |
| `idVendor` / `idProduct` | 主机识别的设备身份 |
| `bcdUSB` / `bcdDevice` | USB 版本与设备版本 |
| `strings/0x409/*` | 厂商、产品名、序列号 |
| `configs/c.1` | 一组功能集合，对应主机看到的一个配置 |

这些是普通属性文件，写入即生效；`strings` 用的是 `0x409`（英文）语言目录。

身份和字符串就位，第四步往里加三个 HID 功能，并把它们交给配置。

### 第四步：添加 HID 功能并绑定

```sh
mkdir "$G/functions/hid.usb0"
printf '1' > "$G/functions/hid.usb0/protocol"
printf '1' > "$G/functions/hid.usb0/subclass"
printf '8' > "$G/functions/hid.usb0/report_length"
printf '%s' "$KBD_B64" | base64 -d > "$G/functions/hid.usb0/report_desc"
ln -s "$G/functions/hid.usb0" "$G/configs/c.1/hid.usb0"
```

两次重复同样的步骤，只是换成绝对鼠标（`hid.usb1`，6 字节，额外写 `no_out_endpoint=1`）和相对鼠标（`hid.usb2`，4 字节）。

| 属性 | 含义 |
| --- | --- |
| `protocol` | 1 键盘、2 鼠标 |
| `subclass` | 1 表示 boot 接口；绝对鼠标用 0，因为它带 Report ID |
| `report_length` | 最大输入报告长度 |
| `report_desc` | 报告描述符二进制内容 |
| `no_out_endpoint` | 置 1 表示不占用 OUT 端点 |

**只建功能还不够**：功能必须先软链接到 `configs/c.1`，才属于主机看到的那个配置。

最后一步才是绑定：

```sh
printf '%s' "$UDC" > "$G/UDC"
```

把控制器名写进 `UDC`，内核才把这组功能挂到 USB 上，主机开始枚举。脚本随后轮询 `/sys/class/udc/<UDC>/state`，直到变成 `configured` 再打印结果。

脚本用 `functions/hid.usbN/dev` 里的 `major:minor` 去 `/sys/class/hidg/` 里匹配，得到实际节点名，所以编号变化也能找到。

四段写在一起，加上 `setup|teardown|status` 的选择，就是一个完整的 `hid-gadget.sh`。课件里有一份写好的完整脚本可以对照，但建议先照着上面的顺序自己写一遍，写不出来再对。

## 创建与验证

先把这个脚本放到板端。在开发主机进入脚本所在目录，起一个临时 HTTP 服务：

```bash
cd <脚本所在目录>
python3 -m http.server 8000
```

板端下载并和主机的 `md5sum` 对照：

```sh
mkdir -p /tmp/tg && cd /tmp/tg
wget -q -O hid-gadget.sh http://192.168.1.69:8000/hid-gadget.sh
chmod +x hid-gadget.sh
md5sum hid-gadget.sh
```

然后执行 `setup`：

```sh
UDC=4100000.udc-controller ./hid-gadget.sh setup
```

输出：

```text
已绑定 4100000.udc-controller
键盘：/dev/hidg0（8 字节）
绝对鼠标：/dev/hidg1（6 字节）
相对鼠标：/dev/hidg2（4 字节）
UDC 状态：configured
```

在被控设备上确认：

```sh
dmesg | tail -20
ls /dev/input/by-id/ | grep tutorial
```

实测输出：

```text
usb 1-1.2: Product: KVM tutorial HID
usb 1-1.2: Manufacturer: 100ASK
usb 1-1.2: SerialNumber: KVM-TUTORIAL-1
hid-generic ...: USB HID v1.01 Keyboard [100ASK KVM tutorial HID] on .../input0
hid-generic ...: USB HID v1.01 Mouse [100ASK KVM tutorial HID] on .../input1
hid-generic ...: USB HID v1.01 Mouse [100ASK KVM tutorial HID] on .../input2
```

`input0` 是键盘，`input1`、`input2` 是两个鼠标。随时可用 `./hid-gadget.sh status` 查看当前节点和状态；板端 `configured` 表示主机完成枚举，`/dev/hidgN` 只说明内核建了节点。

## 清理与恢复

验证通过后要按建立的逆序拆掉自己建的对象。和 `setup` 一样，这几步也写进脚本的 `teardown` 分支：

```sh
printf '\n' > "$G/UDC"            # 解绑，先断开主机
rm -f "$G/configs/c.1/hid.usb0"   # 删掉配置里的链接
rmdir "$G/functions/hid.usb0"     # 再删功能目录
rmdir "$G/configs/c.1" "$G/strings/0x409" "$G"   # 最后删字符串和 Gadget
```

顺序反过来很关键：主机还连着就删对象会失败，链接没删就删功能目录也删不掉。三个功能各做一遍。

清理完启动 KVM，让它把自己的键鼠接回来：

```sh
./hid-gadget.sh teardown
/etc/init.d/S99kvm start
```

`teardown` 按建立的逆序清理：解绑 `UDC`、删配置链接、删功能目录、删字符串和配置、删 Gadget。重启 KVM 后服务的三个功能重建，状态回到 `configured`，`/dev/hidg0..2` 恢复，被控设备侧换回 `KVM USB Emulation Device`。

## 与项目 Gadget 的关系

配套镜像里的 KVM 服务也用同一套 configfs 机制：`idVendor=0x1d6b`、`idProduct=0x0104`，三个 HID 功能，属性值就是上面表中的 8/6/4。区别只是它开机就创建并常驻，教学脚本用完就删。

## 验收

- 三个功能的 `report_length` 为 8、6、4，节点都能找到；
- 绑定 UDC 后状态为 `configured`，被控设备同时出现键盘和两个鼠标；
- teardown 后目录清空，`S99kvm start` 后 KVM 状态恢复 `configured`。

## 常见问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| 创建功能报 `No such device` | HID 功能到 4 个上限 | 先释放项目 Gadget 的三个功能 |
| UDC 写入报 `busy` | `kvm` Gadget 还绑着控制器 | 停服务后向 `kvm/UDC` 写换行 |
| 状态一直是 `not attached` | 主机没重新枚举或 USB 线未接 | 重新插拔 USB，检查被控主机 |
| 只认到部分设备 | 功能没链接到配置，或描述符写错 | 检查 `configs/c.1/hid.usbN` 和 `dmesg` |
| 节点不是 `hidg0..2` | 已有其他 HID 功能占用编号 | 按功能的 `dev` 属性查实际节点 |
| 报告写入长度不对 | `report_length` 与描述符不一致 | 对照上表检查功能属性 |