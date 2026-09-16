# 更新记录

本文件记录 NexLink 对外可见的功能演进。项目当前处于开发阶段，版本号遵循“主版本.次版本.修订版本”的形式：

- 主版本：出现不兼容架构或协议调整时更新。
- 次版本：增加兼容的新功能时更新。
- 修订版本：修复问题、优化或文档更新时更新。

## [Unreleased]

### 硬件引脚调整

- 重新分配 RX / DIRECT 的调试与串口引脚：JTAG TDI = GPIO10，JTAG TDO / SWO = GPIO12，TCK / SWCLK = GPIO17，TMS / SWDIO = GPIO18，nRESET = GPIO13，目标 UART TX = GPIO47，目标 UART RX = GPIO45。
- 按键改到 GPIO38，LED 改到 GPIO48。
- LED 改为高电平点亮；按键保持低电平表示按下。
- 目标 UART RX 使用 strapping 引脚 GPIO45（参与 VDD_SPI 电压选择），该网络不得加外部上拉；GPIO47 复位期间存在约 60 μs 低电平毛刺。

## [0.1.0] - 2026-09-03

### 首个基础可用版本

#### 有线功能

- 实现 USB 复合设备：CMSIS-DAP v2 Bulk、CMSIS-DAP v1 HID 与 USB CDC。
- 支持 SWD、JTAG 和 nRESET / NRST 硬件复位。
- 实现 USB CDC 与目标 UART 双向透传。
- 提供独立的 DIRECT 纯有线入口。

#### 无线功能

- 建立 TX（电脑端）与 RX（目标端）的 ESP-NOW 双设备架构。
- 实现无线 DAP 请求、响应和目标端执行。
- 实现无线 DAP 请求序号、超时、最多三次重传和重复请求响应缓存。
- 实现无线 CDC 双向转发、串口参数同步和基础信用窗口流控。
- 实现链路心跳、3 秒超时检测和 LED 连接状态指示。

#### 模式、配对与交互

- RX 支持有线模式与无线模式切换。
- TX 支持关闭模式与无线开启模式切换。
- 模式保存到 NVS，切换后自动重启并恢复。
- 支持按键短按切换模式、长按约 2 秒发起配对、长按约 5 秒清除配对信息。
- 实现 ESP-NOW 配对记录保存、LMK 保存和加密链路初始化。

#### 架构与工程

- 完成 USB 前端、无线协议、ESP-NOW 链路、RX 硬件后端的分层。
- 将 Wi-Fi/USB 前端任务与 DAP/目标 UART 任务分配至不同 CPU 核。
- 增加关键静态检查脚本，覆盖角色入口、模式、链路、配对、DAP 和 CDC 基础结构。

### 已知限制

详细问题和后续整改方向见 [ISSUES.md](ISSUES.md)。
