# 24 C HID 服务接入 Go 后端

用 configfs 配置键盘、绝对鼠标、相对鼠标，提供 HID IPC 服务，让 Go 后端的网页输入进入 `/dev/hidg*`。

| 文件 | 用途 |
| --- | --- |
| kvm_hid_service.c | Gadget 配置、HID IPC、报告写入、租约与释放 |
| Makefile | 交叉编译（不链接 SDK 编码库） |
| run-hid-service.sh | 建教程根目录，仅替换 HID 服务，退出恢复生产 |

## 编译

```bash
make SDK_ROOT=~/T527_Tina5.0_SDK
file kvm_hid_service
```

产物为 AArch64 ELF；板端没有编译器。

## 运行

主机在课件目录起 HTTP 服务，板端下载 `kvm_hid_service` 和 `run-hid-service.sh`，核对 MD5 后：

```sh
sh run-hid-service.sh
```

脚本创建 `/tmp/kvm24root`，把 `kvm_hid` 指向本课程序，平台、Go 后端和视频仍用生产版本；按 `Ctrl-C` 恢复 `S99kvm`。

打开网页，点击远程画面获得焦点，按键并在被控设备上用 evtest 验收。配置变化会重新枚举 Gadget，设备名以 `ls /dev/input/by-id/` 为准。

## 实测（2026-10-27）

- Go 后端连接 `/run/kvm/hid.sock`，服务日志 `input owner connected`、`configured keyboard=1 absolute=1 relative=1`。
- 浏览器按键到达被控设备：`KEY_A value 1/0`。
- 浏览器绝对鼠标移动和点击：`ABS_X/ABS_Y` 在 4294/20948 一带变化，`BTN_LEFT` 出现 1/0。
- 配置相同复用时不会重建 Gadget；只有设备集合或 USB 身份变化才重新配置。
- `Ctrl-C` 后四个生产服务与 UDC `configured` 恢复，主机回到正式产品名。

## 限制

- 只支持键盘、绝对鼠标、相对鼠标；不含 MTP、USB 音频、mass storage。
- 单输入所有者、3 秒租约；成功写入只代表 USB 驱动接受报告。
- 本轮浏览器验收覆盖键盘和绝对鼠标；相对鼠标和滚轮需在对应鼠标模式下另测。
