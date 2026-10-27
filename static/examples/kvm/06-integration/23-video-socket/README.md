# 23 C 视频端接入 Go 后端

把第 12–17 课的 V4L2 采集与 VE 编码组装成一个 C++ 视频服务，连接现有 Go 后端的控制 Socket 和数据 Socket。

| 文件 | 用途 |
| --- | --- |
| kvm_video_node.cpp | 采集、编码、KVM2 发送、控制命令、状态事件 |
| Makefile | 交叉编译与 SDK 编码封装 |
| run-tutorial.sh | 用教程 profile 启动现有监督脚本，退出恢复生产服务 |

## 编译

```bash
make SDK_ROOT=~/T527_Tina5.0_SDK
file kvm_video_node
```

产物为 AArch64 ELF。板端没有编译器，需在开发主机交叉编译后传输。

## 运行

开发主机在课件目录起 HTTP 服务，板端下载 `kvm_video_node` 和 `run-tutorial.sh`，核对 MD5 后：

```sh
sh run-tutorial.sh
```

脚本复制 `platform.env` 到 `/tmp/kvm-tutorial.env`，只覆盖 `KVM_VIDEO_BINARY`，用现有 `run-kvm.sh` 启动平台、HID、Go 后端和本课视频端；按 `Ctrl-C` 恢复 `S99kvm`。

浏览器打开板端网页（本样机 `http://192.168.1.52/`），页面显示 `1920 × 1080` 即说明 Go 已收到本课程序送出的码流。

## 实测（2026-10-27）

- 启动后先连数据 Socket 并发送 24 字节 SPS/PPS，再连控制 Socket。
- 浏览器建立会话后 Go 发 `request_keyframe` 和 `start_video`；板端采集 `1920x1080@60`，2 秒窗口 `sent=119`、`sent=239`，之后稳定 60.0 fps。
- Go 日志：`native video stream connected`、`parameter sets fmtp=...profile-level-id=64002a`、`capture timestamp ... idr=true`，随后两帧间隔 16,666/16,697 微秒、`idr=false`。
- 关闭页面触发 `stop_video`，板端 `capture stopped`；重开页面再次请求关键帧并恢复采集。
- `Ctrl-C` 后四个生产服务与 UDC `configured` 恢复。

## 限制

- 固定 1920×1080@60、NV12、单数据客户端；不含 EDID、HDMI 热插拔和动态分辨率。
- 程序用 C++ 是因为 SDK 编码器是 C++ 类；V4L2 与 Socket 部分仍是 C 接口。
- 必须用教程 profile 覆盖 `KVM_VIDEO_BINARY`，不要直接改 `platform.env`。
- 视频端和正式 `kvm_video` 不能同时采集同一个 `/dev/video0`。
