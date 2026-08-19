# RT-Spark LVGL 图形化设计工程

本目录是面向 **LVGL Pro Editor / LVGL 9.5.0** 的可继续编辑工程，目标屏幕为板载 **ST7789V3 240 x 240、RGB565**。它对应固件中的开机动画、主页和六个功能页。

## 目录结构

```text
ui_designer/
├─ project.xml                 # 编辑器入口与 240x240 目标配置
├─ globals.xml                 # 主题颜色、公共样式、预览 subject
├─ components/                # 可复用页头、菜单卡、状态行、操作按钮
├─ screens/                   # boot/home 及六个功能页
├─ images/                    # 后续位图资源
├─ fonts/                     # 后续子集字体
└─ validate.ps1               # XML 完整性快速检查
```

## 在 LVGL Editor 中使用

1. 使用支持 LVGL 9.5 的 LVGL Pro Editor 打开本目录的 `project.xml`。
2. 目标选择 `rt_spark_240x240`。
3. 修改 `screens/` 中的界面，公共颜色与样式尽量只在 `globals.xml` 中改。
4. 导出 C 代码后，先保留生成目录，不要直接覆盖 `Applications/lvgl_ui/` 中已经过实机验证的代码。
5. 将导出代码作为 `generated/` 层，把按键、数据刷新、页面动作放在 `custom/` 层；再次导出时不会覆盖业务逻辑。

首次打开后如果编辑器提示升级项目格式，先复制整个 `ui_designer` 目录再确认升级。升级只影响设计工程，不应修改 BSP 或驱动。

## 页面与实体按键

| 页面 | 作用 | RIGHT（确认） | LEFT（返回） |
|---|---|---|---|
| Home | 六功能入口 | 打开聚焦的页面 | 无动作 |
| Sensors | AHT21/AP3216C/ICM20608 实时值 | REFRESH | Home |
| Temp/RH | 温湿度原始值和一维 Kalman 输出 | REFRESH | Home |
| Attitude | 四元数、相对角度、零漂状态 | SET ZERO | Home |
| Storage | W25Q 与 SD 卡状态 | RESCAN | Home |
| Network | RW007 硬件状态 | RESET RW007 | Home |

固件启动时 RW007 保持硬件复位，以避免 Wi-Fi 模块的电流负载扰动 LCD 共用的 3.3 V
电源。Network 页的 RESET 是显式启动动作，不应在开机流程中自动触发。
| System | RTOS、内存、线程、串口信息 | 无破坏性动作 | Home |

UP/DOWN 用于移动 LVGL group 焦点。所有可聚焦组件都避免使用 `transform_scale`；这一约束来自目标板上对“确认/返回后异常”的实机排查结果。

## 预览 subject 与固件数据映射

`globals.xml` 的 subject 让设计器能显示样例数据。以后采用编辑器生成代码时，按下表通过 `Applications/runtime/app_tasks.c` 的快照接口更新它们：

| Subject | 固件来源 |
|---|---|
| `temperature_text`, `humidity_text` | `app_tasks_get_board_snapshot()` 的 AHT21 字段 |
| `temperature_kalman_text`, `humidity_kalman_text` | 温湿度一维 Kalman 输出字段 |
| `light_text`, `proximity_text` | 同一快照的 AP3216C 字段 |
| `acceleration_text`, `gyroscope_text` | 同一快照的 ICM20608 字段 |
| `roll_text`, `pitch_text`, `yaw_text`, `quaternion_text` | Mahony 四元数融合和角度 Kalman 字段 |
| `flash_model_text`, `flash_capacity_text`, `sd_state_text` | 同一快照的 Flash/SD 字段 |
| `rw007_state_text`, `rw007_busy_text`, `network_ip_text` | 同一快照的 RW007 字段；IP 在接入 WLAN 协议栈后更新 |
| `uptime_text`, `thread_count_text`, `heap_text` | RT-Thread 运行时统计 |

UI 线程只能消费快照并更新 LVGL 对象；耗时的 I2C/SPI/网络操作应留在服务层或工作线程中，不能放进按键回调。这个边界既避免 UI 卡死，也方便以后把同一套界面移植到其他 STM32 BSP。

操作按钮统一写入 `action_request`：`1` 表示传感器刷新、`2` 表示存储重扫、`3` 表示 RW007 复位、`4` 表示将当前姿态设为正方向。固件的 custom 层收到后调用对应的 `app_tasks_request_*()`，随即将 subject 清零。主页卡片使用 LVGL 官方 `screen_create_event` 创建目标页；实体 LEFT 返回仍由 custom 按键层统一处理。

## 导出后建议结构

```text
Applications/lvgl_ui/
├─ generated/     # LVGL Editor 生成，允许整目录重新生成
├─ custom/        # 页面事件、subject 更新、按键 group，禁止生成器覆盖
├─ app_ui.c       # UI 生命周期入口
└─ app_ui.h
```

当前固件还使用手写的低内存界面实现，设计工程不会自动参与编译。等界面在编辑器中确认后再切换生成层，可以避免一次改动同时影响显示、按键和外设。

## 校验

在工程根目录执行：

```powershell
./ui_designer/validate.ps1
```

该脚本检查必需文件是否存在以及 XML 是否完整。它不能替代 LVGL Editor 的语义检查；导出后仍需重新编译、烧录并走一遍六页面按键测试。

更完整的烧录、调试和二次开发流程见 [`../docs/二次开发与学习指南.md`](../docs/二次开发与学习指南.md)。
