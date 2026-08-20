# SuyeRT-Spark

基于 STM32F407、RT-Thread 和 LVGL 的 RT-Spark 开发框架。

工程已集成 RT-Thread 5.2.2、LVGL 9.5.0、ST7789V3 FSMC 显示、四方向键菜单、
USART1 115200 启动日志、官方 FinSH/MSH 交互命令行，以及“夙夜凌风”精简中文字库。

当前 UI 已包含 RT-Thread/“夙夜凌风”开机动画，以及 Sensors、Temp/RH、Attitude、
Storage、Network、System、LED Rings、GPIO Pins 八个实时功能页面。上/下选择，右键进入或执行页面操作，左键返回。

已完成的板载功能：

- AHT21 温湿度、AP3216C 光照/接近、ICM20608 六轴实时采集。
- AHT21 温湿度独立一维 Kalman 页面，可同时比较原始值和滤波值。
- ICM20608 50 Hz 四元数姿态融合：开机静止零偏校准、开机方向归零、
  Mahony 重力校正、静止零速偏置更新以及欧拉角 Kalman 平滑。
- SPI Flash JEDEC/容量识别和 SD 卡插卡检测；实物 Flash 为 W25Q64 8 MiB。
- RW007 官方复位时序、RST、INT/BUSY、SPI2 与模块 READY 状态诊断。
- RW007 开机保持硬件复位，避免其启动电流扰动 LCD 共用的 3.3 V 电源；仅在网络页执行 RESET 后启动。
- RT-Thread uptime、线程数、堆内存和 CPU 主频实时显示。
- 19 颗 SK6805 按中心、内环、外环独立点亮，并提供低亮度限流策略。
- GPIOA～GPIOI 的输入电平、输出锁存值和引脚模式实时诊断。
- 提供完整的串口诊断命令：

```text
sensor_status        environment_status   attitude_status
attitude_zero        storage_status       network_status
network_reset        led_ring_status      led_ring_next
gpio_status
```

说明：当前精简 RT-Thread 工程未包含 DFS/FAL、WLAN 管理层和 lwIP，因此尚不提供
文件挂载及 Wi-Fi 扫描/连接/IP。板级服务接口已经独立，后续加入官方组件时无需重写 UI。

第一次使用请从 [二次开发与学习指南](docs/二次开发与学习指南.md) 开始。指南包含：

- 工程目录和各层职责
- STM32CubeMX 修改时钟
- 构建、ST-LINK 烧录与虚拟串口参数
- help、ps、free、ui_status、lcd_test 等串口命令
- 手写页面与图形化 UI 导出接入
- 换屏、换板、换 RTOS 的移植边界
- 黑屏、HardFault、按键、内存和性能调试
