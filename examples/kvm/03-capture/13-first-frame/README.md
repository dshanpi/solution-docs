# 第十三课：采集第一帧

上一课只读到了格式参数，这一课真正向驱动要一帧数据：申请缓冲区、映射、交给驱动、启动采集、取回一帧，保存成 NV12 文件，再把缓冲区和设备释放干净。

## 文件

| 文件 | 用途 |
| --- | --- |
| `first_frame.c` | 采集一帧并保存为原始 NV12 |
| `Makefile` | 使用 SDK 里的交叉编译器编译 |

## 编译

```bash
make SDK_ROOT=~/T527_Tina5.0_SDK
```

## 传到板子

按教程第 06 章的方法，在开发主机用 `python3 -m http.server` 提供文件，板端 `wget` 下载并校验 MD5。

## 运行

`/dev/video0` 同一时间只能有一个采集者。先停 KVM，再运行；程序会自己等输入重新锁定：

```sh
/etc/init.d/S99kvm stop
./first_frame /dev/video0 /tmp/first-frame.nv12
/etc/init.d/S99kvm start
```

程序用 `VIDIOC_QUERY_DV_TIMINGS` 连续确认三次 1080p60，最多等 10 秒。停掉视频服务后输入会短暂掉锁，早期版本没等就启动，日志里报过 `s_stream on=1 result=-67`，随后等不到帧。

## 实测输出（2026-10-26）

板端 Linux 5.15.147，HDMI 1920×1080@60：

```text
waiting for HDMI input...
input locked: 1920x1080@60
buffers=4 sizeimage=3133440 bytesperline=2880
sequence=2 bytesused=3133440 offset=0 saved=3133440 elapsed=82.8ms
```

保存的文件 3,133,440 字节。其中前 3,110,400 字节是紧密 NV12，最后 23,040 字节是填充，实测与全零块 MD5 相同。

## 检查图像

把文件传回主机，用 FFmpeg 按紧密 NV12 解码：

```bash
ffmpeg -f rawvideo -pixel_format nv12 -video_size 1920x1080 \
  -i first-frame.nv12 -frames:v 1 first-frame.png
```

本板取到的帧：

![采集到的 1080p NV12 帧](./images/first-frame/frame.png)

图 1：`first_frame` 保存的帧经 FFmpeg 解码。画面正常、颜色正确，说明缓冲区开头就是紧密 NV12 布局。

## 验收

- 程序退出码为 0，打印 `sequence`、`bytesused`、`saved`；
- 文件大小不小于 3,110,400 字节，前 3,110,400 字节能按 1920×1080 NV12 正常解码；
- 尾部填充不影响播放；若有额外非零数据，说明布局理解有误；
- `/etc/init.d/S99kvm start` 后四个服务都能重新拉起。

## 常见问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| `Device or resource busy` | 还有进程占用采集设备 | 停 KVM，确认 `pidof kvm_video` 为空 |
| `HDMI 输入 10 秒内没有锁定` | 源设备没输出或线松 | 检查 HDMI 源、模块供电和 `dmesg` |
| `等待 2000 ms 没有收到帧` | 输入在启动后又掉锁 | 看 `dmesg` 是否反复 `unavailable` |
| 文件大小是 3,110,400 而不是 3,133,440 | `bytesused` 不含尾部填充 | 两种都能解码，记录实际值 |
| 图像错色或错行 | 像素格式或布局判断错误 | 回看第 12 章的 `G_FMT` 输出 |

## 验证记录

2026-10-26 在样机上完成：交叉编译、HTTP 传输、停服务取帧、恢复服务；保存 3,133,440 字节，前 3,110,400 字节解码为正常 1080p 画面，尾部 23,040 字节为全零填充。
