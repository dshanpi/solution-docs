# 第一课：查询采集格式

这一课只读 `/dev/video0`：查询驱动能力，读取当前图像格式，不改格式、不申请缓冲区、不启动采集。它是后面取帧、编码、对接服务之前必须先做的一步。

## 文件

| 文件 | 用途 |
| --- | --- |
| `query_format.c` | 查询能力与当前格式 |
| `Makefile` | 使用 SDK 里的交叉编译器编译 |

## 编译

示例放在 SDK 外面，编译时用 `SDK_ROOT` 指向 SDK：

```bash
make SDK_ROOT=~/T527_Tina5.0_SDK
```

生成的 `query_format` 是 AArch64 程序。编译器、参数与原因见教程第 05 章「准备主机、SDK 和调试环境」。

## 传到板子

板端没有 `scp`，按教程第 06 章的方法：在开发主机用 `python3 -m http.server` 提供文件，板端 `wget` 下载，校验 MD5 后再运行。

```sh
wget -q -O /tmp/query_format http://192.168.1.69:8000/query_format
chmod +x /tmp/query_format
```

## 运行

当前 `/dev/video0` 由 KVM 视频服务独占，直接查询会返回 `Device or resource busy`。先停服务，查完再拉起：

```sh
/etc/init.d/S99kvm stop
/tmp/query_format /dev/video0
/etc/init.d/S99kvm start
```

## 实测输出（2026-10-26）

板端 Linux 5.15.147，HDMI 输入 1920×1080@60：

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

## 输出说明

| 字段 | 实测值 | 含义 |
| --- | --- | --- |
| `device_caps` | `0x05201000` | 当前节点的能力；含 `VIDEO_CAPTURE_MPLANE` 和 `STREAMING` 才能走后面的流程 |
| `pixelformat` | `NV12` | Y 平面 + UV 交错，后续按这个布局保存 |
| `num_planes` | `1` | 虽然是 MPLANE API，NV12 在这里仍是一个内存平面 |
| `bytesperline` | `2880` | 一行占 2880 字节，比 1920 宽，说明驱动做了对齐 |
| `sizeimage` | `3133440` | 驱动要求的缓冲区容量，比紧密排列的 3110400 多 |
| `NV12 tight` | `3110400` | 只用于理解 1920×1080×1.5，不能拿来当映射长度 |

`bytesperline` 和 `sizeimage` 都以驱动返回值为准，不能自己按宽高算。

## 验收

- `driver`、`card` 是 `sunxi-vin`；
- `pixelformat` 为 `NV12`，宽高与 HDMI 源一致；
- `num_planes` 为 1，`bytesperline` ≥ 宽度，`sizeimage` ≥ 紧密容量；
- 程序退出码为 0，KVM 服务能正常拉起。

## 常见问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| `Device or resource busy` | KVM 视频服务正在采集 | 先 `/etc/init.d/S99kvm stop` |
| `No such file or directory` | `lt6911c_mipi` 未加载 | 检查 `lsmod` 和 `S91camera` |
| 格式不是 NV12 | 采集服务仍在运行或驱动未初始化 | 停服务后重试，确认 HDMI 已锁定 |

## 验证记录

2026-10-26 在样机上完成：交叉编译（GCC 10.3 AArch64）、HTTP 传输、MD5 校验、板端运行，输出与上表一致；查询结束后 `/etc/init.d/S99kvm start` 能把四个服务重新拉起。
