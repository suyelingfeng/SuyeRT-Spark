/**
 * @file lv_port_indev.c
 * @brief 把板上四个低电平有效的方向键映射为 LVGL 键盘输入设备。
 *
 * 数据流：GPIOC 电平 -> 25 ms 消抖 -> LVGL 键值（PREV/NEXT/ENTER/ESC）
 * -> lv_indev 读回调 -> lv_group 焦点移动。读取发生在 lv_timer_handler()
 * 内部，因此本文件代码只在 gui_thread 上下文运行，无需独立按键线程。
 */
#include "lv_port_indev.h"
#include "main.h"

/* 原理图引脚分配：left=PC0, down=PC1, right=PC4, up=PC5，按键另一端接地。 */
#define KEY_LEFT_PIN  GPIO_PIN_0
#define KEY_DOWN_PIN  GPIO_PIN_1
#define KEY_RIGHT_PIN GPIO_PIN_4
#define KEY_UP_PIN    GPIO_PIN_5

/* 消抖窗口：电平保持 25 ms 不变才被采信，用来滤掉机械触点的弹跳。 */
#define KEY_DEBOUNCE_MS 25U

/* 消抖状态机变量：candidate_* 记录正在观察的候选电平，stable_key 为已确认的稳定值。 */
static uint32_t last_key = LV_KEY_NEXT;
static uint32_t candidate_key;
static uint32_t stable_key;
static uint32_t candidate_since;
static lv_indev_t *keypad_indev;

/*
 * 按键处理链路：GPIO电平 -> 25 ms 消抖 -> LVGL键值 -> lv_group焦点移动。
 * 四个按键均为低电平按下；UP/DOWN 切换焦点，RIGHT 确认，LEFT 返回。
 */

/**
 * @brief 读取四个按键的原始电平并合成位掩码（低电平按下，已转成"按下=置位"）。
 * @retval 位掩码，按下位由 LV_PORT_KEY_xxx 表示；全 0 表示无键按下。
 * @note 直接读 GPIO，未消抖，仅供消抖状态机和 shell 诊断使用。
 */
uint32_t lv_port_indev_get_key_mask(void)
{
    uint32_t mask = 0U;
    if (HAL_GPIO_ReadPin(GPIOC, KEY_LEFT_PIN) == GPIO_PIN_RESET) mask |= LV_PORT_KEY_LEFT;
    if (HAL_GPIO_ReadPin(GPIOC, KEY_DOWN_PIN) == GPIO_PIN_RESET) mask |= LV_PORT_KEY_DOWN;
    if (HAL_GPIO_ReadPin(GPIOC, KEY_RIGHT_PIN) == GPIO_PIN_RESET) mask |= LV_PORT_KEY_RIGHT;
    if (HAL_GPIO_ReadPin(GPIOC, KEY_UP_PIN) == GPIO_PIN_RESET) mask |= LV_PORT_KEY_UP;
    return mask;
}

/* 消抖状态机 + 键值映射：候选电平连续稳定 25 ms 后才提升为 stable_key。 */
static uint32_t read_mapped_key(void)
{
    uint32_t raw_key = lv_port_indev_get_key_mask();
    uint32_t now = lv_tick_get();

    /* 触点弹跳期间电平会来回跳，所以只在电平保持不变满 25 ms 后才更新 stable_key。 */
    if (raw_key != candidate_key)
    {
        candidate_key = raw_key;
        candidate_since = now;
    }
    else if (lv_tick_diff(now, candidate_since) >= KEY_DEBOUNCE_MS)
    {
        stable_key = candidate_key;
    }

    /* 板上没有独立的确认/返回键：RIGHT 兼任确认（Enter），LEFT 兼任返回（Esc）。 */
    if ((stable_key & LV_PORT_KEY_UP) != 0U) return LV_KEY_PREV;
    if ((stable_key & LV_PORT_KEY_DOWN) != 0U) return LV_KEY_NEXT;
    if ((stable_key & LV_PORT_KEY_RIGHT) != 0U) return LV_KEY_ENTER;
    if ((stable_key & LV_PORT_KEY_LEFT) != 0U) return LV_KEY_ESC;
    return 0U;
}

/* LVGL 键盘读回调：向 LVGL 上报最近一次确认按键的按下/释放状态。 */
static void keypad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    /* LVGL 在 lv_timer_handler() 内部周期调用此回调，不需要单独按键线程。 */
    const uint32_t key = read_mapped_key();
    LV_UNUSED(indev);

    if (key != 0U)
    {
        last_key = key;
        data->key = key;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        /* LVGL 上报释放事件时仍需附带对应的键码，因此要记住 last_key。 */
        data->key = last_key;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/**
 * @brief 切屏前丢弃触发切屏的那次按键，并屏蔽到所有键物理松开为止。
 *
 * 否则这次按键的释放事件会被派发到新建屏幕的控件上，造成误触发。
 */
void lv_port_indev_begin_screen_change(void)
{
    candidate_key = 0U;
    stable_key = 0U;
    candidate_since = lv_tick_get();

    /* 直接复用 LVGL 自带的"等待释放"门控：物理松开后自动解除，不用自己维护状态。 */
    if (keypad_indev != NULL)
    {
        lv_indev_wait_release(keypad_indev);
        lv_indev_reset(keypad_indev, NULL);
    }
}

/**
 * @brief 初始化 GPIOC 四个方向键，并注册 LVGL 键盘输入设备与焦点组。
 * @retval 非 NULL  成功，返回焦点组；UI 把控件加入该组即可响应按键。
 * @retval NULL     失败（LVGL 内存不足）。
 */
lv_group_t *lv_port_indev_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    lv_indev_t *indev;
    lv_group_t *group;

    /* 按键一端接地、低电平有效，所以配成内部上拉输入：未按下时读到稳定的高电平。 */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    gpio.Pin = KEY_LEFT_PIN | KEY_DOWN_PIN | KEY_RIGHT_PIN | KEY_UP_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio);

    candidate_key = 0U;
    stable_key = 0U;
    candidate_since = lv_tick_get();
    group = lv_group_create();
    indev = lv_indev_create();
    if ((group == NULL) || (indev == NULL))
    {
        return NULL;
    }

    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, keypad_read);
    lv_indev_set_group(indev, group);
    keypad_indev = indev;
    return group;
}
