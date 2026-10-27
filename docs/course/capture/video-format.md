---
title: "查询设备能力和图像格式"
sidebar_label: "12 查询采集格式"
sidebar_position: 9
slug: /course/capture/video-format
---

# 查询设备能力和图像格式

应用打开 `/dev/video0` 后，还需要询问它支持什么接口、当前图像如何排列。宽高相同的两块内存，可能分别是 RGB、UYVY 或 NV12，直接按错误格式显示就会出现错色或错行。这一课写一个只读的小程序，把驱动的能力和当前格式问清楚，后面的取帧、保存和编码都以它返回的参数为准。

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

格式查错，后面的取帧、保存和编码都会跟着错。所以在申请缓冲区之前，先用一个只读程序把设备的参数问清楚：它不改配置，也不启动采集。

## 打开设备

```c
fd = open(dev, O_RDWR | O_NONBLOCK);
if (fd < 0) {
	perror(dev);
	return 1;
}
```

`open` 只是拿到操作设备的文件描述符，不会改变驱动状态。用 `O_RDWR` 是因为后面的课程要申请缓冲区和启动采集；用 `O_NONBLOCK` 是因为之后 `DQBUF` 没有数据时要返回 `EAGAIN`，而不是把线程一直挂住。这一课用不到阻塞语义，但从一开始就统一接口，后面不用改。

这一步失败最常见的原因是设备正被 KVM 视频服务占用，返回 `Device or resource busy`。

## 查询设备能力

拿到描述符之后，先问清楚这个节点能做什么：

```c
if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
	perror("VIDIOC_QUERYCAP");
	goto out;
}
```

`ioctl` 可能被信号打断，返回 `EINTR` 时应当重试，而不是把它当成设备错误。所以示例用一个小的 `xioctl` 包一层：

```c
static int xioctl(int fd, unsigned long req, void *arg)
{
	int ret;
	do {
		ret = ioctl(fd, req, arg);
	} while (ret < 0 && errno == EINTR);
	return ret;
}
```

`VIDIOC_QUERYCAP` 返回驱动名、板卡名和能力位。判断能力时要先看 `V4L2_CAP_DEVICE_CAPS`：有这一位时，当前节点的能力在 `device_caps` 里，`capabilities` 描述的是整块芯片。

```c
if (cap.capabilities & V4L2_CAP_DEVICE_CAPS) {
	if (!(cap.device_caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE) ||
	    !(cap.device_caps & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "该节点不支持多平面采集或流式 I/O\n");
		goto out;
	}
}
```

先检查这两项，是因为后面的 `REQBUFS`、`STREAMON` 都依赖它们。能力不满足时早点停下，比在取帧循环里收到一个看不懂的 `EINVAL` 好定位。

## 读取当前格式

能力确认之后，读取设备当前的图像格式：

```c
fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
if (xioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
	perror("VIDIOC_G_FMT");
	goto out;
}
```

`VIDIOC_G_FMT` 读当前格式，不修改驱动。与之对应的 `S_FMT` 是“请求一个格式”，驱动可以调整后返回实际值，下一课会用到。

取回的是 `v4l2_pix_format_mplane`，因为 `type` 指定了 `MPLANE`：

```c
printf("pixelformat = ");
print_fourcc(fmt.fmt.pix_mp.pixelformat);
printf("\nwidth       = %u\n", fmt.fmt.pix_mp.width);
printf("height      = %u\n", fmt.fmt.pix_mp.height);
printf("num_planes  = %u\n", fmt.fmt.pix_mp.num_planes);

for (uint32_t i = 0; i < fmt.fmt.pix_mp.num_planes; i++) {
	printf("plane[%u]    = bytesperline %u, sizeimage %u\n", i,
	       fmt.fmt.pix_mp.plane_fmt[i].bytesperline,
	       fmt.fmt.pix_mp.plane_fmt[i].sizeimage);
}
```

遍历 `num_planes` 而不是只打印第一个平面，是因为别的格式可能返回多个平面。代码不假设 NV12 一定只有一块内存，只有驱动确认 `num_planes == 1` 时才按单平面处理。

三个返回值下一课都会用到：`num_planes` 决定申请几个平面描述，`sizeimage` 决定缓冲区要多大。`bytesperline` 要特别注意：**这个 VIN 驱动对 NV12 上报的是 1920×1.5=2880，不是 Y 平面的行跨度**。实际图像是紧密排列，UV 从 1920×1080 的位置开始；这个值不能拿去逐行寻址。换到别的平台，`bytesperline` 才通常表示行跨度，要按驱动文档确认。

程序还把紧密 NV12 的容量算出来做对照：

```c
if (fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV12 &&
    fmt.fmt.pix_mp.num_planes == 1) {
	printf("NV12 tight  = %u bytes\n",
	       fmt.fmt.pix_mp.width * fmt.fmt.pix_mp.height * 3 / 2);
}
```

这个数**不能**拿去 `mmap`，也不能当作 `sizeimage`。它和驱动返回值不一样时，说明驱动做了对齐。

## 编译与运行

把上面三步写成程序就是 [12-query-format](/examples/kvm/03-capture/12-query-format/query_format.c)，课件里的版本可以对照。

### 编译

用 `make` 编译，把 `SDK_ROOT` 指向 SDK：

```bash
make SDK_ROOT=~/T527_Tina5.0_SDK
```

编完确认产物架构，避免拿错工具链：

```bash
file query_format
"${CROSS_COMPILE}readelf" -h query_format | grep Machine
```

应显示 `ELF 64-bit` 和 `ARM aarch64`。**板端没有编译器**，程序在开发主机交叉编译后再传上去。

### 传到板子

板端没有 `scp`。在开发主机进入课件目录起临时 HTTP 服务：

```bash
cd <课件目录>
python3 -m http.server 8000
```

板端下载并和主机 `md5sum` 对照：

```sh
wget -q -O /tmp/query_format http://192.168.1.69:8000/query_format
chmod +x /tmp/query_format
md5sum /tmp/query_format
```

### 板端运行

当前 `/dev/video0` 由 KVM 视频服务独占，直接查询会返回 `Device or resource busy`。先停服务，查完再拉起：

```sh
/etc/init.d/S99kvm stop
/tmp/query_format /dev/video0
/etc/init.d/S99kvm start
```

## 板端输出

本板输出（Linux 5.15.147，HDMI 1920×1080@60）：

```text
driver      = sunxi-vin
card        = sunxi-vin
bus_info    =
version     = 1.1.0
capabilities= 0x85201000
device_caps = 0x05201000
pixelformat = NV12
width       = 1920
height      = 1080
field       = 1
num_planes  = 1
plane[0]    = bytesperline 2880, sizeimage 3133440
NV12 tight  = 3110400 bytes (只用来说明布局，映射按 QUERYBUF 返回长度)
```

| 字段 | 实测值 | 含义 |
| --- | --- | --- |
| `device_caps` | `0x05201000` | 含多平面采集和流式 I/O，可以进入下一课 |
| `pixelformat` | `NV12` | Y 平面加 UV 交错，保存和编码都按这个格式 |
| `num_planes` | `1` | 虽然是 MPLANE API，NV12 在这里只有一块内存 |
| `bytesperline` | `2880` | 本驱动对 NV12 上报 1920×1.5，不是行跨度 |
| `sizeimage` | `3133440` | 紧密容量加 23,040 字节尾部填充 |
| `NV12 tight` | `3110400` | 1920×1080×1.5，实际图像区 |

`sizeimage` 比紧密容量多出的 23,040 字节在缓冲区末尾，第 13 课取帧后实测这段全为 0，不影响按紧密 NV12 解码。`bytesperline` 在这个驱动里也不能当行跨度用；如果按 2880 逐行算，UV 的起点会算错。这两个值都按驱动实际返回处理，换成别的平台要重新核对。

## 验收

- `driver`、`card` 为 `sunxi-vin`；
- `pixelformat` 为 `NV12`，宽高与 HDMI 源一致；
- `num_planes` 为 1，`bytesperline` 不小于宽度，`sizeimage` 不小于紧密容量；
- 程序退出码为 0，`/etc/init.d/S99kvm start` 后四个服务都能重新拉起。

## 常见问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| `Device or resource busy` | KVM 视频服务正在采集 | 先 `/etc/init.d/S99kvm stop` |
| `No such file or directory` | `lt6911c_mipi` 未加载 | 检查 `lsmod` 与 `/etc/init.d/S91camera` |
| 格式不是 NV12 | 服务仍在运行或 HDMI 未锁定 | 停服务后重试，先确认输入已锁定 |

拿到这三个值，就可以申请缓冲区了。下一课会把 `REQBUFS`、`QUERYBUF`、`mmap` 和 `STREAMON` 串起来，保存第一帧 NV12。
