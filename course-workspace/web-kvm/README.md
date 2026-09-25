# T527 Web KVM 教学实验链

目标固定为 `t527 / avaota_a1 / linux-5.15 / buildroot / arm64` 与 LT6911C。该目录是课程配套源码事实源；旧 `kvm_video_demo` 仅作历史参考。

`make host` 运行协议/HID 的主机可重复构建，`make arm64` 交叉编译板端程序。`scripts/acceptance.sh --software` 生成带来源与摘要的第一阶段证据；没有实体板卡时所有 HIL 项保持 `NOT_VERIFIED`。
