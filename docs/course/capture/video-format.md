---
title: "查询设备能力和图像格式"
sidebar_label: "09 查询采集格式"
sidebar_position: 9
slug: /course/capture/video-format
---

# 查询设备能力和图像格式

应用打开 `/dev/video0` 后，还需要询问它支持什么接口、当前图像如何排列。宽高相同的两块内存，可能分别是 RGB、UYVY 或 NV12，直接按错误格式显示就会出现错色或错行。

## V4L2 的三个对象

**设备节点**是应用进入驱动的文件接口；**ioctl** 用来查询或配置设备；**缓冲区**承载采集图像。Linux 的 V4L2 为这些动作提供统一接口，VIN 则实现当前平台的具体操作。

| 对象 | 当前项目使用方式 |
| --- | --- |
| 设备 | `/dev/video0` |
| 采集类型 | `V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE` |
| 内存方式 | `V4L2_MEMORY_MMAP` |
| 像素格式 | `V4L2_PIX_FMT_NV12` |

**`MPLANE` 表示使用多平面 API**，不意味着每一种像素格式都返回多个独立内存平面。当前程序把 NV12 的 Y 与 UV 放在一个连续内存平面中处理；应检查驱动返回的 `num_planes`，而不是仅凭 API 名称决定平面数量。

## NV12 的内存排列

8 位 NV12 将亮度 Y 放在前部，后部交错保存 U、V 色度。每个 2×2 像素区域共享一组 U/V，因此紧密排列时需要宽×高×1.5 字节。

```text
单个内存平面
┌──────────────────────────────┐
│ Y0 Y1 Y2 Y3 ...               │ 高度 H
│ 每行跨度为 bytesperline       │
├──────────────────────────────┤
│ U0 V0 U1 V1 ...               │ 高度 H/2
│ 色度水平和垂直均减半          │
└──────────────────────────────┘
```

图 1：NV12 内存布局。

颜色平面与 V4L2 独立内存平面不是同一个概念。

1920×1080 紧密 NV12 为 **3,110,400 字节**。驱动可能提供行跨度和尾部对齐，所以不能把这个算式无条件当作 `mmap` 长度。**映射长度使用 `QUERYBUF` 返回值，图像布局使用 `G_FMT` 返回值**。

## 查询能力和实际格式

本指南提供 [v4l2_capture.c](/examples/kvm/v4l2_capture.c)。下载到 SDK 下自行创建的 `tutorial-examples/`，在 SDK 根目录执行：

```bash
"${CROSS_COMPILE}gcc" -std=c11 -Wall -Wextra -O2 \
  tutorial-examples/v4l2_capture.c -o tutorial-examples/v4l2_capture
```

`CROSS_COMPILE` 沿用开发环境章节。把程序复制到板端实验目录后执行：

```sh
./v4l2_capture /dev/video0
```

默认仅查询能力和当前格式，不执行 `S_FMT` 或启动采集。程序的核心是：

```c
struct v4l2_capability cap = {0};
ioctl(fd, VIDIOC_QUERYCAP, &cap);
struct v4l2_format fmt = {0};
fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
ioctl(fd, VIDIOC_G_FMT, &fmt);
```

这里省略错误处理以突出结构；下载文件包含检查。`capabilities` 含 `V4L2_CAP_DEVICE_CAPS` 时，应使用 `device_caps` 判断当前节点的能力。需要同时具备多平面采集和流式 I/O。

| 输出字段 | 后续用途 |
| --- | --- |
| `format`、宽高 | 选择解码/显示方法，核对实际协商结果 |
| `planes` | 确定每个 buffer 需要几个 `v4l2_plane` |
| `bytesperline` | 逐行处理时的跨度 |
| `sizeimage` | 驱动要求的图像缓冲区容量 |

**`S_FMT` 是请求，驱动可以调整参数**；下一章设置格式后重新读回并校验。只有真实返回值符合示例支持范围，才进入取帧步骤。
