# 第十四课：连续采集

这一课把取帧放进循环：反复 `QBUF`/`DQBUF` 使用同一组缓冲区，统计帧率和序号连续性，并处理超时、Ctrl-C 和退出清理。

## 文件

| 文件 | 用途 |
| --- | --- |
| `capture_loop.c` | 连续采集并统计帧率、丢帧和错误 |
| `Makefile` | 使用 SDK 里的交叉编译器编译 |

## 编译

```bash
make SDK_ROOT=~/T527_Tina5.0_SDK
```

## 传到板子

按教程第 06 章的方法用 HTTP 下载并校验 MD5。

## 运行

```sh
/etc/init.d/S99kvm stop
./capture_loop /dev/video0 300
/etc/init.d/S99kvm start
```

程序启动时自己等 HDMI 锁定，最多 10 秒。300 帧约 5 秒；按 Ctrl-C 可以提前结束，仍会打印摘要并释放资源。

## 实测输出（2026-10-26）

板端 Linux 5.15.147，HDMI 1920×1080@60：

```text
waiting for HDMI input...
input locked: 1920x1080@60
buffers=4 sizeimage=3133440 bytesperline=2880
frame=60 sequence=61 fps=59.3
frame=120 sequence=121 fps=59.7
frame=180 sequence=181 fps=59.8
frame=240 sequence=241 fps=59.8
frame=300 sequence=301 fps=59.9
frames=300 elapsed=5.011s fps=59.87 gaps=0 errors=0
```

`sequence` 的起点不固定，它跟随输入锁定后的帧计数。判断连续性看相邻差值：这次每 60 帧正好差 60，`gaps=0`。

## 两个实现细节

- **序号要在 `QBUF` 之前取出来。** 这块 VIN 驱动在 `QBUF` 后会清空 `v4l2_buffer` 字段。直接用 `buf.sequence` 打印时每行都是 0，先存到本地变量才正确。
- **信号处理函数只置标志。** `SIGINT` 到来时 `poll` 可能返回 `EINTR`，循环检查标志后走统一清理出口；打印和释放都放在循环外。

## 验收

- 请求 300 帧，收到 300 帧，`errors=0`；
- `fps` 在 59–60 之间，`gaps=0`；
- 退出后设备空闲，`pidof kvm_video` 为空，KVM 能重新拉起；
- Ctrl-C 中断也能打印摘要并干净退出。

## 常见问题

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| `Device or resource busy` | 还有进程占用采集设备 | 停 KVM，确认 `pidof kvm_video` 为空 |
| `等待 2000 ms 没有收到帧` | 采集中输入掉锁 | 看 `dmesg`，检查 HDMI 源和线缆 |
| `gaps` 不为 0 | 驱动丢帧或输入重新锁定 | 结合 `dmesg` 和序号差值判断 |
| 帧率低于 60 | 源模式不对或加入了耗时处理 | 先确认输入模式，再对比处理耗时 |

## 验证记录

2026-10-26 在样机上完成：交叉编译、HTTP 传输、停服务连续采集 300 帧、恢复服务；帧率 59.87，`gaps=0`、`errors=0`，退出后设备可再次打开，四个服务正常拉起。
