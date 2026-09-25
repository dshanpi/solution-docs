# 课程、代码与 SDK 一致性

本课程的唯一可执行源码位于仓库的 `course-workspace/web-kvm`。课程包不会从网络下载示例，也不会在导入后替换源码。

`npm run course:pack` 会生成以下本地交付物：

- `.lynx-course`：Ed25519 签名封装，内部含 24 课 manifest、讲义、示例源码、SDK 工作区和 provenance；
- `.md5` 与 `.sha256`：课程包传输校验；
- `release.lock.json`：课程 Git 基线 commit、dirty 状态摘要、课程源码摘要、SDK provenance 摘要、签名者指纹和课程包双哈希。

`provenance/sdk-reference` 固定 SDK 的 repo manifest、每个子仓 commit、dirty diff，以及编译产物和已有镜像的 MD5/SHA256。LYNX 导入时先验证课程包签名，再逐文件核对 `provenance.lock.json` 中的源码 SHA256；不一致时拒绝安装。

软件验收与实机验收必须分开记录。未连接 Avaota A1、LT6911C、HDMI 源和 USB Host 时，HIL 结果固定为 `NOT_VERIFIED`，不能从软件模拟结果推断为通过。

课程 Manifest v2 为第 2、3、4、8、24 课定义类型化 HIL 规则。LYNX 必须先绑定稳定 `board_uid`，再由学生消费一次性确认令牌并提交规则要求的设备证据；AI、前端按钮和软件模拟均不能把 HIL 改成 `PASS`。第 24 课的完整验收固定使用 provenance 已记录的 T527 镜像，并要求连续运行至少 7200 秒。
