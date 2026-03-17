/**
 * @file st7735_display.h
 * @brief ST7735S 128×160 显示驱动 + 智慧农业监测UI
 *
 * 引脚配置见 ai_config.h（LCD_* 宏定义）
 * 界面布局（竖屏 128×160）:
 *   Y=  0  标题栏        18px  深绿背景 "智慧农业" (16×16)
 *   Y= 18  品种栏        10px  深蓝背景 "> EUCALYPTUS <"
 *   Y= 28  分割线         1px
 *   Y= 29  气温/湿度      30px  双列，16px中文标签 + 数值
 *   Y= 59  分割线         1px
 *   Y= 60  土温/土湿      30px  双列，16px中文标签 + 数值
 *   Y= 90  分割线         1px
 *   Y= 91  光照            20px  全宽单行
 *   Y=111  分割线          1px
 *   Y=112  气压 / pH       20px  双列单行
 *   Y=132  分割线          1px
 *   Y=133  NPK             15px  三列彩色
 *   Y=148  分割线          1px
 *   Y=149  状态栏          11px  WiFi/MQTT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* ── 屏幕分辨率 ── */
#define LCD_WIDTH    128
#define LCD_HEIGHT   160

/* ── RGB565 颜色宏 ── */
#define COLOR_RGB(r, g, b) \
    ((uint16_t)(((uint16_t)((r) & 0xF8) << 8) | ((uint16_t)((g) & 0xFC) << 3) | ((b) >> 3)))

/* ── 主题色板（深色农业风） ── */
#define COLOR_BG          COLOR_RGB( 10,  20,  35)   /* 深海军蓝背景       */
#define COLOR_TITLE_BG    COLOR_RGB( 15,  70,  25)   /* 深绿标题栏         */
#define COLOR_SPECIES_BG  COLOR_RGB( 18,  35,  65)   /* 品种栏背景         */
#define COLOR_CARD_BG     COLOR_RGB( 15,  28,  50)   /* 数据卡背景         */
#define COLOR_WHITE       0xFFFFU
#define COLOR_YELLOW      COLOR_RGB(255, 220,   0)   /* 品种名高亮         */
#define COLOR_CYAN        COLOR_RGB( 60, 200, 240)   /* 数据标签           */
#define COLOR_VALUE       COLOR_RGB(160, 255, 160)   /* 数值（浅绿）       */
#define COLOR_ALERT       COLOR_RGB(255,  80,  40)   /* 告警橙红           */
#define COLOR_OK          COLOR_RGB( 60, 220, 100)   /* 正常绿             */
#define COLOR_LINE        COLOR_RGB( 30,  60,  90)   /* 分割线深蓝灰       */
#define COLOR_NPK_N       COLOR_RGB( 80, 255,  80)   /* 氮 N 绿            */
#define COLOR_NPK_P       COLOR_RGB(255, 200,  60)   /* 磷 P 黄            */
#define COLOR_NPK_K       COLOR_RGB(100, 160, 255)   /* 钾 K 蓝            */
#define COLOR_GRAY        COLOR_RGB(100, 120, 140)   /* 无数据灰           */

/* ── 传感器数据（与 main.c sensor_packet_t 字段一一对应） ── */
typedef struct {
    float    lux;
    float    env_temp_c;
    float    env_humi_pct;
    float    press_kpa;
    float    soil_temp_c;
    float    soil_humi_pct;
    float    ph;
    uint16_t n;
    uint16_t p;
    uint16_t k;
} display_sensor_data_t;

/**
 * @brief  初始化 ST7735S 及 SPI 总线，绘制静态框架
 * @return ESP_OK / 错误码
 */
esp_err_t lcd_init(void);

/**
 * @brief  刷新全屏传感器数据
 * @param  data       传感器数据（NULL → 显示等待画面）
 * @param  has_data   是否已收到有效帧
 * @param  species_en 监测品种英文缩写（如 "Eucalyptus"，≤13字符）
 * @param  wifi_ok    WiFi 已连接
 * @param  mqtt_ok    MQTT 已连接
 * @param  alert_str  告警描述（空串 = 正常）
 */
void lcd_update(const display_sensor_data_t *data,
                bool        has_data,
                const char *species_en,
                bool        wifi_ok,
                bool        mqtt_ok,
                const char *alert_str);

/**
 * @brief  设置背光（0 = 关，255 = 最亮）
 *         当前实现：0 关，其余开（无 PWM）
 */
void lcd_set_backlight(uint8_t brightness);
