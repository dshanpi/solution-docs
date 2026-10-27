# Web KVM 文档维护记录

## 2026-10-27：第 24 课 C HID 服务接入 Go 后端

- 第 24 课从“测试客户端发 HID IPC”改为“读者自己的 C HID 服务替换正式 kvm_hid”：新增 `06-integration/24-control-hid/kvm_hid_service.c`、`Makefile`、`run-hid-service.sh`、README；Python 课件从主线删除。
- 服务用 configfs 建键盘 + 绝对鼠标 + 相对鼠标（8/6/4 字节），通过 `dev` 属性定位 `/dev/hidgN`；提供 `/run/kvm/hid.sock` 的按行 JSON：单所有者、`configure` 握手、3 秒租约、`status`/`release`/键盘/绝对/相对/滚轮。配置相同复用 Gadget，避免每次连接重新枚举。
- Go 后端联调：`run-hid-service.sh` 建 `/tmp/kvm24root`，只替换 `kvm_hid`，平台、Go 后端和视频用生产版本。服务日志显示 Go 连接、`configured keyboard=1 absolute=1 relative=1`。
- 浏览器验收：按键 `a` 经 Go 后端到达被控设备，evtest 抓到 `KEY_A value 1/0`；绝对鼠标移动/点击抓到 `ABS_X/ABS_Y` 和 `BTN_LEFT`。本轮浏览器用绝对模式，相对/滚轮未单独切换验收。
- 真实踩坑：`configure` 返回后主机还没枚举完就发报告会 `keyboard write failed`；要等 `by-id` 出现再输入。每次客户端重配 Gadget 会让 evtest 的 fd 失效，所以配置不变时必须复用。
- `Ctrl-C` 后生产四个服务与 UDC `configured` 恢复，被控设备回到 `KVM USB Emulation Device`。
- 下一步：第 25–27 课部署、排障、验收按 C 视频端 + C HID 服务的最终组合重写。

## 2026-10-27：第 23 课 C 视频端接入 Go 后端

- 第 23 课从“测试脚本接收视频”改为“读者自己的 C 视频端接 Go 后端”：新增 `06-integration/23-video-socket/kvm_video_node.cpp`、`Makefile`、`run-tutorial.sh`、README；Python 接收器与对应单测从课件主线删除。
- 程序把第 12–17 课的 V4L2 采集与 `AWVideoEncoder` 编码组装起来，连接 Go 后端的控制与数据 Socket：控制通道处理 `start_video` / `stop_video` / `request_keyframe` 和带 `seq` 的请求，数据通道发送 20 字节 KVM2 包头；编码器只初始化一次，采集会话按启停重建。
- 真实踩坑：Go 在建立 WebRTC 会话前要求 SPS/PPS，如果等 `start_video` 才连数据 Socket，会报 `video encoder parameter sets are not ready` 并形成死锁。程序改为启动时先连数据 Socket、回放 24 字节配置包，再连控制 Socket；这一条已写入正文。
- 联调实测（2026-10-27，样机 1080p60）：浏览器显示 `1920 × 1080` 正常播放；板端日志 `capture started 1920x1080@60`，2 秒窗口 `sent=119`、`sent=239`，稳定 60.0 fps；Go 日志 `parameter sets ... profile-level-id=64002a`、`capture timestamp ... idr=true`，后续帧间隔 16,666/16,697 微秒、`idr=false`。
- 关闭网页触发 `stop_video`，板端 `capture stopped`；重开网页 Go 再次 `request_keyframe` + `start_video`，采集恢复。按 `Ctrl-C` 后四个生产服务与 UDC `configured` 恢复。
- 使用 `KVM_PLATFORM_PROFILE=/tmp/kvm-tutorial.env` 覆盖 `KVM_VIDEO_BINARY`，不改生产 `platform.env`；`run-kvm.sh` 的监督与 Go 后端保持原样。
- 下一步：第 24 课改为 C 的 HID IPC 服务端，接 Go 后端并做浏览器键鼠验收。

## 2026-10-27：第 22 课服务架构

- 第 22 课重写为"先分清四个服务，再沿接口定位"：`kvm_app`、`kvm_video`、`kvm_hid`、`kvm_platform` 各一张职责/接口/不该做什么的合并表，配服务关系图和"换硬件时改哪一层"对照表。
- 本课不新增课件程序，只用板端已有服务和命令：进程与日志（`pidof` 得 9980/9984/9977/9946，日志里 client/encoder 行）、四个 Socket 的 `ls -l` 属主与权限、`platform.env` 中四个路径环境变量，均为 2026-10-27 样机串口实测输出。
- 已执行 Docusaurus 生产构建与页面截图检查；不涉及 SDK 或成品源码修改。
- 下一步：第 23 课视频 Socket——用独立 Socket 接收真实视频服务的 KVM2 包，接收器与驱动脚本放在 `06-integration/23-video-socket/`（已完成，见上方记录）。

## 2026-10-27：第 19 课键盘报告与两端验收

- 第 19 课重写为"从零写这个程序"：先讲 8 字节报告布局（修饰键、保留、6 个键码）、按下/释放语义、写入长度必须等于 `report_length`，再分五步写程序（打开节点、等 POLLOUT、写按下、延迟 100 ms 写释放、失败也释放）。
- 课件补到 `05-usb-hid/19-keyboard/`（程序、Makefile、README）；公共目录的旧 `hid_keyboard.c` 和已无引用的 `keyboard-report-desc.bin` 删除，链接全部改到课目录。
- 被控设备 `192.168.1.66` 上验收：建键鼠 Gadget 后运行 `./hid_keyboard --send /dev/hidg0`，`evtest` 抓到 `type 1 (EV_KEY), code 30 (KEY_A), value 1` 和随后的 `value 0`；说明报告里的键码 `0x04` 在主机会映射成 `KEY_A`。
- teardown 加 `S99kvm start` 后 KVM 恢复，UDC `configured`，四个服务正常。
- 下一步：第 20 课鼠标（绝对/相对鼠标报告）——鼠标示例程序目前还不存在，需要在那一课补写并同样做两端验收。

## 2026-10-27：第 20 课鼠标报告与两端验收

- 新写课件 `05-usb-hid/20-mouse/`（程序、Makefile、README）：绝对鼠标（`hidg1`，Report ID 1 + 按键 + 16 位小端 X/Y，另 Report ID 2 滚轮）和相对鼠标（`hidg2`，按键 + 有符号 X/Y/滚轮），默认只打印，`--abs/--rel/--wheel/--click` 才写入。
- 正文重写为"从零写程序"：先比较两种鼠标的用途和节点，再讲两份报告布局，然后分步写（打开节点、组报告、等 POLLOUT 写整份、点击按下+延迟+释放、滚轮两种走法）。
- 被控设备 `192.168.1.66` 验收：`evtest` 在 if01 抓到 `ABS_X/ABS_Y=16384`、`REL_WHEEL=1`（伴随 `REL_WHEEL_HI_RES=120`）、`BTN_LEFT` 按下与释放；if02 抓到 `REL_X=100`、`REL_Y=50`。绝对鼠标事件值等于发送值。
- teardown 加 `S99kvm start` 后 KVM 恢复，UDC `configured`，四个服务正常。
- 下一步：第 21 课 USB 异常处理；之后 23 视频 Socket、24 控制采集仍缺课目录与示例。

## 2026-10-27：第 21 课 USB 异常恢复与两端验收

- 新增课件 `05-usb-hid/21-usb-recovery/`（`usb-recover.sh` + README），三个子命令：`status` 列控制器/Gadget/进程/HID 节点，`release` 按各功能 `report_length` 写全零报告，`rebind` 停 KVM、解绑全部 Gadget、重启并等 `configured`。
- 正文重写为"先判断再写脚本"：判断控制器占用、停止进程不等于解绑、三层状态表、逐段写 status/release/rebind，再给完整运行步骤。
- 被控设备验收：只发按下报告制造卡键，`evtest` 看到 `KEY_A value 1` 后主机自动重复（`value 2`）；执行 `release` 后收到 `value 0`，重复停止。`rebind` 后 UDC `configured`、四个服务在，主机 `dmesg` 重新枚举出 `KVM USB Emulation Device` 键盘和两个鼠标。
- 下一步：第 22 课服务架构；23 视频 Socket、24 控制采集仍缺课目录和示例程序。

## 2026-10-27：USB Gadget 一次配成键鼠

- 第 18 课改为一次配出键盘、绝对鼠标、相对鼠标三个 HID 功能；三份描述符按 base64 内嵌在脚本里，不再单独提供描述符文件，课件目录只有脚本和 README。
- 关键限制：内核 `HIDG_MINORS=4`，HID 功能全局最多 4 个；项目 `kvm` Gadget 已占 3 个，所以教学 Gadget 必须先停 KVM、解绑 UDC 并释放这三个功能，实验结束用 `S99kvm start` 重建。
- 实测：`/dev/hidg0` 8 字节、`/dev/hidg1` 6 字节、`/dev/hidg2` 4 字节，UDC `configured`；teardown 后 `kvm` 三个功能重建，状态恢复 `configured`，四个服务正常。
- 过程中的一次板子挂死出现在串口逐行执行、shell 变量跨行丢失、属性被写到根目录的那轮；改成整段脚本文件执行后，手动流程和课件脚本各跑一轮都正常。板子重启过一次。

## 2026-10-26：第 12–17 章改为课件式独立实验

- 课件目录改为 `static/examples/kvm/<阶段>/<章号>-内容`，每课含源码、Makefile、README；第 12–17 课分别是查询格式、采集第一帧、连续采集、编码缓冲区、生成码流、编码性能。
- 每章重写为"从读者手上的东西写起"，标题用常规技术名词；每一课都在样机上实测，输出写回正文和 README。
- 实测数据：13 章保存 3,133,440 字节 NV12，前 3,110,400 字节能正常解码；14 章 300 帧 59.87 fps、`gaps=0`；15 章 VE 导入后编码一帧，SPS/PPS 24 B、IDR 14,026 B；16 章 120 帧写成 Annex-B，865,413 B、`idr=2`、High Level 4.2；17 章三档码率各 300 帧，编码 8.5 ms、59.8 fps、实际 3.63/4.45/4.79 Mbps。
- 17 章的 PSNR/SSIM 固定序列测试已复现：原始工具在 `kvm-backups/video-quality-20260917`，关键是 ION 缓冲区按对齐高度 1920×1088 分配（3,133,440 字节）；之前按紧密 1080 分配触发了 VE 页表错误。复现的六轮码流与 2026-09-17 记录逐字节相同，PSNR/SSIM 一致。固定序列代码放在 `17-performance/` 目录（不单独建 README），第 17 章用三张 SVG 图呈现实际码率、整体 PSNR 和分段 PSNR，并补充了测试原理和复现命令。
- 其它实测修正：回调 `isKeyframe` 未初始化不可用，IDR 改为解析 NAL 类型；链接预编译 `libsdk_encoder.so` 得到 Level 5.1，改为编译封装并定义 Level 4.2；BusyBox `wget --post-file` 遇 NUL 截断，二进制先 base64。

## 2026-09-17：接线与 HDMI 检测同步

- 接线章统一为 J3 1→H1 1（3.3V）、2→3（PE15）、3→15（PE4）、4→16（PE3）、5→6（GND）；原理图摘图核对一致。
- 删除旧 PE14 接法和未统一说明，补充无需刷新即可恢复画面的拔插检查。
- 同步设备树、驱动和热插拔专题，去掉已修复缺陷的旧描述；播放期间禁止完整时序测量。
- 依据 SDK 的 board.dts、lt6911c_mipi.c、S91camera、kvm_video_demo/HOTPLUG_VERIFICATION.md，以及本次板端日志和用户目视确认。
- 导航检查通过（38 项），生产构建通过，接线页已在浏览器检查图片、引脚表和重点提示。
- 本轮仅更新文档，未再次改动板端程序。完整镜像重打包、断电冷启动和长期测试不属于本次通过结果。

以下为较早记录，其中“热插拔待实现”等状态由上述更新取代。

## 2026-09-17：同步当前运行实现

本轮核对 SDK 中的四个服务及部署脚本，更新系统框架、职责边界、HID、视频协议、编码内存、性能、启动部署和进阶章节。学习正文统一称为 USB HID 服务，不讨论历史语言版本。网络服务和前端只介绍系统职责、接口与配套使用。

### 本轮完成

- 架构图补入独立 HID 服务、平台服务，以及四类进程通信；保留路径高亮、章节跳转和动画控制。
- 视频示例改为 KVM2：20 字节头、采集时间戳、参数包标志、小端字段与分段读取。
- 编码内存按 DMA-BUF 导入及 ION 拷贝回退讲解；同步 1080p60、12 Mbps、VBR、QP 20–28、GOP 60、High Level 4.2。
- 根据实际报告写入 8/12/16 Mbps 对比和解码帧，区分目标码率、实际码率、编码调用时间、浏览器显示 FPS 与端到端延迟。
- 部署按 S99kvm、run-kvm.sh、platform.env、board.json 和四个服务组织，说明前端内嵌、停止顺序和镜像打包边界。
- 功能范围统一为视频、键鼠、终端和基础设置；扩展章节不再把未提供功能列为当前能力。
- 介绍页和参考镜像页替换为当前实机截图；截图过程中未发送远端键鼠输入。
- 动态分辨率专题补齐 DMA-BUF 导入资源的释放和重新创建关系，仍明确它是待实现专题。

### 学习任务与证据

| 内容 | 读者应能完成 | 本轮证据 |
| --- | --- | --- |
| 系统框架 | 按画面和输入两条路径定位负责服务 | 对照当前源码、Socket 和部署配置；桌面页面目视检查 |
| 视频接收 | 解析 KVM2 消息并识别截断、非法字段 | 6 项 Python 测试，包括真实 socketpair 分段发送 |
| HID 报告 | 对照描述符解释报告布局 | 下载用键盘描述符与现用 descriptor_0 一致，共 63 字节 |
| 编码与性能 | 区分内存路径、码率、FPS 和延迟口径 | 引用既有实机编码与 WebRTC 测试记录；本轮未重新运行硬件压测 |
| 部署 | 理解配置、监督脚本及升级前的停机要求 | 读取当前 SDK 脚本与安装目录；本轮未替换板上程序 |

### 检查与范围

- 课程导航检查通过：38 个文档及分类文件。
- KVM2 接收器 6 项测试、学习进度 3 项测试通过。
- Docusaurus 最终生产构建通过，已包含本轮截图和文字修订；日志保存于本机 work/docs-refresh/build-final.log。
- 文档中的实际硬件结果来自已有测试记录，不把正式程序的通过结果套用到教学示例。
- 独立采集示例、独立 Gadget 实验、完整独立编码落盘程序，仍需各自补足板端运行证据。
- 本轮没有完成动态分辨率、驱动热插拔改造，也没有重新生成和验收整机烧录镜像。

以下保留此前记录，仅用于追溯；其中对当时源码状态的描述不代表当前实现。

---

# Web KVM 正文初稿核对记录

日期：2026-09-16。此文件供维护者使用，不进入学习侧栏。

## 本次内容

- 将 24 章基础与 4 个进阶专题的占位大纲展开为正文。
- 写入真实源码入口、概念、命令、协议与验收条件；不教授 Go 和前端实现。
- 加入模块/主板原理图摘图、已有 UpgradeFlash 错误截图、当前 KVM 桌面截图。
- 使用流程图、状态图、时序图、布局与报告表解释关键关系。
- 增加独立采集、键盘报告、视频收包示例及项目键盘描述符。

## 已执行检查

- SSH 读取 SDK，确认 kvm HEAD d778c46、实际工具链路径和构建入口。
- 读取板端 Linux 5.15.147、video0、hidg0/1/2、4100000.udc-controller、应用进程、编码日志和 S91camera。
- 获取运行中 Web KVM 的真实桌面截图，没有发送键鼠输入。
- 两个 C 文件通过 Linux 主机 GCC 与 SDK GCC 10.3 AArch64 的 Wall/Wextra/Werror 编译；执行键盘示例的默认打印模式。
- 视频接收器检查分段读取、零长度、超限、过早 EOF。
- Docusaurus 生产构建与课程导航检查通过。

## 不能宣称已通过的内容

- 新增采集工具的板端取帧与异常清理回归。
- 新建 Gadget 的枚举、键盘报告实际输入和完整鼠标实验。
- 独立硬件编码落盘程序：正文目前为源码讲解与改造步骤，不是完成的独立程序。
- 驱动检测初始化、ID 错误传播和热插拔补丁。
- 固定供电与 GPIO 接法的统一冻结，PE 电源域的实测核对。
- 版本化镜像、恢复流程、部署脚本与冷启动自启动验收。
- 动态分辨率、零拷贝、低延迟和可选功能。

## 逐章技术复核重点

- 准备：J3 无主电源；区分固件、驱动、应用与两种 GPIO 接法；不猜排线方向。
- 驱动：0x56 为 VIN 约定，注册后 7 位地址为 0x2b；缺 GPIO 提前返回且 probe 忽略；查询/卸载风险明确。
- 采集：MPLANE API 不等于多个内存平面；QUERYBUF 长度、G_FMT 布局、bytesused/data_offset 分开。
- 编码：当前仍有 memcpy；对齐容量不代表紧密布局；输出回调生命周期与两段数据；日志与 maxKeyFrame 差异。
- HID：报告长度、Report ID、按下与释放、UDC 占用、停止 adbd 与解绑的区别。
- 集成：Go 监听，C++ 连接；小端长度包没有时间戳；短写与控制 JSON 边界问题不掩盖。
- 部署和进阶：建议结构与当前已发布产物明确区分，验收矩阵不假定已经执行。

下一轮应在空闲样机上逐项补运行证据，再将对应章节的实验验证说明更新为实际结果。原理图完整文件目前仅进入本地站点，外部发布仍沿用仓库现有发布流程。
