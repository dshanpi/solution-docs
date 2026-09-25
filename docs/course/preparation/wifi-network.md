---
title: "连接 WiFi 与确认网络"
sidebar_label: "04 连接 WiFi"
sidebar_position: 4
slug: /course/preparation/wifi-network
---

# 连接 WiFi 与确认网络

上一课已经能在串口中执行板端命令。现在让 T527 加入操作电脑和 Ubuntu 虚拟机所在的局域网，后续才能打开 KVM 网页、从虚拟机下载程序。

以下命令全部在 **T527 的串口终端**执行。配套 Buildroot 系统使用 `wifi` 命令管理无线网络。

## 确认无线工具和网卡

```sh
wifi -h
ip link show wlan0
```

配套工具帮助中包含：

```text
wifi -o sta          打开 STA 模式
wifi -s              扫描热点
wifi -c ssid [passwd] 连接热点
wifi -t ssid         连接已经保存的热点
wifi -a [enable/disable] 自动重连
```

STA 模式表示主板作为客户端连接路由器。配套系统的 `/etc/init.d/S90wifi_start` 在启动时加载无线驱动、启动 `wifi_daemon` 并打开 STA 模式；刚装好的系统仍需要填写实际热点的信息。

若没有 `wifi` 命令或没有 `wlan0`，先检查镜像是否包含无线工具和驱动。这不是密码错误，反复输入连接命令不会补齐缺失的驱动。

## 扫描并连接热点

在串口执行：

```sh
wifi -o sta
wifi -s
```

等待扫描结果，找到自己的热点名称（SSID）。以下以实验热点 `KVM-Lab` 为例，执行前替换名称和密码：

```sh
wifi -c 'KVM-Lab' '替换为实际密码'
```

当前客户端先把参数发送给 `wifi_daemon`，连接和获取地址的结果随后才出现。因此，命令返回提示符不等于联网成功，要继续检查下一节的状态。

:::note 名称中的空格与日志

当前 SDK 的 WiFi 工具内部会再次解析空格，只有 shell 引号还不够。例如热点名为 `KVM Lab`，要写成 `'KVM\ Lab'`，让反斜杠传给 WiFi 工具；密码里的空格也按相同方式处理。首次实验可使用不含空格的热点名称和密码。

连接命令及该版本 WiFi 日志可能显示密码。分享串口日志或截图前，删去实际密码。

:::

## 确认连接状态、IP 和路由

```sh
wpa_cli -p /etc/wifi/wpa_supplicant/sockets -i wlan0 status
ip -4 addr show wlan0
ip route
```

观察三处结果：

| 结果 | 说明 |
| --- | --- |
| `wpa_state=COMPLETED` | 已完成与热点的认证 |
| `wlan0` 的 `inet` 地址 | 已取得 IPv4 地址，后续浏览器访问这个地址 |
| `default via … dev wlan0` | 默认网关，用于访问本地子网以外的地址 |

一次实机检查中，板端地址为 `192.168.1.47/24`，网关为 `192.168.1.1`。这是输出示例，不能作为所有开发板的固定地址。驱动管理程序会调用 DHCP 客户端获取地址，不需要再并行启动另一套 `wpa_supplicant`。

然后在串口测试到 Ubuntu 虚拟机的连通性。把示例 IP 改为虚拟机的实际局域网地址：

```sh
ping -c 3 192.168.1.69
```

收到回复说明该方向可达。若 ping 被防火墙禁用，仍需在文件传输课通过 HTTP 下载确认通信，不能单凭 ping 超时判定 WiFi 断开。

虚拟机需要有板端可达的地址。使用桥接网络通常可以让它和板端加入同一路由器所在的网段；如果采用 NAT，需要配置相应的访问或端口转发。Windows 能登录虚拟机，并不代表 T527 也能访问虚拟机。

## 保存连接与再次上电

配套 WiFi 实现会保存成功连接的网络配置。已经保存过热点时，可执行：

```sh
wifi -t 'KVM-Lab'
wifi -a enable
```

`wifi -t` 使用已保存的信息连接热点；`wifi -a enable` 开启当前无线服务的自动重连。掉线重连与重启后的自动联网是两项检查：后者还依赖开机脚本及配置是否保留。

准备好结束本次操作后，通过串口执行 `sync`、`reboot`，等待系统启动，再检查 `wlan0` 地址。若没有自动连接，用 `wifi -t` 重新连接，并检查 `S90wifi_start` 的启动日志；不要把密码明文追加进启动脚本来代替排查。

## 网络不通时按层检查

| 现象 | 检查位置 |
| --- | --- |
| 找不到 `wlan0` | `ip link`、启动日志、`/etc/init.d/S90wifi_start` 中的驱动加载结果 |
| 扫描不到自己的热点 | 热点是否开启、距离与信号、是否为隐藏 SSID |
| 认证未完成 | SSID、密码、空格转义、热点加密方式 |
| 认证完成但无 IPv4 | 路由器 DHCP、地址池及无线连接日志 |
| 有 IP，但电脑打不开网页 | 电脑与板端是否互通、访客网络隔离、KVM 服务是否监听 |
| 能打开网页但下载不到虚拟机文件 | 虚拟机网络模式、HTTP 服务绑定地址、端口与防火墙 |

完成后记录板端 IP 和虚拟机 IP。下一课[准备开发环境](./development-environment.md)检查 SDK 与编译器，随后通过网络把文件传到板端。

<!-- LYNX-LAB:BEGIN -->

## AI 辅助实践闭环

- **稳定步骤**：`t527-kvm-04`
- **学习目标**：理解“连接 WiFi 与确认网络”在 T527 Web KVM 链路中的职责，并能用证据解释结果。
- **理解检查**：说明本章输入、输出以及失败时最先检查的证据。
- **学生操作**：在锁定的 Avaota A1 SDK 学习工作区完成“连接 WiFi 与确认网络”实验，不直接修改其他板型。
- **工具范围**：`terminal`
- **风险级别**：`hardware-confirmation`
- **预期现象**：软件合同通过；接入实机后获得对应设备观测。

确定性验证：

```bash
cd tutorial-examples/web-kvm
./scripts/verify_network_contract.sh
```

必须保存命令退出码、产物摘要和证据来源；模拟结果只能标记为 `simulation`。完成后回答：解释本章结果为何可信，并指出哪些结论仍需要真实硬件。

<details>
<summary>分级提示</summary>

1. 先确认本章输入、输出与证据来源。
2. 检查 ./scripts/verify_network_contract.sh 的首个失败阶段。
3. 只对当前步骤相关文件做最小修改，并重新生成一次独立 attempt。
4. 参考已签名课程包中的 solution/04，报告标记为 ASSISTED_PASS。

</details>

<!-- LYNX-LAB:END -->
