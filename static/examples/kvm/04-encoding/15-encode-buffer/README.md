# 第十五课：把采集缓冲区交给硬件编码器

采集一帧 NV12，导出 DMA-BUF，经 `/dev/cedar_dev` 导入编码器地址空间，直接编码一帧。回调里打印 SPS/PPS 和码流包大小，不保存文件。

## 文件

| 文件 | 用途 |
| --- | --- |
| `encode_buffer.cpp` | 采集一帧、导入 VE、硬件编码一次 |
| `Makefile` | 用 SDK 里的 g++ 和编码库编译 |

## 编译

```bash
make SDK_ROOT=~/T527_Tina5.0_SDK
```

链接 `libsdk_encoder.so`、`libvencoder.so` 等 SDK 库，头文件来自 `buildroot/package/auto/sdk_lib`。

## 传到板子

按教程第 06 章的方法用 HTTP 下载。板端 `/usr/lib` 需要已有的编码库，配套镜像自带。

## 运行

```sh
/etc/init.d/S99kvm stop
./encode_buffer /dev/video0
/etc/init.d/S99kvm start
```

## 实测输出（2026-10-26）

板端 Linux 5.15.147，HDMI 1920×1080@60：

```text
waiting for HDMI input...
input locked: 1920x1080@60
buffers=4 sizeimage=3133440 bytesperline=2880
captured: index=0 sequence=2 bytesused=3133440
VE import: fd=4 phy=0xfe400000
config: data0=24 data1=0
encoder: H.264 1920x1080 60fps 12000kbps VBR QP 20-28
frame=1 data0=14026 data1=0
encode: ret=0 elapsed=8.5ms config=24B frames=1 bytes=14026
```

`data0=24` 是 SPS/PPS，`data0=14026` 是这一帧的 IDR。`elapsed=8.5ms` 是同步编码调用时间，不是端到端延迟。

## 几个关键点

- **VIN 的物理地址不能直接给编码器。** 两个设备看到的地址空间不同，必须 `VIDIOC_EXPBUF` 后用 `/dev/cedar_dev` 的 `CEDAR_MAP_DMA_BUF`（0x504）导入，拿返回的 `phy_addr`。
- **编码期间不能 QBUF。** `encode()` 同步返回后才把 buffer 还给驱动，否则采集硬件会重写编码器还没读完的画面。
- **UV 从 `width*height` 开始**，长度 `width*height/2`；不要用 `bytesperline` 算。
- 回调里的 `pAddrVir0` 只在本次调用内有效，要保存必须自己拷贝；`pts == -1` 的是配置包。
- **回调里的 `isKeyframe` 不能用**：SDK 封装的输出 `AVPacket` 没有初始化，这个字段从未赋值。关键帧只能自己解析 Annex-B 的 NAL 类型。

## 验收

- EXPBUF 和 MAP_DMA_BUF 成功，打印出非零物理地址；
- 收到 SPS/PPS（约 24 字节）；
- 编码一帧返回 0，回调收到一帧码流，字节数大于 0；
- 退出后设备可再次打开，KVM 能重新拉起。

## 常见问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| EXPBUF 失败 | 缓冲区类型或索引不对 | 用 MMAP 申请的缓冲区，索引来自本次 DQBUF |
| MAP_DMA_BUF 失败 | `/dev/cedar_dev` 未打开或 fd 无效 | 检查节点和导出 fd |
| 编码器 init 失败 | 库缺失或参数越界 | 检查 `/usr/lib` 编码库、QP 和码率 |
| 只有配置包没有图像帧 | `encode()` 失败或没调用 | 看 `encode ret` 和回调 |
| 画面花屏 | UV 起点或长度不对 | UV 从 `width*height` 开始 |

## 验证记录

2026-10-26 在样机上完成：交叉编译、HTTP 传输、停服务采集一帧、VE 导入、硬件编码一帧；SPS/PPS 24 字节，IDR 14,026 字节，编码调用 8.5 ms；恢复服务后四个进程正常。
