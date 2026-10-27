# 第十六课：生成 H.264 码流

连续采集 120 帧，硬件编码后把 Annex-B 码流写入文件。文件开头是 SPS/PPS，之后是 IDR 和 P 帧，可以直接用 FFmpeg 播放。

## 文件

| 文件 | 用途 |
| --- | --- |
| `h264_file.cpp` | 采集、导入 VE、编码并写 Annex-B 文件 |
| `Makefile` | 编译本程序和 SDK 的 `AWVidoeEncoder.cpp` |

## 编译

```bash
make SDK_ROOT=~/T527_Tina5.0_SDK
```

Makefile 把 `buildroot/package/auto/sdk_lib/sdk_encoder/AWVidoeEncoder.cpp` 一起编，并定义 `-DKVM_H264_LEVEL=VENC_H264Level42 -DMODULE_VERSION=...`。不要改成链接预编译的 `libsdk_encoder.so`，那是默认级别，实测 SPS 是 Level 5.1。

## 传到板子

按教程第 06 章的方法用 HTTP 下载。板端 `/usr/lib` 需要有编码库，配套镜像自带。

## 运行

```sh
/etc/init.d/S99kvm stop
./h264_file /dev/video0 /tmp/capture.h264 120
/etc/init.d/S99kvm start
```

程序启动时自己等 HDMI 锁定；120 帧在 60 fps 下约 2 秒。

## 实测输出（2026-10-26）

```text
waiting for HDMI input...
input locked: 1920x1080@60
buffers=4 sizeimage=3133440 bytesperline=2880
buffer[0] size=3133440 phy=0xfe400000
buffer[1] size=3133440 phy=0xfe000000
buffer[2] size=3133440 phy=0xfdc00000
buffer[3] size=3133440 phy=0xfd800000
encoding 120 frames to /tmp/capture.h264
captured=120 encoded=120 idr=2 elapsed=2.025s fps=59.27
file=/tmp/capture.h264 config=24B frames=865389B total=865413B
```

## 检查码流

```bash
ffprobe -v error -f h264 -show_streams capture.h264
ffmpeg -f h264 -i capture.h264 -vf "select=eq(n\,90)" -frames:v 1 frame.png
```

本板文件：`codec_name=h264`、`profile=High`、`1920x1080`、`level=42`，第 90 帧解码正常。

## 几个关键点

- 配置包（`pts == -1`）是 SPS/PPS，必须写进文件，否则无法解码。
- 回调返回后编码器就复用输出内存，两段数据必须在回调内写完。
- **回调里的 `isKeyframe` 不可用**：SDK 封装没给这个字段赋值；本课按 Annex-B 的 NAL 类型 5 自己数 IDR，120 帧得到 2 个。
- 板端 BusyBox 的 `wget --post-file` 遇 NUL 截断，传二进制要先 base64。
- 865 KB 是静态桌面在 12 Mbps VBR 下的实际结果，画面变化大时会增加。

## 验收

- 文件能被 `ffprobe` 识别为 H.264 High、1920×1080、Level 4.2；
- 120 帧全部写入，`idr=2`；
- 抽帧解码画面与 HDMI 源一致；
- 退出后设备空闲，KVM 能重新拉起。

## 常见问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| 文件无法播放 | 缺 SPS/PPS 或写入半截包 | 配置包也要写，写失败要停 |
| Level 报 5.1 | 链接了预编译库 | 编 `AWVidoeEncoder.cpp` 并定义 Level42 |
| 所有帧都是关键帧 | 用了回调的 `isKeyframe` | 按 NAL 类型自己数 |
| 传回 0 字节 | `wget --post-file` 遇 NUL 截断 | 先 base64 再传 |
| 画面花屏 | UV 起点或长度错误 | UV 从 `width*height` 开始 |

## 验证记录

2026-10-26 在样机上完成：交叉编译、HTTP 传输、停服务编码 120 帧、恢复服务；文件 865,413 字节，`idr=2`，编码帧率 59.27；FFmpeg 解码为 High Level 4.2、1920×1080，第 90 帧画面正常。
