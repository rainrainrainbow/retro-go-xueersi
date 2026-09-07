/*
 * WS2812B (NeoPixel) LED 驱动与灯效 - 学而思小喵掌机
 *
 * 硬件: WS2812B x3, 数据引脚 GPIO14 (RMT 驱动), GRB 位序
 * 仅当定义了 RG_GPIO_LED_WS2812B 时编译 (在 target config.h 中设置)
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// LED 灯效模式
typedef enum {
    RG_LED_EFFECT_OFF = 0,      // 关闭
    RG_LED_EFFECT_STATIC,       // 静态颜色
    RG_LED_EFFECT_BREATH,       // 呼吸灯
    RG_LED_EFFECT_RAINBOW,      // 彩虹循环
    RG_LED_EFFECT_RAINBOW_CYCLE,// 彩虹流动
    RG_LED_EFFECT_COUNT,
} rg_led_effect_t;

// 初始化 WS2812B (在 rg_system_init 早期调用)
void rg_led_init(void);

// 关闭并释放 RMT
void rg_led_deinit(void);

// 设置灯效模式与颜色 (color = RGB565, 高字节R低字节B)
void rg_led_set_effect(rg_led_effect_t effect, uint16_t color);

// 设置单颗 LED 颜色 (index: 0-2, color: RGB565)
void rg_led_set_pixel(int index, uint16_t color);

// 刷新到硬件 (将缓冲区发送到 LED strip)
void rg_led_update(void);

#ifdef __cplusplus
}
#endif
