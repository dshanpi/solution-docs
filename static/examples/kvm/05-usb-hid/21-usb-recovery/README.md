# 第二十一课：处理 USB 冲突、断线与状态

USB 键鼠出问题通常不是单点故障：UDC 被别的 Gadget 占着、服务停了但绑定还在、节点消失、主机那边按键卡住。这一课的脚本把检查、释放输入、重新绑定三件事分开做，先在板子上看清状态，再动手恢复。

## 文件

| 文件 | 用途 |
| --- | --- |
| `usb-recover.sh` | `status` 查状态、`release` 释放卡住的按键、`rebind` 重新绑定 KVM |

## 传到板子

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

## 查状态

```sh
./usb-recover.sh status
```

输出分四段：控制器状态、每个 Gadget 的 `UDC`、ADB/KVM 进程、HID 节点与 `report_length`。先确认谁绑着控制器、哪个节点对应键盘或鼠标。

## 释放卡住的输入

主机侧一直重复输入或按钮按着不放时，向所有 HID 节点写全零报告：

```sh
./usb-recover.sh release
```

脚本按每个功能的 `report_length` 写对应长度的零，键盘 8 字节、绝对鼠标 6 字节、相对鼠标 4 字节。

## 重新绑定 KVM

确认教学 Gadget 已经 teardown 后执行：

```sh
./usb-recover.sh rebind
```

脚本停 KVM、清空所有 Gadget 的 `UDC`、再启动 KVM，最多等 8 秒看控制器是否回到 `configured`，最后打印完整状态。

## 验收

- `status` 能指出控制器被哪个 Gadget 占用、节点对应的功能；
- `release` 后主机侧卡住的按键或按钮收到释放；
- `rebind` 后 UDC 状态 `configured`，`/dev/hidg*` 恢复，KVM 四个服务都在。

## 常见问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| `rebind` 报教学 Gadget 还在 | 第 18 课的对象没清理 | 先 `hid-gadget.sh teardown` |
| `release` 写入失败 | 节点没枚举或主机未连接 | 先看 `status` 和 UDC 状态 |
| 状态一直 `not attached` | 主机没重新枚举 | 重新插拔 USB，检查被控主机日志 |
| 仍有卡键 | 报告没送达主机 | 物理断开时无法保证释放，重连后再发一次 |