/*
 * Retro-Go 学而思小喵掌机 (Xueersi XiaoMiao / XUEERSI-ESP32) 目标配置
 *
 * 硬件依据：XiaoMiao Desktop 硬件信息导出文档
 *  - MCU: ESP32-WROVER (双核, 240MHz, 4MB flash, 8MB PSRAM)
 *  - 显示: ST7735 160x128 横屏, SPI2 (SCLK=18 MOSI=23 MISO=19 CS=5 DC=4)
 *  - 背光: GPIO0, LEDC PWM 5kHz 13bit
 *  - 按键: A=34 B=12 UP=2 DOWN=13 LEFT=27 RIGHT=35 (低电平有效)
 *  - 音频: I2S 外置功放 NS4168 (BCK=25 WS=32 DOUT=33)
 *  - LED : WS2812B NeoPixel x3, GPIO14 (蜂鸣器已物理拆除)
 *  - SD  : CS=22
 */
// Target definition
#define RG_TARGET_NAME             "XUEERSI-XIAOMIAO"
#define RG_PROJECT_NAME            "Xueersi Retro-Go"

// Storage: MicroSD over SPI2, shared with TFT
#define RG_STORAGE_ROOT             "/sd"
#define RG_STORAGE_SDSPI_HOST       SPI2_HOST
#define RG_STORAGE_SDSPI_SPEED      SDMMC_FREQ_DEFAULT

// Audio: I2S external amplifier (NS4168). GPIO14 is now WS2812B LED, buzzer removed.
#define RG_AUDIO_USE_BUZZER_PIN     0   // 0 = Disable (buzzer physically removed)
#define RG_AUDIO_USE_INT_DAC        0   // 0 = Disable
#define RG_AUDIO_USE_EXT_DAC        1   // 1 = Enable (external NS4168 amplifier via I2S)

// Video: ST7735 160x128 landscape, SPI2 @ 20MHz
#define RG_SCREEN_DRIVER            2   // 2 = ST7735
#define RG_SCREEN_HOST              SPI2_HOST
#define RG_SCREEN_SPEED             SPI_MASTER_FREQ_20M
#define RG_SCREEN_BACKLIGHT         80  // default backlight level (0-100), GPIO0 via LEDC
#define RG_SCREEN_WIDTH             160
#define RG_SCREEN_HEIGHT            128
#define RG_SCREEN_ROTATE            0
#define RG_SCREEN_VISIBLE_AREA      {0, 0, 0, 0}
#define RG_SCREEN_SAFE_AREA         {0, 0, 0, 0}
#define RG_SCREEN_MADCTL            0x60 // MX | MV, landscape
#define RG_SCREEN_SWAP_BYTES        1

// Input: 6 GPIO buttons, active-low
// START=UP+DOWN, SELECT=LEFT+RIGHT, MENU=UP+DOWN+B
#define RG_RECOVERY_BTN             RG_KEY_MENU
#define RG_GAMEPAD_GPIO_MAP {\
    {RG_KEY_UP,     .num = GPIO_NUM_2,  .pullup = 1, .level = 0},\
    {RG_KEY_DOWN,   .num = GPIO_NUM_13, .pullup = 1, .level = 0},\
    {RG_KEY_LEFT,   .num = GPIO_NUM_27, .pullup = 1, .level = 0},\
    {RG_KEY_RIGHT,  .num = GPIO_NUM_35, .pullup = 0, .level = 0},\
    {RG_KEY_A,      .num = GPIO_NUM_34, .pullup = 0, .level = 0},\
    {RG_KEY_B,      .num = GPIO_NUM_12, .pullup = 1, .level = 0},\
}
#define RG_GAMEPAD_VIRT_MAP {\
    {RG_KEY_START,  .src = RG_KEY_UP | RG_KEY_DOWN},\
    {RG_KEY_SELECT, .src = RG_KEY_LEFT | RG_KEY_RIGHT},\
    {RG_KEY_MENU,   .src = RG_KEY_UP | RG_KEY_DOWN | RG_KEY_B},\
}

// Battery: GPIO34 shared with button A (ADC1_CH6). Keep disabled for now.
#define RG_BATTERY_DRIVER           0
#define RG_BATTERY_CALC_PERCENT(raw) (100)
#define RG_BATTERY_CALC_VOLTAGE(raw) (0)

// Status LED: WS2812B NeoPixel strip on GPIO14 (3 LEDs)
#define RG_GPIO_LED                 GPIO_NUM_14

// SPI Display pins
#define RG_GPIO_LCD_MISO            GPIO_NUM_19
#define RG_GPIO_LCD_MOSI            GPIO_NUM_23
#define RG_GPIO_LCD_CLK             GPIO_NUM_18
#define RG_GPIO_LCD_CS              GPIO_NUM_5
#define RG_GPIO_LCD_DC              GPIO_NUM_4
#define RG_GPIO_LCD_BCKL            GPIO_NUM_0   // Backlight (LEDC PWM)

// External I2S DAC / amplifier (NS4168)
#define RG_GPIO_SND_I2S_BCK         GPIO_NUM_25
#define RG_GPIO_SND_I2S_WS          GPIO_NUM_32
#define RG_GPIO_SND_I2S_DATA        GPIO_NUM_33
// #define RG_GPIO_SND_AMP_ENABLE      GPIO_NUM_NC

// SPI SD Card
#define RG_GPIO_SDSPI_MISO          GPIO_NUM_19
#define RG_GPIO_SDSPI_MOSI          GPIO_NUM_23
#define RG_GPIO_SDSPI_CLK           GPIO_NUM_18
#define RG_GPIO_SDSPI_CS            GPIO_NUM_22