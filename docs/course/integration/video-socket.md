---
title: "通过视频 Socket 接入网页"
sidebar_label: "20 接入视频 Socket"
sidebar_position: 20
slug: /course/integration/video-socket
---

# 通过本地 Socket 传输视频

编码器输出 H.264 后，还需要把码流和采集时间交给网络服务。Unix stream Socket 只保证字节流顺序，**一次 `send()` 不一定对应一次 `recv()`**，所以接收端必须按协议边界组装完整消息。

## KVM2 消息包含什么

发送端位于 `kvm_video_demo/main.cpp` 和 `video_transport.h`；接收与时间处理位于 `kvm/internal/app/native.go`、`video_stream.go`。当前协议使用**固定 20 字节包头，数值字段都是小端序**。

| 偏移 | 字节数 | 字段 | 含义 |
| --- | --- | --- | --- |
| 0 | 4 | magic | ASCII `KVM2`，识别协议 |
| 4 | 4 | length | 后续 H.264 字节数，1—4,147,200 |
| 8 | 8 | timestamp | 单调时钟采集时间，单位微秒 |
| 16 | 4 | flags | bit 0 表示配置包，其余保留为 0 |
| 20 | length | payload | Annex-B H.264 数据 |

```text
K V M 2 │ 长度 4 B │ 采集时间 8 B │ 标志 4 B │ H.264 数据
                 ← 固定包头共 20 字节 →
```

图 1：KVM2 视频包结构。

长度描述的是 payload，不包含包头。不要把一帧 NV12 的大小当成压缩视频长度。

配置包携带 SPS/PPS，时间戳为 0；图像包时间戳非零，表示对应采集帧的时间。一次图像消息对应一个 access unit，即同一幅编码图像所需的 NAL 集合，不保证只有一个 NAL。

## 时间戳怎样进入 RTP

网络服务从采集时间生成 90 kHz RTP 时间戳。例如相隔约 16,667 微秒的两帧，其 RTP 时间差约为 1,500。这个关系保留采集节奏，不把本地 Socket 到达间隔误当成源帧间隔。

单调时钟用于测量间隔，不是日期时间。视频进程重新连接时，网络服务重新建立时间映射，缓存新的 SPS/PPS，并等待新的 IDR 恢复播放。不能继续发送已经失去参考状态的 P 帧。

## 完整读取和完整发送

接收端**先精确读取 20 字节**，验证 magic、长度、标志和时间，**再读取 length 个字节**。EOF 出现在包头或 payload 中间都代表截断，不能把残余字节当成下一帧。

发送端循环处理短写、EINTR 和 EAGAIN，但给整条消息设置 100 ms 截止时间。超时后关闭这条连接并重新同步；继续沿用半包连接会破坏接收端边界。**100 ms 是发送失败保护上限，不是正常视频延迟目标**。

## 使用教学接收器保存码流

下载 [receive_video.py](/examples/kvm/receive_video.py) 到开发主机的 `tutorial-examples/`。它仅接收 KVM2，保留 payload 到 H.264 文件，同时打印配置包和图像时间戳。输出文件使用独占创建，避免覆盖已有录像。

在装有 Python 3 的 Linux 开发主机执行：

```bash
python3 tutorial-examples/receive_video.py \
  --socket /tmp/kvm-tutorial-video.sock \
  --output capture.h264 --packets 120
```

看到 `listening:` 后只是等待发送者。发送程序必须位于同一台 Linux 主机，并连接该测试 Socket。板端精简镜像不一定有 Python；不要将网络 IP 填入 Unix Socket 路径。

这里的 `--packets` 统计消息，包含配置包，不能当作图像帧数。生产视频服务还依赖控制连接与 `start_video`，单独运行此接收器不会自动开始 HDMI 采集；独立编码实验的前提见[保存 H.264 视频](../encoding/h264-file.md)。不要删除或替换运行中服务的 Socket。

保存后在开发主机检查：

```bash
ffprobe -v error -f h264 -show_streams capture.h264
```

H.264 文件只保留码流，KVM2 采集时间并未写进文件。检查分辨率、profile 和解码错误；裸流推算的帧率不能代替 RTP 或浏览器测量。
