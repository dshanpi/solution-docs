export const course = {
  schemaVersion: 2,
  courseId: 't527-web-kvm-ai-learning',
  version: '1.0.0-rc.1',
  title: 'T527 Web KVM 嵌入式 AI 辅助学习',
  boardProfile: 'avaota-a1-t527-lt6911c',
  sdkProfile: {
    platform: 'linux', linuxDev: 'buildroot', chip: 't527', board: 'avaota_a1',
    kernel: 'linux-5.15', arch: 'arm64', buildroot: '202205', toolchain: 'gcc-10.3.1',
    root: '/home/ubuntu/T527/AvaotaA1-Tina5-SDK_V1',
  },
  workspaceRoot: 'tutorial-examples/web-kvm',
  evidencePolicy: ['document', 'answer', 'command', 'artifact', 'verification', 'reflection'],
};

const recordedImage = {
  path: 'out/t527/avaota_a1/buildroot/t527_linux_avaota_a1_uart0.img',
  bytes: 718863360,
  md5: 'c2b21dad6a052e83332ab5adfc26db1e',
  sha256: 'ecf3e222beaf24e7ddb89bc1caf1a5b340e9286ad49456c53824493aeefc111b',
};

const hilDefinitions = new Map([
  [2, {
    actionId: 'verify-wiring-and-power', target: 'avaota-a1-t527-lt6911c',
    instructions: '断电核对 FPC、H1 和共地；上电后记录 3.3V、RSTN 与模块状态。',
    requiredEvidenceKinds: ['wiring-observation', 'power-observation'],
    expectedObservations: ['LT6911C 与 Avaota A1 接线符合课程表格', '模块 3.3V 与复位电平符合要求'],
  }],
  [3, {
    actionId: 'verify-serial-boot', target: 'avaota-a1-t527-lt6911c',
    instructions: '绑定唯一串口，冷启动板卡并保存从 BootROM 到登录提示的脱敏日志摘要。',
    requiredEvidenceKinds: ['serial-boot-log'],
    expectedObservations: ['串口参数为 115200 8N1', '冷启动后出现可识别的登录或 shell 提示'],
  }],
  [4, {
    actionId: 'verify-board-network', target: 'avaota-a1-t527-lt6911c',
    instructions: '记录板端地址、路由及 Mac 到板端的连通性；地址不得写死为教程示例值。',
    requiredEvidenceKinds: ['network-observation'],
    expectedObservations: ['板端获得局域网地址', 'Mac 与板端双向连通'],
  }],
  [8, {
    actionId: 'verify-hdmi-input', target: 'avaota-a1-t527-lt6911c',
    instructions: '连接 1080p60 HDMI 源，保存驱动识别、输入锁定和 /dev/video0 观测。',
    requiredEvidenceKinds: ['driver-observation', 'hdmi-observation'],
    expectedObservations: ['LT6911C 驱动加载且 /dev/video0 存在', '输入锁定为 1920x1080@60 并能产生帧'],
  }],
  [24, {
    actionId: 'full-system-acceptance', target: 'avaota-a1-t527-lt6911c',
    instructions: '使用已记录镜像完成受控烧录、冷启动、视频/HID/热插拔和 2 小时稳定性验收。',
    requiredEvidenceKinds: ['flash-verification', 'cold-boot-log', 'video-observation', 'hid-observation', 'recovery-observation', 'soak-observation'],
    expectedObservations: ['烧录介质与记录镜像 SHA256 一致', '冷启动自动进入服务', '视频、键盘和鼠标闭环正常', 'HDMI 与 USB 重连可恢复', '连续运行至少 7200 秒无不可恢复故障'],
    minimumDurationSeconds: 7200,
    image: recordedImage,
  }],
]);

const rows = [
  [1,'preparation/hardware','认识 Web KVM 与硬件组成','hardware','software','./scripts/verify_architecture.sh'],
  [2,'preparation/wiring-firmware','连接采集模块与主板','hardware','hil','./scripts/verify_wiring_contract.sh'],
  [3,'preparation/serial-console','通过 H1 串口进入 T527','terminal','hil','./scripts/verify_serial_contract.sh'],
  [4,'preparation/wifi-network','连接 WiFi 与确认网络','terminal','hil','./scripts/verify_network_contract.sh'],
  [5,'drivers/sdk-build','构建内核、设备树和应用','build','software','./scripts/build_all.sh --check'],
  [6,'drivers/device-tree','把原理图连接写进设备树','code','software','./scripts/verify_dts.sh'],
  [7,'drivers/lt6911c-driver','适配 LT6911C 子设备驱动','code','software','./scripts/verify_driver.sh'],
  [8,'drivers/hdmi-signal','判断 HDMI 是否真的有输入','terminal','hil','./scripts/verify_hdmi_contract.sh'],
  [9,'capture/video-format','查询设备能力和图像格式','terminal','simulation','./scripts/run_simulation.sh video-format'],
  [10,'capture/first-frame','用 MMAP 获取并保存第一帧','code','simulation','./scripts/run_simulation.sh first-frame'],
  [11,'capture/capture-loop','连续采集、超时与资源释放','code','simulation','./scripts/run_simulation.sh capture-loop'],
  [12,'encoding/encoder-memory','把采集缓冲区交给硬件编码器','code','simulation','./scripts/run_simulation.sh encoder-memory'],
  [13,'encoding/h264-file','编写 HDMI 转 H.264 文件程序','build','software','./scripts/build_all.sh --arm64'],
  [14,'encoding/video-performance','测量帧率、码率与延迟','terminal','simulation','./scripts/run_simulation.sh performance'],
  [15,'usb-hid/gadget-config','让 T527 枚举成 USB HID 设备','terminal','simulation','./scripts/run_simulation.sh gadget'],
  [16,'usb-hid/keyboard-reports','编写 USB 键盘报告程序','code','simulation','./scripts/run_simulation.sh keyboard'],
  [17,'usb-hid/mouse-reports','编写相对鼠标和绝对鼠标程序','code','simulation','./scripts/run_simulation.sh mouse'],
  [18,'usb-hid/usb-recovery','处理 USB 冲突、断线与状态','terminal','simulation','./scripts/run_simulation.sh usb-recovery'],
  [19,'integration/service-architecture','理解服务和网页的系统职责','document','software','./scripts/verify_architecture.sh'],
  [20,'integration/video-socket','通过视频 Socket 接入网页','code','simulation','./scripts/run_simulation.sh video-socket'],
  [21,'integration/capture-control','控制采集启停与远程操作','code','simulation','./scripts/run_simulation.sh capture-control'],
  [22,'deployment/startup','组织运行文件并实现开机启动','build','software','./scripts/verify_startup.sh'],
  [23,'deployment/troubleshooting','按链路定位无画面与无键鼠','terminal','simulation','./scripts/run_simulation.sh troubleshooting'],
  [24,'deployment/acceptance','完整验收与课程项目交付','report','software','./scripts/acceptance.sh --software'],
];

const objective = (title) => `理解“${title}”在 T527 Web KVM 链路中的职责，并能用证据解释结果。`;

export const labs = rows.map(([number, path, title, tool, evidenceSource, verify]) => ({
  stepId: `t527-kvm-${String(number).padStart(2, '0')}`,
  number, path, title, learningObjective: objective(title),
  content: `docs/course/${path}.md`,
  comprehensionCheck: `说明本章输入、输出以及失败时最先检查的证据。`,
  studentAction: `在锁定的 Avaota A1 SDK 学习工作区完成“${title}”实验，不直接修改其他板型。`,
  toolScope: [tool],
  riskLevel: hilDefinitions.has(number) ? 'hardware-confirmation' : 'software-safe',
  expectedObservation: evidenceSource === 'hil' ? '软件合同通过；接入实机后获得对应设备观测。' : '命令退出码为 0，并生成带 SHA-256 的结构化证据。',
  verificationRules: [{id: 'command-exit-zero', kind: 'command', command: verify, expectedExitCode: 0}],
  commonMistakes: ['使用 demo_linux_aiot 的旧路径', '把模拟证据写成真实 HIL', '只看命令发送而未检查退出码和产物'],
  hintLevels: {
    L1: '先确认本章输入、输出与证据来源。',
    L2: `检查 ${verify} 的首个失败阶段。`,
    L3: '只对当前步骤相关文件做最小修改，并重新生成一次独立 attempt。',
    L4: `参考已签名课程包中的 solution/${String(number).padStart(2,'0')}，报告标记为 ASSISTED_PASS。`,
  },
  requiredEvidence: [{kind: 'command', source: evidenceSource === 'hil' ? 'software' : evidenceSource}, {kind: 'reflection', source: 'student'}],
  reflectionPrompt: '解释本章结果为何可信，并指出哪些结论仍需要真实硬件。',
  hilVerification: hilDefinitions.get(number) ?? null,
}));

export const advancedLabs = ['hdmi-hotplug','dynamic-resolution','low-latency','optional-features'].map((name, index) => ({
  stepId: `t527-kvm-advanced-${String.fromCharCode(97 + index)}`,
  path: `advanced/${name}`,
  required: false,
  riskLevel: index < 2 ? 'hardware-confirmation' : 'software-safe',
}));
