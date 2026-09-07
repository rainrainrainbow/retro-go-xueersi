/*
 * WS2812B (NeoPixel) LED driver & effects - Xueersi XiaoMiao handheld
 * 3x WS2812B on GPIO14 (RMT), GRB order. ESP-IDF legacy RMT API.
 */
#include "rg_system.h"

#ifdef RG_GPIO_LED_WS2812B

#include <driver/rmt.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <string.h>

#define LED_COUNT       (3)
#define LED_GPIO        (RG_GPIO_LED_WS2812B)
#define LED_T0H         (8)
#define LED_T1H         (18)
#define LED_TOTAL       (25)
#define ANIM_CYCLE      (256)
#define BRIGHTNESS      (128)

static rmt_channel_t rmt_channel = RMT_CHANNEL_0;
static rmt_item32_t led_items[(LED_COUNT * 24) + 1];
static uint8_t led_buffer[LED_COUNT * 3];
static bool initialized = false;
static rg_led_effect_t effect = RG_LED_EFFECT_OFF;
static uint16_t base_color = 0xFFFF;
static uint32_t anim_step = 0;

static void rgb565_to_rgb888(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = ((c >> 11) & 0x1F) << 3;
    *g = ((c >> 5) & 0x3F) << 2;
    *b = (c & 0x1F) << 3;
}

static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint16_t region, rem, p, q, t;
    if (s == 0) { *r = v; *g = v; *b = v; return; }
    region = h / 43;
    rem = (h - (region * 43)) * 6;
    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * rem) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - rem)) >> 8))) >> 8;
    switch (region) {
        case 0: *r=v;*g=t;*b=p; break;
        case 1: *r=q;*g=v;*b=p; break;
        case 2: *r=p;*g=v;*b=t; break;
        case 3: *r=p;*g=q;*b=v; break;
        case 4: *r=t;*g=p;*b=v; break;
        default:*r=v;*g=p;*b=q; break;
    }
}

static void led_set_grb(int index, uint8_t g, uint8_t r, uint8_t b)
{
    if (index < 0 || index >= LED_COUNT) return;
    led_buffer[index*3+0] = g;
    led_buffer[index*3+1] = r;
    led_buffer[index*3+2] = b;
}

static void led_build_items(void)
{
    rmt_item32_t *item = led_items;
    int i, bit;
    for (i = 0; i < LED_COUNT; ++i) {
        for (bit = 0; bit < 24; ++bit) {
            uint8_t byte = led_buffer[i*3 + bit/8];
            bool level = (byte >> (7 - (bit % 8))) & 1;
            item->level0 = 1;
            item->duration0 = level ? LED_T1H : LED_T0H;
            item->level1 = 0;
            item->duration1 = LED_TOTAL - item->duration0;
            item++;
        }
    }
    item->level0 = 0;
    item->duration0 = 100;
    item->level1 = 0;
    item->duration1 = 100;
}

void rg_led_init(void)
{
    if (initialized) return;

    rmt_config_t config = {
        .rmt_mode = RMT_MODE_TX,
        .channel = rmt_channel,
        .gpio_num = LED_GPIO,
        .clk_div = 4,
        .mem_block_num = 1,
        .flags = 0,
        .tx_config.loop_en = false,
        .tx_config.carrier_en = false,
        .tx_config.idle_output_en = true,
        .tx_config.idle_level = RMT_IDLE_LEVEL_LOW,
    };
    if (rmt_config(&config) == ESP_OK && rmt_driver_install(config.channel, 0, 0) == ESP_OK)
    {
        initialized = true;
        memset(led_buffer, 0, sizeof(led_buffer));
        led_build_items();
        RG_LOGI("WS2812B LED init OK\n");
    }
    else
    {
        RG_LOGE("WS2812B LED init failed!\n");
    }
}

void rg_led_deinit(void)
{
    if (!initialized) return;
    rmt_driver_uninstall(rmt_channel);
    initialized = false;
}

void rg_led_set_pixel(int index, uint16_t color)
{
    uint8_t r, g, b;
    if (!initialized || index < 0 || index >= LED_COUNT) return;
    rgb565_to_rgb888(color, &r, &g, &b);
    led_set_grb(index, g, r, b);
}

void rg_led_update(void)
{
    if (!initialized) return;
    led_build_items();
    rmt_write_items(rmt_channel, led_items, LED_COUNT * 24 + 1, false);
}

static void led_process(void)
{
    int i;
    anim_step++;
    switch (effect)
    {
        case RG_LED_EFFECT_OFF:
            memset(led_buffer, 0, sizeof(led_buffer));
            break;

        case RG_LED_EFFECT_STATIC:
            for (i = 0; i < LED_COUNT; ++i)
                rg_led_set_pixel(i, base_color);
            break;

        case RG_LED_EFFECT_BREATH:
        {
            float phase = (float)(anim_step % ANIM_CYCLE) / ANIM_CYCLE * 6.283f;
            float bright = (sinf(phase) + 1.0f) / 2.0f;
            uint8_t r, g, b;
            rgb565_to_rgb888(base_color, &r, &g, &b);
            for (i = 0; i < LED_COUNT; ++i)
                led_set_grb(i,
                    (uint8_t)(g * bright),
                    (uint8_t)(r * bright),
                    (uint8_t)(b * bright));
            break;
        }

        case RG_LED_EFFECT_RAINBOW:
        case RG_LED_EFFECT_RAINBOW_CYCLE:
            for (i = 0; i < LED_COUNT; ++i)
            {
                uint16_t hue16 = ((anim_step % ANIM_CYCLE) << 8) / ANIM_CYCLE + i * (65536 / LED_COUNT);
                uint8_t r, g, b;
                hsv_to_rgb((hue16 >> 8) & 0xFF, 255, BRIGHTNESS, &r, &g, &b);
                led_set_grb(i, g, r, b);
            }
            break;

        default:
            break;
    }
}

void rg_led_task(void)
{
    if (!initialized || effect == RG_LED_EFFECT_OFF)
        return;
    led_process();
    rg_led_update();
}

void rg_led_set_effect(rg_led_effect_t new_effect, uint16_t color)
{
    effect = new_effect;
    if (color)
        base_color = color;
}

#endif
