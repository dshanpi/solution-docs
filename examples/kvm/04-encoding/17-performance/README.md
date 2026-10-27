# 第十七课：测量编码性能

连续采集并编码 300 帧，统计 `encode()` 耗时、帧率、实际码率和 IDR 数量。不保存码流，画面内容由当前 HDMI 输入决定。

## 文件

| 文件 | 用途 |
| --- | --- |
| `perf.cpp` | 实时采集并测量编码性能 |
| `encode_bench.cpp`、`sequence.h` | 固定 600 帧序列的板端编码测试 |
| `reference.cpp` | 主机端生成参考帧，供 PSNR/SSIM 对比 |
| `base.nv12` | 单帧参考图 |
| `Makefile` | 编板端程序和主机端 `reference` |

## 编译

```bash
make SDK_ROOT=~/T527_Tina5.0_SDK
```

## 运行

```sh
/etc/init.d/S99kvm stop
./perf /dev/video0 8000 300
./perf /dev/video0 12000 300
./perf /dev/video0 16000 300
/etc/init.d/S99kvm start
```

参数依次是设备、配置码率（kbps）、帧数。每次启动会自己等 HDMI 锁定。

固定序列测试用同一目录的 `quality_bench` 和 `base.nv12`，停掉 KVM 后运行 600 帧：

```sh
./quality_bench base.nv12 12000 /tmp/r1-12000.h264
```

主机端用 `reference` 生成参考帧，再用 FFmpeg 比较：

```bash
./reference base.nv12 | ffmpeg -f rawvideo -pix_fmt nv12 -s 1920x1080 -r 60 -i - \
  -r 60 -i r1-12000.h264 \
  -lavfi "[0:v]settb=AVTB,setpts=N/(60*TB),format=yuv420p[ref];[1:v]settb=AVTB,setpts=N/(60*TB),format=yuv420p[dec];[dec][ref]psnr" \
  -f null -
```

把 `psnr` 换成 `ssim` 得到 SSIM。`reference` 是主机程序，不要拷到板子上。固定序列的 ION 缓冲区按 1920×1088×1.5 分配，按 1080 行会越界。

## 实测结果（2026-10-26）

同一路 1080p60 静态桌面输入：

| 配置码率 | 实际码率 | 帧率 | encode 平均 | encode 最大 | IDR |
| --- | --- | --- | --- | --- | --- |
| 8 Mbps | 3.63 Mbps | 59.81 | 8.52 ms | 8.63 ms | 5 |
| 12 Mbps | 4.45 Mbps | 59.87 | 8.52 ms | 8.67 ms | 5 |
| 16 Mbps | 4.79 Mbps | 59.73 | 8.54 ms | 8.69 ms | 5 |

原始输出：

```text
bitrate_cfg=8000 frames=300 elapsed=5.016s fps=59.81 enc_min=8.30ms enc_avg=8.52ms enc_max=8.63ms idr=5 bytes=2275393 actual_kbps=3629
bitrate_cfg=12000 frames=300 elapsed=5.011s fps=59.87 enc_min=8.30ms enc_avg=8.52ms enc_max=8.67ms idr=5 bytes=2784210 actual_kbps=4445
bitrate_cfg=16000 frames=300 elapsed=5.023s fps=59.73 enc_min=8.33ms enc_avg=8.54ms enc_max=8.69ms idr=5 bytes=3009596 actual_kbps=4794
```

## 怎么读这些数

- `enc_avg` 只统计 `encode()` 调用（含回调），不含采集等待和启动；60 fps 预算 16.7 ms，8.5 ms 还有余量。
- `fps` 是编码吞吐，受采集 60 fps 限制，不是编码器上限。
- `actual_kbps` 由帧字节数和统计窗口算出。静态桌面下 VBR 用不满配置值，换运动画面会上升。
- `idr=5` 对应第 0、60、120、180、240 帧，按 NAL 类型 5 统计。

## 验收

- 三档都完成 300 帧，帧率 59–60，IDR 为 5；
- `enc_avg` 低于 16.7 ms，`enc_max` 无明显尖峰；
- 实际码率随配置变化且低于上限；
- 保存原始输出行，便于和下次对比。

## 常见问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| 帧率低 | 编码耗时接近预算或输入掉锁 | 看 `enc_avg` 和 `dmesg` |
| 实际码率远低于配置 | 静态画面，VBR 用不满 | 固定测试序列再比 |
| 两次结果差异大 | 画面内容不同 | 固定输入和统计时长 |
| IDR 数量不对 | GOP 或 `isKeyframe` 设置不同 | 检查 `maxKeyFrame` 和输入标志 |

## 验证记录

2026-10-26 在样机上完成三档码率各 300 帧测量；帧率 59.7–59.9，编码调用 8.52–8.54 ms，实际码率 3.63–4.79 Mbps，IDR 均为 5；恢复服务后四个进程正常。
