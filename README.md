# NexLink

NexLink 是一个基于 **ESP32-S3** 与 **ESP-IDF** 的调试器和串口桥接项目。它把 CMSIS-DAP 调试能力、USB CDC 虚拟串口与 ESP-NOW 无线链路组合在一起，使电脑既可以通过 USB 直接调试目标板，也可以通过一对 TX/RX 设备无线访问远端目标板。

它面向 Cortex-M 目标芯片的开发、下载、调试和串口日志查看。电脑侧工具把 NexLink 识别为普通的 CMSIS-DAP 调试器和 USB 串口；无论目标板在本地还是在远端，使用方式尽量保持一致。

> 项目当前的技术限制、已知风险和待改进事项统一记录在 [ISSUES.md](ISSUES.md)。

## 已实现功能

### CMSIS-DAP 调试器

- 支持 **SWD** 调试接口。
- 支持 **JTAG** 调试接口。
- 支持目标板 **nRESET / NRST** 硬件复位。
- 可供 Keil MDK、OpenOCD 等支持 CMSIS-DAP 的上位机工具使用。
- 有线和无线模式均使用同一套调试能力。

### USB CDC 虚拟串口

- 电脑侧枚举为标准 USB CDC 虚拟 COM 口。
- 支持电脑与目标板 UART 的双向数据透传。
- 有线模式：USB CDC 直接连接目标板 UART。
- 无线模式：USB CDC 数据经 TX、ESP-NOW、RX 后到达目标板 UART。
- 支持将主机设置的波特率、数据位、校验位和停止位同步给目标端 UART；具体支持范围见 [ISSUES.md](ISSUES.md)。

### 有线调试模式

在有线模式中，一块 NexLink 直接连接电脑和目标板：

```text
电脑 ── USB ── NexLink ── SWD / JTAG / NRST / UART ── 目标板
```

该模式适合桌面开发、首次烧录、硬件排查和不需要无线延长距离的场景。电脑会同时看到 CMSIS-DAP 调试接口和 CDC 串口接口。

### 无线调试模式

无线模式由两块设备配合工作：

```text
电脑 ── USB ── TX ))) ESP-NOW ((( RX ── SWD / JTAG / NRST / UART ── 目标板
```

- **TX（发送端 / 电脑端）**：连接电脑，通过 USB 提供 CMSIS-DAP 与 CDC 接口，并将请求转换为无线协议帧。
- **RX（接收端 / 目标端）**：连接目标板，实际执行 SWD、JTAG、复位和 UART 操作。
- TX 与 RX 之间通过 ESP-NOW 点对点通信。
- 无线 DAP 具备请求序号、超时等待、最多三次重传，以及 RX 端对重复请求的响应缓存。
- 无线 CDC 支持双向转发和基础发送窗口流控。

## 设备角色和运行模式

NexLink 的固件提供三个入口文件，分别服务于不同部署方式：

| 入口 | 角色 | 用途 |
|---|---|---|
| `main_rx.c` | RX | 可在本地有线模式和远端无线模式之间切换。 |
| `main_tx.c` | TX | 连接电脑，在开启时作为无线 DAP + CDC 的 USB 前端。 |
| `main_direct.c` | DIRECT | 纯有线 DAP + CDC，不初始化无线功能。 |

### RX 模式

| 模式 | LED 状态 | 行为 |
|---|---|---|
| 有线模式（默认） | 灭 | 本机直接向电脑提供 USB DAP + CDC，并连接目标板。 |
| 无线模式，等待连接 | 常亮 | RX 连接目标板，等待已配对 TX 建立 ESP-NOW 链路。 |
| 无线模式，已连接 | 1 Hz 闪烁 | 已与 TX 建立无线链路，可执行无线 DAP 和 CDC。 |

### TX 模式

| 模式 | LED 状态 | 行为 |
|---|---|---|
| 关闭（默认） | 灭 | 不启动 USB DAP/CDC 和无线业务。 |
| 开启，等待连接 | 常亮 | 向电脑提供 USB DAP + CDC，等待 RX。 |
| 开启，已连接 | 1 Hz 闪烁 | 已连接 RX，可进行无线 DAP 和 CDC。 |

## 按键与配对

当前硬件约定：按键使用 GPIO38，低电平表示按下。GPIO48 是状态 LED，GPIO9 和 GPIO46 分别是 CDC RX/TX 活动 LED；三盏 LED 均按高电平点亮设计。

- 短按：切换 RX 的有线/无线模式，或切换 TX 的关闭/开启模式。
- 长按约 2 秒：进入配对流程。
- 长按约 5 秒：清除已保存的配对信息。

配对后的对端 MAC、通信信道和 LMK 会保存到 NVS。设备重启后会恢复已保存的配对关系，并建立 ESP-NOW 加密链路。

## 软件架构

```text
USB CMSIS-DAP / USB CDC
          │
          ▼
      TX Frontend
          │
          ▼
    NexLink Protocol
          │
          ▼
      ESP-NOW Link
          │
          ▼
   RX DAP / CDC Bridge
          │
          ▼
SWD / JTAG / NRST / UART
```

主要模块如下：

| 目录或模块 | 职责 |
|---|---|
| `components/BSP/` | LED、按键和调试 GPIO 等板级驱动。 |
| `components/Middlewares/debug_probe/` | CMSIS-DAP 命令处理与调试接口执行。 |
| `components/Middlewares/usb_device/` | USB 复合设备、CDC、CMSIS-DAP v1 HID 与 v2 Bulk。 |
| `components/Middlewares/transport_uart/` | 目标板 UART 收发。 |
| `components/Middlewares/nexlink_protocol/` | TX 与 RX 之间的 DAP、CDC、流控协议帧。 |
| `components/Middlewares/espnow_transport/` | ESP-NOW 初始化、Peer 管理和收发队列。 |
| `components/Middlewares/nexlink_link/` | 心跳、连接状态、超时检测与业务载荷转发。 |
| `components/Middlewares/nexlink_dap_bridge/` | 无线 DAP 请求/响应与无线 CDC 桥接。 |
| `components/Middlewares/nexlink_pairing/` | 配对请求、记录保存和链路密钥配置。 |
| `components/Middlewares/nexlink_mode/` | RX/TX 模式持久化和切换后的重启。 |

## 双核任务分工

ESP32-S3 的两个 CPU 核按实时性划分职责：

| CPU 核 | 主要任务 |
|---|---|
| Core 0 | Wi-Fi、ESP-NOW、USB 前端、无线链路与配对。 |
| Core 1 | CMSIS-DAP 执行、SWD/JTAG 时序、目标 UART 和 RX 桥接。 |

这样可减少 Wi-Fi/USB 活动对目标调试 GPIO 时序的直接影响。

## 硬件连接概要

以下 GPIO 分配用于适配项目配套的 NexLink ESP32-S3 开发板，因此替换了初版基于通用开发板的引脚配置。
开发板的原理图、PCB 等硬件资料见[立创开源硬件项目](https://oshwhub.com/miraitowa-la/project_iatvaxyy)。

当前 RX / DIRECT 的默认目标接口引脚如下：

| 功能 | ESP32-S3 GPIO |
|---|---:|
| JTAG TDI | GPIO10 |
| JTAG TDO / SWO | GPIO12 |
| TCK / SWCLK | GPIO17 |
| TMS / SWDIO | GPIO18 |
| nRESET / NRST | GPIO13 |
| 目标 UART TX | GPIO47 |
| 目标 UART RX | GPIO45 |
| 按键 | GPIO38 |
| 状态 LED | GPIO48 |
| CDC RX 活动 LED | GPIO9 |
| CDC TX 活动 LED | GPIO46 |

目标板必须与 NexLink 共地。当前调试 GPIO 使用 3.3 V 逻辑电平，连接前应确认目标板电平兼容性。

GPIO45 是 ESP32-S3 的 strapping 引脚，参与 VDD_SPI（片内 Flash/PSRAM 供电）电压选择：芯片复位释放时若采样为高，VDD_SPI 输出 1.8 V；采样为低则输出 3.3 V。该引脚复位默认带内部弱下拉，因此无外部干预时采样为低、VDD_SPI 为 3.3 V。用作目标 UART RX 时，请勿在该网络上加外部上拉，否则可能把 VDD_SPI 切到 1.8 V。若模块已通过 `EFUSE_VDD_SPI_FORCE` 固定 VDD_SPI 电压（常见于片内 Flash 模组），则该 strapping 不再影响电压。复位释放后 GPIO45 作为普通 GPIO 使用。

GPIO47 复位时电平下降较慢，复位期间可能出现约 60 μs 的低电平毛刺，用作 UART TX 时应在目标端确认不会误触发。

### CDC 活动指示灯

GPIO9 和 GPIO46 只反映 CDC 串口数据活动，不反映 CMSIS-DAP 调试、无线心跳、配对、CDC 参数设置或流控报文。

| 指示灯 | 含义 | 点亮时机 |
|---|---|---|
| CDC RX（GPIO9） | 目标板到主机方向 | CDC 数据成功进入 USB 或 ESP-NOW 的下一层时，亮约 75 ms。 |
| CDC TX（GPIO46） | 主机到目标板方向 | CDC 数据成功进入目标 UART 或 ESP-NOW 的下一层时，亮约 75 ms。 |

连续 CDC 数据会延长亮灯时间，视觉上表现为持续点亮。GPIO48 的模式、配对和无线链路状态指示逻辑保持不变。

## 构建环境

- 芯片：ESP32-S3
- SDK：ESP-IDF v5.5.4
- 构建系统：CMake / `idf.py`

在已配置 ESP-IDF 环境的终端内，可在项目目录运行：

```powershell
idf.py build
```

构建前需在 `main/CMakeLists.txt` 中选择要编译的入口角色。当前角色选择方式及其改进计划见 [ISSUES.md](ISSUES.md)。

## 项目状态

当前版本为 **v0.1.0**，定位为“基础功能可用的开发阶段版本”。

- 有线 DAP：已验证可用。
- 有线 CDC：已验证可用。
- 无线 DAP：已验证可用。
- 无线 CDC：已验证可用。

详细版本演进请参见 [CHANGELOG.md](CHANGELOG.md)。

## 许可证

本项目使用 [Apache License 2.0](LICENSE) 开源。
