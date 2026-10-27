# Web KVM 教学示例

这些是独立教学辅助文件，和配套软件分开维护。

例子按学习阶段分目录，阶段号与课程一致；每个阶段里的子目录以章号开头，与侧栏编号对应。

```text
03-capture/                 阶段三 图像采集
├─ 12-query-format          查询格式
├─ 13-first-frame           采集第一帧
└─ 14-capture-loop          连续采集
04-encoding/                阶段四 视频编码
├─ 15-encode-buffer
├─ 16-h264-file
└─ 17-performance
05-usb-hid/                 阶段五 USB 键鼠
├─ 18-gadget-config
├─ 19-keyboard
├─ 20-mouse
└─ 21-usb-recovery
06-integration/             阶段六 服务集成
├─ 23-video-socket
└─ 24-control-hid
```

第 22 课认识服务架构只用板端已有服务和系统命令，没有独立课件文件。

| 文件 | 用途 | 已检查范围 |
| --- | --- | --- |
| 03-capture/12-query-format/ | 查询 `/dev/video0` 能力和当前格式 | GCC 10.3 AArch64 编译，2026-10-26 样机实测，见目录内 README |
| 03-capture/13-first-frame/ | 采集一帧 NV12 并保存 | GCC 10.3 AArch64 编译，2026-10-26 样机取帧并用 FFmpeg 解码确认 |
| 03-capture/14-capture-loop/ | 连续采集并统计帧率、丢帧 | GCC 10.3 AArch64 编译，2026-10-26 样机 300 帧实测 |
| 04-encoding/15-encode-buffer/ | 采集缓冲区导入 VE，硬件编码一帧 | GCC 10.3 AArch64 交叉编译，2026-10-26 样机实测 |
| 04-encoding/16-h264-file/ | 连续编码 120 帧并写 Annex-B 文件 | GCC 10.3 AArch64 交叉编译，2026-10-26 样机编码并用 FFmpeg 解码确认 |
| 04-encoding/17-performance/ | 测量编码耗时、帧率和实际码率，含固定序列 PSNR 测试 | GCC 10.3 AArch64 交叉编译，2026-10-26 样机三档码率及六轮固定序列实测 |
| 05-usb-hid/18-gadget-config/ | configfs 键鼠 Gadget 脚本（键盘 + 绝对/相对鼠标） | 2026-10-27 样机一次配成三个功能并枚举为 configured |
| 05-usb-hid/19-keyboard/ | 写 8 字节键盘报告，默认只打印 | 2026-10-27 样机发送 a 键，被控设备收到 KEY_A 按下与释放 |
| 05-usb-hid/20-mouse/ | 写绝对/相对鼠标报告，默认只打印 | 2026-10-27 样机发送坐标、位移、滚轮、点击，被控设备收到 ABS/REL/BTN 事件 |
| 05-usb-hid/21-usb-recovery/ | status / release / rebind USB 恢复脚本 | 2026-10-27 样机释放卡键、重绑 KVM，主机重新枚举 |
| 06-integration/23-video-socket/ | C 视频端：采集、编码、KVM2、控制命令，接 Go 后端 | 2026-10-27 样机浏览器播放 1080p，60.0 fps，stop/start 与生产恢复验证 |
| 06-integration/24-control-hid/ | C HID 服务：Gadget + HID IPC，接 Go 后端 | 2026-10-27 样机 Go 后端连接并 configure；浏览器按键和绝对鼠标到达被控设备 |

C 示例使用 `-std=c11 -Wall -Wextra -O2` 检查。采集实验只支持单内存平面的 NV12，输出保存有效区原始字节，可能含 stride/padding；不能不检查布局就按紧密图像播放。

键盘报告必须与描述符匹配，测试前在被控设备打开空白编辑器。视频端不能与正式 `kvm_video` 同时占用 `/dev/video0`，实验结束后用 `S99kvm start` 恢复。

描述符按 HID boot keyboard 规范编写，与 Linux HID Gadget 示例一致。各示例的实验条件、操作和限制见相应教程章节。
