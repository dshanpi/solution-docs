export const stages = [
  {
    "id": "preparation",
    "title": "认识硬件与实验环境",
    "position": 3,
    "description": "完成硬件、串口、网络与开发工具准备，使用参考镜像完成一次 KVM 操作。",
    "lessons": [
      {
        "title": "认识 Web KVM 与硬件组成",
        "number": 1,
        "href": "/docs/course/preparation/hardware",
        "shortTitle": "认识 Web KVM",
        "stage": "preparation"
      },
      {
        "title": "连接采集模块与主板",
        "number": 2,
        "href": "/docs/course/preparation/wiring-firmware",
        "shortTitle": "接线与供电检查",
        "stage": "preparation"
      },
      {
        "title": "通过 H1 串口进入 T527",
        "number": 3,
        "href": "/docs/course/preparation/serial-console",
        "shortTitle": "连接调试串口",
        "stage": "preparation"
      },
      {
        "title": "连接 WiFi 与确认网络",
        "number": 4,
        "href": "/docs/course/preparation/wifi-network",
        "shortTitle": "连接 WiFi",
        "stage": "preparation"
      },
      {
        "title": "准备主机、SDK 和调试环境",
        "number": 5,
        "href": "/docs/course/preparation/development-environment",
        "shortTitle": "准备开发环境",
        "stage": "preparation"
      },
      {
        "title": "从开发主机传输核心程序",
        "number": 6,
        "href": "/docs/course/preparation/file-transfer",
        "shortTitle": "传输核心程序",
        "stage": "preparation"
      },
      {
        "title": "使用参考镜像运行完整 KVM",
        "number": 7,
        "href": "/docs/course/preparation/reference-image",
        "shortTitle": "运行参考镜像",
        "stage": "preparation"
      }
    ]
  },
  {
    "id": "drivers",
    "title": "适配 HDMI 采集驱动",
    "position": 4,
    "description": "构建底层软件，将原理图连接对应到设备树和驱动。",
    "lessons": [
      {
        "title": "构建内核、设备树和应用",
        "number": 8,
        "href": "/docs/course/drivers/sdk-build",
        "shortTitle": "构建与部署",
        "stage": "drivers"
      },
      {
        "title": "把原理图连接写进设备树",
        "number": 9,
        "href": "/docs/course/drivers/device-tree",
        "shortTitle": "适配设备树",
        "stage": "drivers"
      },
      {
        "title": "适配 LT6911C 子设备驱动",
        "number": 10,
        "href": "/docs/course/drivers/lt6911c-driver",
        "shortTitle": "适配 LT6911C 驱动",
        "stage": "drivers"
      },
      {
        "title": "判断 HDMI 是否真的有输入",
        "number": 11,
        "href": "/docs/course/drivers/hdmi-signal",
        "shortTitle": "检查 HDMI 输入",
        "stage": "drivers"
      }
    ]
  },
  {
    "id": "capture",
    "title": "编写 V4L2 采集程序",
    "position": 5,
    "description": "从查询格式开始，独立保存原始图像并完成连续采集。",
    "lessons": [
      {
        "title": "查询设备能力和图像格式",
        "number": 12,
        "href": "/docs/course/capture/video-format",
        "shortTitle": "查询采集格式",
        "stage": "capture"
      },
      {
        "title": "用 MMAP 获取并保存第一帧",
        "number": 13,
        "href": "/docs/course/capture/first-frame",
        "shortTitle": "采集第一帧图像",
        "stage": "capture"
      },
      {
        "title": "连续采集、超时与资源释放",
        "number": 14,
        "href": "/docs/course/capture/capture-loop",
        "shortTitle": "连续采集与释放",
        "stage": "capture"
      }
    ]
  },
  {
    "id": "encoding",
    "title": "实现 H.264 硬件编码",
    "position": 6,
    "description": "把采集图像交给硬件编码器，保存视频并测量性能。",
    "lessons": [
      {
        "title": "把采集缓冲区交给硬件编码器",
        "number": 15,
        "href": "/docs/course/encoding/encoder-memory",
        "shortTitle": "准备编码缓冲区",
        "stage": "encoding"
      },
      {
        "title": "编写 HDMI 转 H.264 文件程序",
        "number": 16,
        "href": "/docs/course/encoding/h264-file",
        "shortTitle": "保存 H.264 视频",
        "stage": "encoding"
      },
      {
        "title": "测量帧率、码率与延迟",
        "number": 17,
        "href": "/docs/course/encoding/video-performance",
        "shortTitle": "测量视频性能",
        "stage": "encoding"
      }
    ]
  },
  {
    "id": "usb-hid",
    "title": "实现 USB 键盘鼠标",
    "position": 7,
    "description": "通过本地 C 程序发送键鼠报告，处理 USB 占用与重连。",
    "lessons": [
      {
        "title": "让 T527 枚举成 USB HID 设备",
        "number": 18,
        "href": "/docs/course/usb-hid/gadget-config",
        "shortTitle": "配置 USB Gadget",
        "stage": "usb-hid"
      },
      {
        "title": "编写 USB 键盘报告程序",
        "number": 19,
        "href": "/docs/course/usb-hid/keyboard-reports",
        "shortTitle": "编写键盘程序",
        "stage": "usb-hid"
      },
      {
        "title": "编写相对鼠标和绝对鼠标程序",
        "number": 20,
        "href": "/docs/course/usb-hid/mouse-reports",
        "shortTitle": "编写鼠标程序",
        "stage": "usb-hid"
      },
      {
        "title": "处理 USB 冲突、断线与状态",
        "number": 21,
        "href": "/docs/course/usb-hid/usb-recovery",
        "shortTitle": "处理 USB 异常",
        "stage": "usb-hid"
      }
    ]
  },
  {
    "id": "integration",
    "title": "接入现有网络服务",
    "position": 8,
    "description": "通过进程间接口接入 Go 服务，在浏览器中查看画面并操作设备。",
    "lessons": [
      {
        "title": "理解 Go 服务和网页的系统职责",
        "number": 22,
        "href": "/docs/course/integration/service-architecture",
        "shortTitle": "认识服务架构",
        "stage": "integration"
      },
      {
        "title": "通过视频 Socket 接入网页",
        "number": 23,
        "href": "/docs/course/integration/video-socket",
        "shortTitle": "接入视频 Socket",
        "stage": "integration"
      },
      {
        "title": "控制采集启停与远程操作",
        "number": 24,
        "href": "/docs/course/integration/capture-control",
        "shortTitle": "控制采集与远程操作",
        "stage": "integration"
      }
    ]
  },
  {
    "id": "deployment",
    "title": "部署与系统验收",
    "position": 9,
    "description": "完成开机启动、分层排障和可复现交付。",
    "lessons": [
      {
        "title": "组织运行文件并实现开机启动",
        "number": 25,
        "href": "/docs/course/deployment/startup",
        "shortTitle": "部署与开机启动",
        "stage": "deployment"
      },
      {
        "title": "按链路定位无画面与无键鼠",
        "number": 26,
        "href": "/docs/course/deployment/troubleshooting",
        "shortTitle": "分层定位故障",
        "stage": "deployment"
      },
      {
        "title": "完整验收与课程项目交付",
        "number": 27,
        "href": "/docs/course/deployment/acceptance",
        "shortTitle": "验收与交付",
        "stage": "deployment"
      }
    ]
  },
  {
    "id": "advanced",
    "title": "进阶专题",
    "description": "在基础功能之上扩展能力。",
    "lessons": [
      {
        "title": "修复并实现 HDMI 热插拔",
        "shortTitle": "HDMI 热插拔",
        "number": "A",
        "href": "/docs/course/advanced/hdmi-hotplug",
        "stage": "advanced"
      },
      {
        "title": "支持分辨率变化和动态重建",
        "shortTitle": "动态分辨率",
        "number": "B",
        "href": "/docs/course/advanced/dynamic-resolution",
        "stage": "advanced"
      },
      {
        "title": "降低延迟与资源占用",
        "shortTitle": "延迟与资源优化",
        "number": "C",
        "href": "/docs/course/advanced/low-latency",
        "stage": "advanced"
      },
      {
        "title": "方案功能与扩展接口",
        "shortTitle": "方案功能与扩展接口",
        "number": "D",
        "href": "/docs/course/advanced/optional-features",
        "stage": "advanced"
      }
    ]
  }
];
export const stageLabels = ['阶段一', '阶段二', '阶段三', '阶段四', '阶段五', '阶段六', '阶段七', '拓展学习'];
export const lessons = stages.flatMap(stage => stage.lessons);
export const shortStageNames = ["硬件准备", "驱动适配", "图像采集", "视频编码", "USB 键鼠", "服务集成", "部署验收", "进阶专题"];

