---
title: "通过 HTTP 传输并校验文件"
sidebar_label: "06 传输与校验文件"
sidebar_position: 6
slug: /course/preparation/file-transfer
---

# 通过 HTTP 传输并校验文件

上一课已经在开发主机上生成过 ARM64 程序。这一课把文件送到板端，核对 SHA-256 后运行，确认开发主机和 T527 之间的文件通道可用。文件只放到板端 `/tmp`，不替换 `/userdata` 里的生产程序，也不停止 KVM 服务；正式程序的备份和替换放在第 25 课。

## 准备一个传输样本

在开发主机上建一个固定目录，编一个最小程序作为样本：

```bash
export SDK_ROOT=~/T527_Tina5.0_SDK
export CROSS_COMPILE="$SDK_ROOT/out/toolchain/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-"
mkdir -p ~/kvm-transfer
cd ~/kvm-transfer

cat > hello.c <<'EOF'
#include <stdio.h>

int main(void)
{
    puts("Hello, T527!");
    return 0;
}
EOF

"${CROSS_COMPILE}gcc" -Wall -Wextra -o hello hello.c
file hello
sha256sum hello
```

`file` 应显示 `ELF 64-bit LSB executable, ARM aarch64`。板端没有编译器，这个程序必须在开发主机交叉编译，不能把 `hello.c` 拷到板端再编译。

## 让开发主机提供文件

在同一个目录启动临时 HTTP 服务：

```bash
cd ~/kvm-transfer
python3 -m http.server 8000 --bind 0.0.0.0
```

终端会停在服务上，显示类似：

```text
Serving HTTP on 0.0.0.0 port 8000 (http://0.0.0.0:8000/) ...
```

保持这个终端运行。另开一个开发主机终端查地址：

```bash
hostname -I
```

记下板端可达的 IPv4 地址。示例使用 `192.168.1.69`，实际以 `hostname -I` 为准。板端和开发主机必须在同一网段，或已经配置好可达的路由。

## 板端下载并校验

在板端串口或终端建立临时目录并下载：

```sh
mkdir -p /tmp/kvm-transfer
cd /tmp/kvm-transfer
wget -q -O hello http://192.168.1.69:8000/hello
chmod +x hello
sha256sum hello
./hello
```

把板端 `sha256sum` 的结果和开发主机上 `sha256sum hello` 的结果逐项比较，必须一致。运行程序应输出：

```text
Hello, T527!
```

这三步分别证明：文件完整到达、可执行权限正确、程序能在 T527 上运行。只看到 `wget` 返回成功还不够，散列不一致时不要运行或覆盖任何文件。

`/tmp` 在重启后会清空，适合放临时文件。停止开发主机的 HTTP 服务时按 `Ctrl-C`，不要让它长期占用 8000 端口。

## 传输二进制时检查什么

本课传的是 AArch64 程序；后面课件里的采集、编码和键鼠程序也走同一条通道：

| 检查 | 命令 | 通过条件 |
| --- | --- | --- |
| 架构 | `file hello` | `ARM aarch64` |
| 完整性 | `sha256sum` | 两端一致 |
| 权限 | `chmod +x` | 能直接执行 |
| 运行结果 | `./hello` | 输出 `Hello, T527!` |

板端 `/tmp` 下执行程序只改变临时目录；不要在这一课替换 `/userdata/100ask_kvm/bin` 下的文件，也不要在更新时依赖即将断开的网页终端。

## 常见问题

| 现象 | 原因与处理 |
| --- | --- |
| `Connection refused` | HTTP 服务没启动，或开发主机地址/端口不对。确认服务终端还在运行，重新执行 `hostname -I` |
| `404 Not Found` | 下载的文件名和 HTTP 目录里的文件名不一致。先 `ls ~/kvm-transfer` 核对 |
| 散列值不一致 | 下载中断、文件被替换或目录选错。删除 `/tmp/kvm-transfer/hello` 后重下，不要继续运行 |
| `Permission denied` | 忘记 `chmod +x`，或文件系统不允许执行。先用 `ls -l` 检查权限 |
| `Exec format error` | 用了主机 `gcc` 而不是交叉编译器。检查 `file` 是否为 `ARM aarch64` |
| 板端访问不到开发主机 | 两者不在同一网段，或开发主机防火墙拦截 8000 端口。检查 WiFi、网络模式和地址 |

## 验收

- 开发主机能把 `hello` 编成 AArch64 程序并算出 SHA-256。
- 板端通过 HTTP 下载到 `/tmp/kvm-transfer/hello`，两端散列一致。
- `chmod +x` 后能在板端运行并输出 `Hello, T527!`。
- 没有替换生产程序，也没有停止 KVM 服务。

正式程序的备份、停服务、替换、重启和回滚属于第 25 课部署流程；下一课先用参考镜像检查完整 KVM 服务。
