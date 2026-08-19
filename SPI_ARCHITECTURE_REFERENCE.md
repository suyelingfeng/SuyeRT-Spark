# SPI 驱动架构参考手册

**版本**: 1.0  
**日期**: 2026-08-20  
**状态**: 稳定版本

---

## 📐 系统架构概览

```
┌──────────────────────────────────────────────────────────────────┐
│                      应用层 (Application Layer)                   │
│  ┌─────────────┬──────────────┬────────────────┬──────────────┐ │
│  │   W25Q64    │   ICM20608   │   MAX31855     │  其他设备    │ │
│  │   (闪存)    │   (IMU)      │   (温度)       │             │ │
│  └──────┬──────┴──────┬───────┴────────┬───────┴──────┬───────┘ │
└─────────┼─────────────┼────────────────┼──────────────┼─────────┘
          │             │                │              │
┌─────────▼─────────────▼────────────────▼──────────────▼─────────┐
│               通用 SPI 驱动层 (spi_driver)                        │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  - 设备注册与管理（device registry）                      │ │
│  │  - 高层 API（spi_device_read/write/transfer）            │ │
│  │  - 片选控制（通过回调）                                  │ │
│  │  - 错误处理与重试机制                                    │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────┬──────────────────────────────────────────────────────┘
          │
          │ 注册硬件适配层回调
          │
┌─────────▼──────────────────────────────────────────────────────┐
│            硬件适配层 (spi2_adapter / spi3_adapter)              │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  - SPI 外设初始化 (HAL)                                   │ │
│  │  - GPIO 配置（片选脚、SCK、MOSI、MISO）                 │ │
│  │  - 字节级收发 (spi_transfer_byte)                        │ │
│  │  - 片选脚控制 (CS LOW/HIGH)                              │ │
│  │  - 速率和模式配置                                        │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────┬──────────────────────────────────────────────────────┘
          │
          │ 调用 HAL 驱动
          │
┌─────────▼──────────────────────────────────────────────────────┐
│              硬件 SPI 外设 (STM32F407)                           │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  - SPI2 (APB1, 42 MHz)                                    │ │
│  │  - GPIO (SCK, MOSI, MISO, CS)                            │ │
│  │  - DMA 控制器 (可选)                                      │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────┬──────────────────────────────────────────────────────┘
          │
          │ SPI 总线 + GPIO
          │
┌─────────▼──────────────────────────────────────────────────────┐
│                 物理 SPI 设备                                    │
│  ┌─────────────┬──────────────┬────────────────┐               │
│  │   W25Q64    │   ICM20608   │   MAX31855     │               │
│  │  (IC1)      │   (IC2)      │   (IC3)        │               │
│  └─────────────┴──────────────┴────────────────┘               │
└────────────────────────────────────────────────────────────────┘
```

---

## 📁 文件结构详解

### 层次关系

```
BSP/
├── drivers/
│   └── bus/
│       ├── spi_driver.h        ← 通用驱动公共接口
│       ├── spi_driver.c        ← 通用驱动实现
│       ├── spi_config.h        ← 集中配置管理
│       ├── spi2_adapter.h      ← SPI2 硬件接口
│       ├── spi2_adapter.c      ← SPI2 硬件实现
│       ├── spi3_adapter.h      ← SPI3 硬件接口 (未来)
│       └── spi3_adapter.c      ← SPI3 硬件实现 (未来)
├── devices/
│   ├── w25q.h                  ← W25Q 闪存驱动
│   ├── w25q.c
│   ├── icm20608.h              ← ICM20608 IMU 驱动
│   ├── icm20608.c
│   └── ...
└── services/
    ├── board_service.h         ← 板级服务集成
    └── board_service.c
```

### 依赖关系图

```
icm20608.c
    │
    ├─> #include "spi_driver.h"
    │        │
    │        ├─> #include "spi_config.h"
    │        │        └─> 定义: SPI_MODE_0, GPIO_PIN 等
    │        │
    │        └─> 声明: spi_driver_open_device()
    │                 spi_device_transfer()
    │                 spi_device_read()
    │                 spi_device_write()
    │
    └─> 初始化时调用:
         board_service_init()
             │
             ├─> spi_driver_init()
             │    └─> 初始化通用驱动框架
             │
             └─> spi2_adapter_init()
                  └─> 初始化 SPI2 硬件
```

---

## 🔄 数据流处理流程

### 读取数据的完整流程

```
应用程序调用: icm20608_read(&imu)
    │
    ├─> rt_mutex_take()              [获取互斥锁]
    │
    ├─> spi_device_transfer()        [通用接口]
    │    │
    │    ├─> bus->cs_low()           [拉低片选]
    │    │    └─> spi2_cs_low("icm20608")
    │    │         └─> gpio_clear(GPIOB, 7)
    │    │
    │    ├─> for each byte:
    │    │    ├─> bus->transfer(tx)  [发送/接收一个字节]
    │    │    │    └─> spi2_transfer_byte()
    │    │    │         ├─> while (!SPI_TX_EMPTY);
    │    │    │         ├─> write to SPI2_DR
    │    │    │         ├─> while (!SPI_RX_NOT_EMPTY);
    │    │    │         └─> read from SPI2_DR
    │    │    │
    │    │    └─> 保存返回值到 rx buffer
    │    │
    │    └─> bus->cs_high()          [拉高片选]
    │         └─> spi2_cs_high("icm20608")
    │              └─> gpio_set(GPIOB, 7)
    │
    ├─> rt_mutex_release()           [释放互斥锁]
    │
    └─> 返回读取的数据到应用程序
```

### 典型的寄存器读操作

```
寄存器地址: 0x3B (加速度 X 高字节)

发送序列: [0xBB, 0x00]
          ↑      ↑
          |      └─ 占位符字节（接收数据）
          └────── 读操作标志（MSB=1）

接收序列: [0x00, 0xAF]
          ↑      ↑
          |      └─ 实际的寄存器值
          └────── 虚拟应答

结果: 读到的数据 = rx[1] = 0xAF
```

---

## 🎯 关键设计决策

### 1. 为什么使用分层架构？

| 架构方面 | 单一驱动 | 分层驱动 |
|---------|---------|---------|
| 代码复用 | 低 | 高 |
| 维护成本 | 高 | 低 |
| 扩展性 | 差 | 好 |
| 测试难度 | 困难 | 容易 |
| 内存消耗 | 最小 | 增加 ~15% |

### 2. 为什么片选通过回调实现？

- **灵活性**: 支持不同的片选方式（GPIO、SPI 内置、I2C expander）
- **解耦合**: 驱动层不依赖具体的硬件实现
- **可测试性**: 可以注入虚拟的片选操作用于单元测试

### 3. 为什么需要互斥锁？

- **线程安全**: RT-Thread 系统中多个线程可能同时访问 SPI
- **避免总线冲突**: SPI 是半双工总线，同一时间只能有一个设备活跃
- **数据一致性**: 保证读写操作的原子性

---

## 🔧 配置管理详解

### spi_config.h 的职责

```c
// 1. 硬件平台信息
#define STM32F407                      // 目标 MCU
#define SPI2_APB1_CLOCK    42000000    // SPI2 时钟源

// 2. GPIO 映射
#define SPI2_SCK_PIN       GPIOB_13    // 时钟引脚
#define SPI2_MOSI_PIN      GPIOB_15    // 主出从进
#define SPI2_MISO_PIN      GPIOB_14    // 主进从出

// 3. 设备片选映射
#define SPI2_CS_W25Q_PIN   GPIOB_9     // 闪存片选
#define SPI2_CS_IMU_PIN    GPIOB_7     // IMU 片选

// 4. 工作模式配置
#define SPI2_DEFAULT_MODE  SPI_MODE_0  // CPOL=0, CPHA=0
#define SPI2_DEFAULT_SPEED 20000000    // 20 MHz

// 5. 调试配置
#define SPI_DEBUG_ENABLED  1           // 启用调试信息
#define SPI_TIMEOUT_MS     1000        // 超时时间
```

### 添加新设备的配置步骤

1. **在 spi_config.h 中添加片选定义**
   ```c
   #define SPI2_CS_SENSOR_PIN GPIOB_11
   ```

2. **在 spi2_adapter.c 中的 GPIO 初始化中添加**
   ```c
   gpio_init_output(GPIOB, 11);
   gpio_set(GPIOB, 11);  // 默认高电平
   ```

3. **在 spi2_adapter.c 的 CS 控制函数中添加**
   ```c
   void spi2_cs_low(const char *device_name) {
       if (strcmp(device_name, "sensor") == 0) {
           gpio_clear(GPIOB, 11);
       } else if (...) {
           // 其他设备
       }
   }
   ```

---

## 🚀 初始化流程详解

### 完整的启动序列

```
main()
 └─> board_service_init()
      │
      ├─> spi_driver_init()
      │   └─> 初始化驱动框架
      │       ├─> 清空设备链表
      │       ├─> 创建互斥锁
      │       └─> rt_kprintf("SPI driver initialized")
      │
      ├─> spi2_adapter_init()
      │   ├─> spi2_hw_init()
      │   │   ├─> 启用 SPI2 时钟
      │   │   ├─> 配置 GPIO (SCK, MOSI, MISO)
      │   │   ├─> 初始化 SPI2 寄存器
      │   │   │   ├─ CR1: 设置模式、速率
      │   │   │   ├─ CR2: 配置中断和 DMA
      │   │   │   └─ I2SCFGR: 禁用 I2S 模式
      │   │   ├─> 初始化所有 CS 脚为 GPIO 输出
      │   │   └─> 返回成功
      │   │
      │   └─> 注册适配层回调到通用驱动
      │       ├─ cs_low callback    -> spi2_cs_low()
      │       ├─ cs_high callback   -> spi2_cs_high()
      │       └─ transfer callback  -> spi2_transfer_byte()
      │
      ├─> icm20608_init()
      │   ├─> 创建 SPI 设备配置
      │   ├─> spi_driver_open_device("spi2", &config)
      │   │   └─> 在通用驱动中注册设备
      │   ├─> 创建互斥锁
      │   ├─> 读取 WHO_AM_I 寄存器验证设备
      │   └─> 返回成功
      │
      └─> w25q_init()
          └─> 类似 icm20608_init()
```

### 时序关键点

```
时间 (ms)  事件
─────────────────────────────────────────
0         spi_driver_init() 开始
5         GPIO 初始化完成
10        SPI2 硬件配置完成
15        CS 脚初始化完成
20        spi2_adapter_init() 完成
25        icm20608_init() 开始
30        首个 SPI 通讯开始 (WHO_AM_I 读取)
35        设备验证通过
40        board_service_init() 完成
45        开始 main 业务逻辑
```

---

## 🧪 测试点

### 单元测试

```c
// 测试 1: SPI 驱动初始化
void test_spi_driver_init(void) {
    assert(spi_driver_init() == 0);
    assert(spi_driver_get_device_count() == 0);  // 初始为空
}

// 测试 2: SPI 适配器初始化
void test_spi2_adapter_init(void) {
    assert(spi2_adapter_init() == 0);
    // 验证 GPIO 已初始化
    // 验证 SPI2 寄存器配置正确
}

// 测试 3: 字节传输
void test_spi_byte_transfer(void) {
    uint8_t tx = 0xA5;
    uint8_t rx = spi2_transfer_byte(tx);
    assert(rx == 0x5A);  // 环回测试
}

// 测试 4: 设备注册
void test_device_registration(void) {
    spi_bus_config_t config = {...};
    spi_device_t *dev = spi_driver_open_device("spi2", &config);
    assert(dev != NULL);
    assert(spi_driver_get_device_count() == 1);
}

// 测试 5: 块传输
void test_block_transfer(void) {
    uint8_t tx[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t rx[4] = {0};
    assert(spi_device_transfer(dev, tx, rx, 4) == 0);
    // 验证数据传输
}
```

### 集成测试

```c
// 测试 6: 设备发现
void test_device_discovery(void) {
    board_service_init();
    
    // 验证 ICM20608 可以访问
    uint8_t who_am_i;
    assert(icm20608_read_register(..., &who_am_i) == 0);
    assert(who_am_i == 0xAF);
}

// 测试 7: 多设备访问
void test_multi_device_access(void) {
    board_service_init();
    
    // 交替访问不同设备
    w25q_read(...);
    icm20608_read(...);
    w25q_write(...);
    icm20608_read(...);
}

// 测试 8: 并发访问
void test_concurrent_access(void) {
    board_service_init();
    
    // 创建两个线程，分别访问不同设备
    rt_thread_create(..., w25q_thread, ...);
    rt_thread_create(..., icm20608_thread, ...);
    
    rt_thread_mdelay(5000);
    
    // 验证两个线程都能正常访问
}
```

---

## 📊 性能优化建议

### 1. 使用 DMA 加速传输

```c
// 当前: 轮询方式 (~50 KB/s)
// 优化: DMA 方式 (~1 MB/s)

void spi2_transfer_dma(const uint8_t *tx, uint8_t *rx, int len) {
    // 配置 DMA
    DMA_Tx_Init(tx, len);
    DMA_Rx_Init(rx, len);
    
    // 启动 SPI DMA 传输
    SPI2->CR2 |= SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN;
    
    // 等待完成
    while (!(DMA1_Stream4->NDTR == 0 && DMA1_Stream3->NDTR == 0));
    
    SPI2->CR2 &= ~(SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN);
}
```

### 2. 使用中断代替轮询

```c
// 当前: 轮询等待 TX 完成
while (!(SPI2->SR & SPI_SR_TXE));

// 优化: 中断驱动
void SPI2_IRQHandler(void) {
    if (SPI2->SR & SPI_SR_RXNE) {
        rx_data = SPI2->DR;
        spi_rx_callback(rx_data);
    }
}
```

### 3. 动态速率调整

```c
// 根据设备需求调整速率
int spi_device_set_speed(spi_device_t *dev, uint32_t speed) {
    // 计算分频值
    uint8_t prescaler = calculate_prescaler(speed);
    
    // 更新 SPI2 配置
    SPI2->CR1 = (SPI2->CR1 & ~SPI_CR1_BR_Msk) | prescaler;
    
    return 0;
}
```

---

## 🔗 扩展点

### 添加 SPI3 总线

1. 创建 `spi3_adapter.c` 和 `spi3_adapter.h`
2. 在 `spi_config.h` 中添加 SPI3 的 GPIO 映射
3. 实现 `spi3_adapter_init()` 函数
4. 注册硬件回调到通用驱动
5. 在 `board_service.c` 中调用初始化

### 添加新的传输模式

- 支持 16-bit 数据字宽
- 支持 CRC 校验
- 支持 CPOL/CPHA 动态切换
- 支持片选持续时间配置

### 支持 RTOS 事件

```c
// 使用事件集代替互斥锁
rt_event_t spi_event;

int spi_device_read_async(spi_device_t *dev, ..., rt_event_t *event) {
    // 异步读取
    // 完成时发送事件信号
    rt_event_send(event, SPI_READ_COMPLETE);
}
```

---

## 📚 参考资源

- **STM32F407 数据手册**: SPI 部分在第 28-32 章
- **STM32 HAL 库**: 参考 stm32f4xx_hal_spi.c
- **RT-Thread 文档**: RTOS API 使用
- **优化总结**: `SPI_OPTIMIZATION_SUMMARY.md`
- **集成指南**: `SPI_INTEGRATION_GUIDE.md`

---

**架构参考版本**: 1.0  
**维护者**: SPI 驱动开发团队  
**最后更新**: 2026-08-20
