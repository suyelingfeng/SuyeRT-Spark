# SPI 驱动集成快速指南

## 📌 快速开始

### 1. 对于现有设备驱动的迁移（以 W25Q 为例）

#### 原来的方式（旧）
```c
// w25q.c
#include "spi2_board.h"

void w25q_read(uint32_t addr, uint8_t *data, int len) {
    spi2_cs_low();
    spi2_transfer_byte(0x03);  // 读命令
    // ... 直接调用 spi2 函数
    spi2_cs_high();
}
```

#### 新的方式（推荐）
```c
// w25q.c
#include "spi_driver.h"

typedef struct {
    spi_device_t *spi_dev;  // 添加 SPI 设备指针
} w25q_t;

int w25q_init(w25q_t *flash) {
    // 创建 SPI 设备配置
    static spi_bus_config_t config = {
        .device_name = "w25q64",
        .cs_pin_group = GPIOB,
        .cs_pin = 9,
        .speed_hz = 20000000,
        .mode = SPI_MODE_0
    };
    
    // 从驱动框架获取 SPI 总线
    flash->spi_dev = spi_driver_open_device("spi2", &config);
    return flash->spi_dev ? 0 : -1;
}

void w25q_read(w25q_t *flash, uint32_t addr, uint8_t *data, int len) {
    uint8_t cmd[] = {0x03, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF};
    
    // 使用统一接口
    spi_device_transfer(flash->spi_dev, cmd, NULL, 4);
    spi_device_read(flash->spi_dev, data, len);
}
```

### 2. 向现有项目添加新的 SPI 设备

假设要添加一个新的 SPI 温度传感器 MAX31855：

```c
// max31855.h
#ifndef __MAX31855_H__
#define __MAX31855_H__

#include "spi_driver.h"

typedef struct {
    spi_device_t *spi_dev;
    float temperature;
} max31855_t;

int max31855_init(max31855_t *sensor, const char *bus_name);
int max31855_read(max31855_t *sensor);

#endif
```

```c
// max31855.c
#include "max31855.h"

int max31855_init(max31855_t *sensor, const char *bus_name) {
    static spi_bus_config_t config = {
        .device_name = "max31855",
        .cs_pin_group = GPIOB,
        .cs_pin = 10,           // 使用不同的片选脚
        .speed_hz = 5000000,    // 5 MHz
        .mode = SPI_MODE_0
    };
    
    sensor->spi_dev = spi_driver_open_device(bus_name, &config);
    if (!sensor->spi_dev) {
        rt_kprintf("[MAX31855] Failed to open SPI device\n");
        return -1;
    }
    
    return 0;
}

int max31855_read(max31855_t *sensor) {
    uint8_t rx_data[4];
    int ret = spi_device_read(sensor->spi_dev, rx_data, 4);
    
    if (ret < 0) return ret;
    
    // 解析温度数据（31 位到 18 位）
    uint16_t raw_temp = ((rx_data[0] << 8) | rx_data[1]) >> 2;
    sensor->temperature = raw_temp * 0.25f;
    
    return 0;
}
```

### 3. 配置片选脚映射

编辑 `spi_config.h` 添加新设备的片选脚：

```c
// spi_config.h

// SPI2 设备片选脚映射
#define SPI2_CS_W25Q_PORT       GPIOB
#define SPI2_CS_W25Q_PIN        9

#define SPI2_CS_ICM20608_PORT   GPIOB
#define SPI2_CS_ICM20608_PIN    7

#define SPI2_CS_MAX31855_PORT   GPIOB      // 新增
#define SPI2_CS_MAX31855_PIN    10         // 新增
```

然后在 `spi2_adapter.c` 中的 GPIO 初始化函数中添加：

```c
static int spi2_hw_init(void) {
    // ... 现有代码 ...
    
    // 初始化所有片选脚为输出
    gpio_init_output(GPIOB, 9);   // W25Q
    gpio_init_output(GPIOB, 7);   // ICM20608
    gpio_init_output(GPIOB, 10);  // MAX31855 (新增)
    
    // 默认片选为高电平（不选中）
    gpio_set(GPIOB, 9);
    gpio_set(GPIOB, 7);
    gpio_set(GPIOB, 10);          // 新增
    
    return 0;
}
```

### 4. 在 board_service.c 中初始化新设备

```c
// board_service.c

#include "max31855.h"

typedef struct {
    // ... 现有设备 ...
    max31855_t max31855;  // 新增
} board_service_snapshot_t;

static board_service_snapshot_t *snapshot;

void board_service_init(board_service_snapshot_t *snap) {
    snapshot = snap;
    
    // ... 现有初始化代码 ...
    
    // 初始化 SPI 驱动
    spi_driver_init();
    spi2_adapter_init();
    
    // 初始化新的温度传感器
    if (max31855_init(&snapshot->max31855, "spi2") == 0) {
        rt_kprintf("[BSP] MAX31855 initialized successfully\n");
    } else {
        rt_kprintf("[BSP] MAX31855 initialization failed\n");
    }
}

void board_service_process(board_service_snapshot_t *snapshot, ...) {
    // ... 现有代码 ...
    
    // 定期读取温度
    if (now - last_sensor_tick > 500) {
        max31855_read(&snapshot->max31855);
        rt_kprintf("[SENSOR] Temp: %.2f C\n", snapshot->max31855.temperature);
        last_sensor_tick = now;
    }
}
```

---

## 🔌 常见配置参数

### SPI 模式（mode）
| 模式 | CPOL | CPHA | 常用设备 |
|------|------|------|---------|
| MODE_0 | 0 | 0 | W25Q, ICM20608, MAX31855 |
| MODE_1 | 0 | 1 | - |
| MODE_2 | 1 | 0 | - |
| MODE_3 | 1 | 1 | 部分传感器 |

### 推荐速率
| 设备 | 推荐速率 | 最大速率 |
|------|---------|---------|
| W25Q | 20 MHz | 50 MHz |
| ICM20608 | 5 MHz | 10 MHz |
| MAX31855 | 5 MHz | 5 MHz |

### 分频系数计算
STM32F407 APB1 时钟通常为 42 MHz

```
SPI_CR1_BR_DIV2   = 21 MHz   (SPIxCLK = APB1CLK/2)
SPI_CR1_BR_DIV4   = 10.5 MHz (SPIxCLK = APB1CLK/4)
SPI_CR1_BR_DIV8   = 5.25 MHz (SPIxCLK = APB1CLK/8)
SPI_CR1_BR_DIV16  = 2.6 MHz  (SPIxCLK = APB1CLK/16)
SPI_CR1_BR_DIV32  = 1.3 MHz  (SPIxCLK = APB1CLK/32)
```

---

## 🐛 调试技巧

### 启用 SPI 驱动调试日志
在 `spi_config.h` 中：
```c
#define SPI_DEBUG_ENABLED 1
```

### 常见问题排查

**问题 1**: 读取的数据全是 0xFF 或 0x00
- 检查片选脚是否正确连接
- 验证 GPIO 初始化是否完成
- 使用逻辑分析仪查看实际的 SPI 信号

**问题 2**: 设备初始化失败
```c
// 添加调试输出
spi_device_t *dev = spi_driver_open_device("spi2", &config);
if (!dev) {
    rt_kprintf("[DEBUG] Failed to open device\n");
    rt_kprintf("[DEBUG] Bus name: %s\n", config.device_name);
    rt_kprintf("[DEBUG] CS pin: %d\n", config.cs_pin);
}
```

**问题 3**: 数据传输超时
- 检查 SPI 时钟是否启用
- 验证 GPIO 速度设置（应为 HIGH）
- 确认 SPI 模式（CPOL/CPHA）与设备匹配

---

## 📚 API 参考

### 设备开启
```c
spi_device_t *spi_driver_open_device(const char *bus_name,
                                     spi_bus_config_t *config);
```

### 单字节传输
```c
uint8_t spi2_transfer_byte(uint8_t tx_byte);  // 返回收到的字节
```

### 块传输
```c
int spi_device_write(spi_device_t *dev, const uint8_t *data, int len);
int spi_device_read(spi_device_t *dev, uint8_t *data, int len);
int spi_device_transfer(spi_device_t *dev, const uint8_t *tx,
                        uint8_t *rx, int len);
```

### 片选控制（低级 API）
```c
void spi2_cs_low(const char *device_name);
void spi2_cs_high(const char *device_name);
```

---

## 🔄 完整集成示例：添加 ICM20608 IMU 传感器

假设要将原有的 ICM20608 驱动迁移到新的 SPI 框架：

### 步骤 1：修改 ICM20608 头文件

```c
// icm20608.h - 旧的方式
typedef struct {
    uint16_t accel_x, accel_y, accel_z;
    uint16_t gyro_x, gyro_y, gyro_z;
    float temperature;
} icm20608_data_t;

void icm20608_init(void);
void icm20608_read(icm20608_data_t *data);
```

```c
// icm20608.h - 新的方式
#include "spi_driver.h"

typedef struct {
    spi_device_t *spi_dev;              // SPI 设备句柄
    uint16_t accel_x, accel_y, accel_z;
    uint16_t gyro_x, gyro_y, gyro_z;
    float temperature;
    rt_mutex_t lock;                    // 线程安全锁
} icm20608_t;

int icm20608_init(icm20608_t *imu);
int icm20608_read(icm20608_t *imu);
int icm20608_read_register(icm20608_t *imu, uint8_t reg, uint8_t *value);
int icm20608_write_register(icm20608_t *imu, uint8_t reg, uint8_t value);
```

### 步骤 2：修改 ICM20608 实现

```c
// icm20608.c - 关键函数
#include "icm20608.h"

#define ICM20608_ADDR       0x68
#define ICM20608_WHO_AM_I   0x00

int icm20608_init(icm20608_t *imu) {
    // 创建 SPI 配置
    static spi_bus_config_t config = {
        .device_name = "icm20608",
        .cs_pin_group = GPIOB,
        .cs_pin = 7,
        .speed_hz = 5000000,        // 5 MHz
        .mode = SPI_MODE_0
    };
    
    // 打开 SPI 设备
    imu->spi_dev = spi_driver_open_device("spi2", &config);
    if (!imu->spi_dev) {
        rt_kprintf("[ICM20608] Failed to open SPI device\n");
        return -1;
    }
    
    // 创建线程安全锁
    imu->lock = rt_mutex_create("icm20608_lock", RT_IPC_FLAG_FIFO);
    if (!imu->lock) {
        rt_kprintf("[ICM20608] Failed to create mutex\n");
        return -1;
    }
    
    // 验证设备存在
    uint8_t who_am_i;
    if (icm20608_read_register(imu, ICM20608_WHO_AM_I, &who_am_i) != 0) {
        rt_kprintf("[ICM20608] Failed to read WHO_AM_I\n");
        return -1;
    }
    
    if (who_am_i != 0xAF) {  // ICM20608 的 WHO_AM_I 值
        rt_kprintf("[ICM20608] Invalid WHO_AM_I: 0x%02X\n", who_am_i);
        return -1;
    }
    
    rt_kprintf("[ICM20608] Initialized successfully (WHO_AM_I: 0x%02X)\n", who_am_i);
    return 0;
}

int icm20608_read_register(icm20608_t *imu, uint8_t reg, uint8_t *value) {
    uint8_t tx[2] = {reg | 0x80, 0x00};  // 0x80 = read flag
    uint8_t rx[2] = {0, 0};
    
    rt_mutex_take(imu->lock, RT_WAITING_FOREVER);
    int ret = spi_device_transfer(imu->spi_dev, tx, rx, 2);
    rt_mutex_release(imu->lock);
    
    if (ret == 0) {
        *value = rx[1];
    }
    return ret;
}

int icm20608_write_register(icm20608_t *imu, uint8_t reg, uint8_t value) {
    uint8_t tx[2] = {reg & 0x7F, value};  // 清除读写位（写）
    
    rt_mutex_take(imu->lock, RT_WAITING_FOREVER);
    int ret = spi_device_transfer(imu->spi_dev, tx, NULL, 2);
    rt_mutex_release(imu->lock);
    
    return ret;
}

int icm20608_read(icm20608_t *imu) {
    uint8_t tx[7] = {0x3B | 0x80, 0, 0, 0, 0, 0, 0};  // 加速度 + 温度
    uint8_t rx[7] = {0};
    
    rt_mutex_take(imu->lock, RT_WAITING_FOREVER);
    int ret = spi_device_transfer(imu->spi_dev, tx, rx, 7);
    rt_mutex_release(imu->lock);
    
    if (ret == 0) {
        // 解析加速度数据（16 位有符号）
        imu->accel_x = ((rx[1] << 8) | rx[2]);
        imu->accel_y = ((rx[3] << 8) | rx[4]);
        imu->accel_z = ((rx[5] << 8) | rx[6]);
        
        // 温度计算：TEMP_OUT / 326.8 + 25
        imu->temperature = (float)imu->accel_x / 326.8f + 25.0f;
    }
    
    return ret;
}
```

### 步骤 3：在 board_service.c 中集成

```c
// board_service.c
#include "icm20608.h"

typedef struct {
    w25q_t w25q;              // 闪存
    icm20608_t icm20608;      // IMU 传感器（新增）
} board_service_snapshot_t;

void board_service_init(board_service_snapshot_t *snap) {
    rt_kprintf("[BSP] Initializing board services\n");
    
    // 初始化 SPI 驱动框架
    if (spi_driver_init() != 0) {
        rt_kprintf("[BSP] Failed to initialize SPI driver\n");
        return;
    }
    
    // 初始化 SPI2 硬件适配层
    if (spi2_adapter_init() != 0) {
        rt_kprintf("[BSP] Failed to initialize SPI2 adapter\n");
        return;
    }
    
    // 初始化 W25Q 闪存
    if (w25q_init(&snap->w25q) != 0) {
        rt_kprintf("[BSP] Failed to initialize W25Q\n");
    } else {
        rt_kprintf("[BSP] W25Q initialized successfully\n");
    }
    
    // 初始化 ICM20608 IMU（新增）
    if (icm20608_init(&snap->icm20608) != 0) {
        rt_kprintf("[BSP] Failed to initialize ICM20608\n");
    } else {
        rt_kprintf("[BSP] ICM20608 initialized successfully\n");
    }
}

void board_service_process(board_service_snapshot_t *snap, uint32_t now) {
    static uint32_t last_sensor_tick = 0;
    
    // 每 100ms 读取一次 IMU 数据
    if (now - last_sensor_tick > 100) {
        if (icm20608_read(&snap->icm20608) == 0) {
            rt_kprintf("[SENSOR] Accel: X=%d Y=%d Z=%d Temp=%.2f\n",
                      snap->icm20608.accel_x,
                      snap->icm20608.accel_y,
                      snap->icm20608.accel_z,
                      snap->icm20608.temperature);
        }
        last_sensor_tick = now;
    }
}
```

---

## 🧵 多线程安全用法

当多个线程访问同一个 SPI 设备时，使用互斥锁保护：

```c
// 线程 1：定期读取温度
void temperature_thread(void *param) {
    icm20608_t *imu = (icm20608_t *)param;
    
    while (1) {
        if (icm20608_read(imu) == 0) {
            rt_kprintf("Temperature: %.2f\n", imu->temperature);
        }
        rt_thread_mdelay(1000);
    }
}

// 线程 2：处理命令
void command_thread(void *param) {
    icm20608_t *imu = (icm20608_t *)param;
    
    while (1) {
        // 修改配置
        icm20608_write_register(imu, 0x1A, 0x00);  // 配置寄存器
        rt_thread_mdelay(5000);
    }
}

// 主程序
void main(void) {
    icm20608_t imu;
    icm20608_init(&imu);
    
    rt_thread_t t1 = rt_thread_create("temp_read", temperature_thread, &imu, 512, 20, 10);
    rt_thread_t t2 = rt_thread_create("cmd", command_thread, &imu, 512, 21, 10);
    
    rt_thread_startup(t1);
    rt_thread_startup(t2);
}
```

---

## 🔧 调试和监控

### 添加 SPI 统计信息

```c
// spi_driver.h 中添加
typedef struct {
    uint32_t total_transfers;
    uint32_t total_bytes;
    uint32_t error_count;
    uint32_t timeout_count;
} spi_stats_t;

spi_stats_t* spi_driver_get_stats(const char *bus_name);
void spi_driver_reset_stats(const char *bus_name);
```

### 实时监控

```c
void spi_monitor_thread(void *param) {
    while (1) {
        spi_stats_t *stats = spi_driver_get_stats("spi2");
        if (stats) {
            rt_kprintf("[SPI2 Stats] Transfers: %lu, Bytes: %lu, Errors: %lu\n",
                      stats->total_transfers,
                      stats->total_bytes,
                      stats->error_count);
        }
        rt_thread_mdelay(5000);
    }
}
```

---

## ✅ 验证清单（集成新设备时）

- [ ] 添加设备结构体定义（包含 spi_device_t 指针）
- [ ] 实现 device_init() 函数
- [ ] 实现低级的寄存器读写函数
- [ ] 在 spi_config.h 中配置片选脚
- [ ] 更新 spi2_adapter.c 的 GPIO 初始化
- [ ] 在 board_service.c 中调用初始化函数
- [ ] 编译无错误和警告
- [ ] 使用 rt_kprintf() 验证初始化消息
- [ ] 使用逻辑分析仪验证 SPI 信号
- [ ] 单线程功能测试通过
- [ ] 多线程并发测试通过
- [ ] 长时间压力测试（至少 24 小时）

---

## 📋 常见设备集成参数表

| 设备 | 速率 | 模式 | 特殊要求 |
|------|------|------|---------|
| W25Q64 | 20 MHz | MODE_0 | 支持 32MB 寻址 |
| ICM20608 | 5 MHz | MODE_0 | 寄存器访问 |
| MAX31855 | 5 MHz | MODE_0 | 只读 |
| LCD (SPI) | 10 MHz | MODE_0 | 需要 DMA |
| RF 模块 | 8 MHz | MODE_0 | 低延迟要求 |

---

**最后更新**: 2026-08-20  
**维护者**: SPI 驱动开发团队  
**相关文档**: 见 `SPI_OPTIMIZATION_SUMMARY.md`
