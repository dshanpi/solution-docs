# 图注原始资料记录

仅供维护者核对来源，页面使用简短图注。以下保存本次精简前的资料定位与说明。

- `advanced/dynamic-resolution.md`：图 1：一种可实现的分辨率切换流程。每一代 buffer 必须只对应同一代格式，不能边写新尺寸边沿用旧分配。
- `advanced/hdmi-hotplug.md`：图 1：建议的输入状态处理。中断只触发检查，不能把一个边沿直接解释成“已得到图像”。
- `advanced/low-latency.md`：图 1：看见远端光标变化必须等待输入路径和视频返回路径。只缩短 HID 报告处理时间，不一定能消除画面上的全部落后。
- `capture/capture-loop.md`：图 1：缓冲区状态。应用不能保留映射指针给异步消费者后，立即 QBUF 让硬件覆盖它。
- `capture/first-frame.md`：图 1：第一帧的取得过程。这里的箭头是控制调用，图像由硬件写入缓冲区。
- `capture/video-format.md`：图 1：NV12 的颜色平面。颜色平面与 V4L2 独立内存平面不是同一个概念。格式定义参见 [Linux NV12 文档](https://docs.kernel.org/userspace-api/media/v4l/pixfmt-yuv-planar.html)。
- `deployment/startup.md`：图 1：监督脚本管理进程；网络服务管理视频会话。驱动节点与连接实际就绪仍需要检查，不能只看脚本编号。
- `deployment/troubleshooting.md`：图 1：视频故障的检查顺序。独立采集前须停止原采集者，不能让两个实验争抢设备。
- `drivers/hdmi-signal.md`：图 1：模块 HDMI 输入页，摘自配套模块原理图第 1 页。[查看原理图](/resources/kvm/module-schematic.pdf)。
- `drivers/hdmi-signal.md`：图 2：有效图像的检查顺序。前一层通过，只能使下一层检查有意义，不能跳过中间层。
- `drivers/lt6911c-driver.md`：图 1：媒体总线格式与应用缓冲区格式。`MEDIA_BUS_FMT_*` 与 `V4L2_PIX_FMT_*` 描述的位置不同。
- `drivers/sdk-build.md`：图 1：源码、构建产物与板端消费者。驱动模块还需要匹配内核 ABI，用户态程序需要匹配目标动态库。
- `encoding/encoder-memory.md`：图 1：两条路径最终共享同一编码接口。这里省去的是采集到编码之间的图像复制，不代表整个网络链路都没有复制。
- `encoding/h264-file.md`：图 1：编码输出的有效期。回调包中的两段数据共同组成一次编码输出。
- `encoding/video-performance.md`：图 1：8 Mbps 配置的第 420 帧解码结果。
- `encoding/video-performance.md`：图 2：12 Mbps 配置下的同一输入帧。[编码前参考图](./images/video-performance/reference-frame420.png)与[16 Mbps 解码图](./images/video-performance/frame420-16000.png)可用于进一步对照。数据采集于 2026-09-17，比较从 NV12 输入开始，不包含 HDMI 采集和色彩转换的损失。
- `integration/capture-control.md`：图 1：刷新请求走控制方向，新的图像走视频方向。周期 GOP 仍为 60 帧，请求关键帧并不要求修改 GOP。
- `integration/capture-control.md`：图 2：视频 Socket 不传递按键。网络服务负责会话和事件，HID 服务负责设备报告。
- `integration/service-architecture.md`：图 1：网络服务连接浏览器与本地服务。视频流与键鼠事件走不同接口；板级设备访问留在对应服务中。
- `integration/video-socket.md`：图 1：长度描述的是 payload，不包含包头。不要把一帧 NV12 的大小当成压缩视频长度。
- `preparation/hardware.md`：图 1：Web KVM 的外部连接。网络承载双向通信，HDMI 和 USB 是接向被控设备的两条不同线缆。
- `preparation/hardware.md`：图 2：模块接口，摘自配套 `HDMI_LT6911UXC_V11原理图.pdf` 第 2 页，图纸日期 2026-09-06。[查看配套原理图](/resources/kvm/module-schematic.pdf)。
- `preparation/reference-image.md`：图 1：Web KVM 实际运行界面。中间的桌面属于 HDMI 源设备；外层工具栏属于 KVM。当前功能为视频、键鼠、终端与基础设置。
- `preparation/wiring-firmware.md`：图 1：Avaota 主板 H1，摘自 `SCH_Avaota Pi A_2_2024-08-30.pdf` 第 9 页，REV 1.0。[查看主板原理图](/resources/kvm/mainboard-schematic.pdf)。
- `usb-hid/gadget-config.md`：图 1：USB 键鼠分工。USB HID 服务管理配置和报告，内核完成 USB 枚举与传输。
- `usb-hid/keyboard-reports.md`：图 1：一次按键的两份状态报告。**`0x04` 是 HID Usage ID，不是 ASCII 字符 `a` 的编码**。
- `usb-hid/mouse-reports.md`：图 1：绝对位置报告。X、Y 都按低字节在前发送，不能直接复制与主机字节序绑定的 C 结构体。
