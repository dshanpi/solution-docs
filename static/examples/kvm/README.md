# Web KVM 教学示例

这些是独立教学辅助文件，不是现有 `kvm_video_demo` 的替换版本。

| 文件 | 用途 | 已检查范围 |
| --- | --- | --- |
| v4l2_capture.c | 默认查询格式；显式采集单内存平面 NV12 | Linux 主机和 GCC 10.3 AArch64 编译，尚未实机取帧 |
| hid_keyboard.c | 默认打印报告；显式发送 a 键与释放 | 两种架构编译、主机默认输出；尚未实机发送 |
| keyboard-report-desc.bin | 63 字节键盘描述符 | 与 USB HID 服务 descriptor_0 逐字节核对 |
| receive_video.py | 接收 KVM2 视频包与采集时间戳 | 分段读取、配置包、时间戳、标志、非法长度和截断检查 |

C 示例使用 `-std=c11 -Wall -Wextra -Werror -O2` 检查。采集实验只支持单内存平面的 NV12，输出保存有效区原始字节，可能含 stride/padding；不能不检查布局就按紧密图像播放。

键盘报告必须与描述符匹配，测试前在被控设备打开空白编辑器。视频接收器默认独立测试路径，不能替换运行中服务的 Socket。板端未必有 Python。

描述符来源：配套 SDK `kvm-hid-service/c/descriptors.h` 的 `descriptor_0`，上游出处为 Linux HID Gadget 示例。各示例的实验条件、操作和限制见相应教程章节。
