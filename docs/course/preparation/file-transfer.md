---
title: "从开发主机传输核心程序"
sidebar_label: "06 传输核心程序"
sidebar_position: 6
slug: /course/preparation/file-transfer
---

# 从开发主机传输核心程序

完成交叉编译后，需要把新的用户态程序放到 T527 上验证。当前配套镜像提供 `wget`，但没有发现 `sshd`、`dropbear` 或 `scp`，因此使用“Ubuntu 虚拟机临时提供 HTTP 文件，板端主动下载”的方式。文件先下载到 `/tmp` 并校验，确认无误后再停止服务、备份旧文件和替换程序。

本课只更新 `kvm_app`、`kvm_hid`、`kvm_platform`、`kvm_video` 这四个用户态核心程序。内核、设备树和 `.ko` 模块必须与内核版本成套部署，不能套用本课的复制命令。

## 让开发主机提供更新文件


在 Ubuntu 虚拟机中进入 SDK 根目录，先把要更新的核心二进制复制到一个临时目录：

```bash
cd ~/T527_Tina5.0_SDK
RELEASE_DIR=$(mktemp -d /tmp/kvm-release.XXXXXX)
cp out/t527/demo_linux_aiot/buildroot/buildroot/target/userdata/100ask_kvm/bin/kvm_app "$RELEASE_DIR/"
cp out/t527/demo_linux_aiot/buildroot/buildroot/target/userdata/100ask_kvm/bin/kvm_hid "$RELEASE_DIR/"
cp out/t527/demo_linux_aiot/buildroot/buildroot/target/userdata/100ask_kvm/bin/kvm_platform "$RELEASE_DIR/"
cp out/t527/demo_linux_aiot/buildroot/buildroot/target/userdata/100ask_kvm/bin/kvm_video "$RELEASE_DIR/"
sha256sum "$RELEASE_DIR"/*
python3 -m http.server 8000 --bind 0.0.0.0 --directory "$RELEASE_DIR"
```

保持这个终端运行。另开一个 Ubuntu 终端执行 `hostname -I`，记下开发主机在 T527 所在局域网中的 IPv4 地址。示例中的虚拟机地址是 `192.168.1.69`，实际传输时以 `hostname -I` 的结果为准。

## 下载并校验核心二进制

回到串口终端，在板端先建立临时目录。把下面的 `192.168.1.69` 替换为开发主机的实际地址：

```sh
rm -rf /tmp/kvm-update
mkdir -p /tmp/kvm-update
for name in kvm_app kvm_hid kvm_platform kvm_video; do
    wget -q -O "/tmp/kvm-update/$name" "http://192.168.1.69:8000/$name" || exit 1
done
sha256sum /tmp/kvm-update/*
```

将板端显示的散列值与开发主机 `sha256sum "$RELEASE_DIR"/*` 的结果逐项比较。散列值不一致时不要覆盖正在运行的文件，先检查主机 IP、WiFi 稳定性和下载目录。

下载成功只代表文件完整到达 `/tmp`，还没有改变正在运行的 KVM。`/tmp` 中的文件在重启后会消失，这正适合先做校验和试运行。

## 更新板端程序

核心程序由 `/etc/init.d/S99kvm` 统一启动和停止。更新前通过串口保留旧文件，并停止整套服务：

```sh
cd /userdata/100ask_kvm/bin
mkdir -p /userdata/100ask_kvm/backup
for name in kvm_app kvm_hid kvm_platform kvm_video; do
    cp "$name" "/userdata/100ask_kvm/backup/$name.$(date +%Y%m%d-%H%M%S)"
done

/etc/init.d/S99kvm stop
for name in kvm_app kvm_hid kvm_platform kvm_video; do
    install -m 0755 "/tmp/kvm-update/$name" "/userdata/100ask_kvm/bin/$name"
done
/etc/init.d/S99kvm start
sync
```

停止服务后，网页、视频和 USB HID 会暂时断开，这是预期现象。使用串口执行更新，不能依赖即将被停止的网页终端。重新启动后检查进程和日志：

```sh
pidof kvm_app kvm_video kvm_hid kvm_platform
tail -30 /tmp/kvm-supervisor.log
tail -30 /tmp/kvm-app.log
tail -30 /tmp/kvm_video.log
```

最后刷新浏览器，确认视频持续更新，并测试键盘、鼠标和终端。出现异常时先恢复备份文件，再重新启动服务；内核、设备树和 `.ko` 模块属于另一组与内核版本匹配的产物，不能只替换用户态二进制来更新。

完成本课后，应能把经过校验的核心程序传到 T527，并在不依赖网页终端的情况下完成服务重启。下一步进入[运行参考镜像](./reference-image.md)，检查更新后的 KVM 服务。
