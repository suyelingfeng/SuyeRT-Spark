# STM32F407 RT-Spark RT-Thread 移植说明

详细的软件分层、线程通信、采集流程、UI 导航和移植逻辑框图见
[`docs/软件架构与逻辑框图.md`](docs/软件架构与逻辑框图.md)。

## 当前配置

- MCU：STM32F407ZGT6，Cortex-M4F，Flash 1 MiB，片内 SRAM 128 KiB
- RTOS：RT-Thread v5.2.2（官方稳定版本）
- 主频：外部 8 MHz 晶振，经 PLL 倍频到 168 MHz
- 系统节拍：1 kHz
- 调试串口：USART1，PA9/PA10，115200-8-N-1，连接板载 ST-Link VCP
- 运行指示：红色 LED PF12，每 500 ms 翻转一次，低电平点亮
- 图形界面：LVGL 9.5.0，ST7789V3 240 × 240，四方向键导航
- 板载服务：AHT21、AP3216C、ICM20608、温湿度 Kalman、四元数姿态、W25Q、SD detect、RW007 状态（Wi-Fi 开机保持复位以避免 LCD 频闪）

原理图中的主晶振是 8 MHz。工程最初使用了 25 MHz 的 HSE 参数，现已同时在
`STM32F407_RTT.ioc`、HAL 配置和 `SystemClock_Config()` 中修正。

## 工程目录

```text
Applications/runtime/                 所有线程注册、启动和线程间通信
Applications/lvgl_ui/                 页面、导航和 UI 业务
BSP/services/                         快慢采样与驱动/算法编排
BSP/algorithms/                       Kalman、Mahony、四元数姿态算法
BSP/drivers/                          总线、传感器、存储、RW007 独立驱动
BSP/lcd/                              ST7789/FSMC 显示驱动
Core/                                 STM32CubeMX 生成的初始化代码
Drivers/                              STM32 HAL/CMSIS
Middlewares/Third_Party/RT-Thread/    RT-Thread v5.2.2 内核与 Cortex-M4 移植层
```

应用代码建议放在 `Applications`，板级适配放在 `BSP`。时钟、GPIO、USART 等
STM32 外设配置继续由 `STM32F407_RTT.ioc` 和 STM32CubeMX 管理。

## 编译

需要 ARM GNU Toolchain、CMake 和 Ninja。在工程根目录执行：

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

使用预设时编译结果位于 `build/Debug`。本工程提供的一键脚本使用 STM32CubeCLT，结果位于
`build/codex-gcc`，包括 `.elf`、`.hex` 和 `.bin` 文件：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build.ps1
```

## 烧录与验证

使用板载 ST-Link 将 HEX/ELF 烧录到内部 Flash，起始地址为 `0x08000000`。
打开 ST-Link 虚拟串口并设置为 115200-8-N-1。复位后应看到 RT-Thread v5.2.2
启动信息、`suye>` 提示符和六功能 UI，同时红色 LED 周期闪烁。可执行：

```text
sensor_status
environment_status
attitude_status
attitude_zero
storage_status
network_status
ui_status
```

实机 SPI Flash JEDEC 为 `EF 40 17`，即 W25Q64 8 MiB；与原理图 W25Q128 标注不同。
完整 DFS/FAL 文件挂载和 WLAN/lwIP 尚未加入当前精简内核。

## 重新生成 CubeMX 代码

`.ioc` 已保存 8 MHz HSE、168 MHz SYSCLK、USART1 和两路 LED 配置。重新生成后，
请保留 `main.c` 与 `stm32f4xx_it.c` 的 `USER CODE` 区域。顶层 `CMakeLists.txt`
是用户维护文件，RT-Thread 源码清单和包含路径都放在这里，不应由 CubeMX 覆盖。

RT-Thread 使用自己的 PendSV 和 HardFault 处理函数。`stm32f4xx_it.c` 的用户代码区
会将 CubeMX 生成的同名函数重命名，避免与 RT-Thread 上下文切换代码冲突。
