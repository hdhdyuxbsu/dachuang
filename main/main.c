#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "lwip/inet.h"
#include "esp_timer.h"
#include "mqtt_client.h"
#include "mbedtls/md.h"
#include "mbedtls/base64.h"

#define TAG "A39C_RX"

/* ====== WiFi STA 配置（连接路由器） ====== */
#define WIFI_STA_SSID      "ESP"
#define WIFI_STA_PASS      "123456789"
#define WIFI_MAX_RETRY     10

/* ====== OneNet MQTT 配置 ====== */
#define ONENET_PRODUCT_ID   "W9BY3SlO8w"
#define ONENET_DEVICE_NAME  "dachuang"
#define ONENET_DEVICE_KEY   "aDFIWDNUeFpLTTdUTzdCS1VzdWlyV3JLS3pnR2tkdVU="
#define ONENET_BROKER_URL   "mqtt://mqtts.heclouds.com"
#define ONENET_ET           "4102444800"  /* 2100-01-01, token 长期有效 */
#define ONENET_RES          "products/" ONENET_PRODUCT_ID "/devices/" ONENET_DEVICE_NAME
#define ONENET_TOPIC_POST   "$sys/" ONENET_PRODUCT_ID "/" ONENET_DEVICE_NAME "/thing/property/post"

/* ====== 引脚配置（ESP32-S3 N16R8） ====== */
/* 注意: GPIO26-37 被 Octal Flash/PSRAM 占用，不可使用 */
#define A39C_UART_NUM      UART_NUM_2
#define A39C_TX_PIN        GPIO_NUM_17   /* ESP32-S3 TX -> A39C RXD */
#define A39C_RX_PIN        GPIO_NUM_16   /* ESP32-S3 RX <- A39C TXD */
#define A39C_BAUD          9600
#define A39C_MD0_PIN       GPIO_NUM_15   /* 模式选择引脚 MD0 */
#define A39C_MD1_PIN       GPIO_NUM_14   /* 模式选择引脚 MD1 */
#define A39C_AUX_PIN       GPIO_NUM_18   /* AUX 状态引脚 */
#define A39C_MD0_LEVEL     1
#define A39C_MD1_LEVEL     0

/* ====== 帧格式 ====== */
#define FRAME_HEAD1        0xAA
#define FRAME_HEAD2        0x55
#define FRAME_PAYLOAD_LEN  0x19
#define FRAME_TOTAL_LEN    29

typedef struct {
    uint8_t seq;
    float lux;
    float env_temp_c;
    float env_humi_pct;
    float press_kpa;
    float soil_temp_c;
    float soil_humi_pct;
    float ph;
    uint16_t n;
    uint16_t p;
    uint16_t k;
} sensor_packet_t;

static uint32_t ok_count = 0;
static uint32_t fail_count = 0;
static uint32_t rx_bytes = 0;
static uint32_t aux_low_count = 0;

/* 最新一帧数据（供 HTTP 接口读取） */
static sensor_packet_t latest_pkt = {0};
static bool has_data = false;

/* ====== 历史数据环形缓冲区（趋势图表） ====== */
#define HISTORY_MAX 60

typedef struct {
    float env_temp;
    float env_humi;
    float soil_temp;
    float soil_humi;
    float lux;
    float ph;
    uint32_t timestamp_s;   /* 自启动以来的秒数 */
} history_point_t;

static history_point_t history_buf[HISTORY_MAX];
static int history_head = 0;   /* 下一个写入位置 */
static int history_count = 0;  /* 当前存储的数据点数 */

static void history_push(const sensor_packet_t *pkt)
{
    history_point_t *pt = &history_buf[history_head];
    pt->env_temp  = pkt->env_temp_c;
    pt->env_humi  = pkt->env_humi_pct;
    pt->soil_temp = pkt->soil_temp_c;
    pt->soil_humi = pkt->soil_humi_pct;
    pt->lux       = pkt->lux;
    pt->ph        = pkt->ph;
    pt->timestamp_s = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    history_head = (history_head + 1) % HISTORY_MAX;
    if (history_count < HISTORY_MAX) history_count++;
}

/* 嵌入的 HTML 文件 */
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

static void log_packet(const sensor_packet_t *pkt)
{
    ESP_LOGI(TAG, "解析成功: SEQ=%u", pkt->seq);
    ESP_LOGI(TAG, "  光照=%.2f lx, 环境温度=%.1f ℃, 环境湿度=%.1f %%",
             pkt->lux, pkt->env_temp_c, pkt->env_humi_pct);
    ESP_LOGI(TAG, "  气压=%.1f hPa, 土壤温度=%.1f ℃, 土壤湿度=%.1f %%",
             pkt->press_kpa, pkt->soil_temp_c, pkt->soil_humi_pct);
    ESP_LOGI(TAG, "  pH=%.2f, N=%u mg/kg, P=%u mg/kg, K=%u mg/kg",
             pkt->ph, pkt->n, pkt->p, pkt->k);
}

static inline uint16_t u16le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline int16_t i16le(const uint8_t *p)
{
    return (int16_t)u16le(p);
}

static inline uint32_t u32le(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static bool parse_frame(const uint8_t *f, sensor_packet_t *out)
{
    if (f[0] != FRAME_HEAD1 || f[1] != FRAME_HEAD2) {
        return false;
    }
    if (f[2] != FRAME_PAYLOAD_LEN) {
        return false;
    }

    uint8_t xor_val = 0;
    for (int i = 0; i < FRAME_TOTAL_LEN - 1; i++) {
        xor_val ^= f[i];
    }
    if (xor_val != f[FRAME_TOTAL_LEN - 1]) {
        return false;
    }

    out->seq           = f[3];
    out->lux           = (float)u32le(&f[4]) / 100.0f;
    out->env_temp_c    = (float)i16le(&f[8]) / 10.0f;
    out->env_humi_pct  = (float)u16le(&f[10]) / 10.0f;
    out->press_kpa     = (float)u32le(&f[12]) / 100.0f;  /* 实际单位 hPa */
    out->soil_temp_c   = (float)i16le(&f[16]) / 10.0f;
    out->soil_humi_pct = (float)u16le(&f[18]) / 10.0f;
    out->ph            = (float)u16le(&f[20]) / 100.0f;
    out->n             = u16le(&f[22]);
    out->p             = u16le(&f[24]);
    out->k             = u16le(&f[26]);
    return true;
}

/* ====== WiFi STA 初始化 ====== */
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_retry_num = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "重新连接WiFi... 第%d次", s_retry_num);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "WiFi连接失败，已达最大重试次数");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "获得IP地址: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_STA_SSID,
            .password = WIFI_STA_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "正在连接WiFi: %s ...", WIFI_STA_SSID);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi已连接: %s", WIFI_STA_SSID);
    } else {
        ESP_LOGE(TAG, "WiFi连接失败");
    }
}

/* ====== HTTP 服务器 ====== */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    size_t len = index_html_end - index_html_start;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, len);
    return ESP_OK;
}

static esp_err_t api_data_handler(httpd_req_t *req)
{
    char buf[320];
    if (has_data) {
        snprintf(buf, sizeof(buf),
            "{\"seq\":%u,\"lux\":%.2f,\"env_temp\":%.1f,\"env_humi\":%.1f,"
            "\"press\":%.1f,\"soil_temp\":%.1f,\"soil_humi\":%.1f,"
            "\"ph\":%.2f,\"n\":%u,\"p\":%u,\"k\":%u,"
            "\"ok\":%lu,\"fail\":%lu}",
            latest_pkt.seq, latest_pkt.lux, latest_pkt.env_temp_c, latest_pkt.env_humi_pct,
            latest_pkt.press_kpa, latest_pkt.soil_temp_c, latest_pkt.soil_humi_pct,
            latest_pkt.ph, latest_pkt.n, latest_pkt.p, latest_pkt.k,
            (unsigned long)ok_count, (unsigned long)fail_count);
    } else {
        snprintf(buf, sizeof(buf),
            "{\"seq\":0,\"lux\":0,\"env_temp\":0,\"env_humi\":0,"
            "\"press\":0,\"soil_temp\":0,\"soil_humi\":0,"
            "\"ph\":0,\"n\":0,\"p\":0,\"k\":0,"
            "\"ok\":0,\"fail\":0}");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, strlen(buf));
    return ESP_OK;
}

static esp_err_t api_history_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    char line[128];

    /* 发送 JSON 数组开头 */
    httpd_resp_sendstr_chunk(req, "[");

    /* 从最旧到最新遍历环形缓冲区 */
    int start = (history_count < HISTORY_MAX) ? 0 : history_head;
    for (int i = 0; i < history_count; i++) {
        int idx = (start + i) % HISTORY_MAX;
        const history_point_t *pt = &history_buf[idx];
        int len = snprintf(line, sizeof(line),
            "%s{\"t\":%lu,\"et\":%.1f,\"eh\":%.1f,\"st\":%.1f,\"sh\":%.1f,\"lx\":%.1f,\"ph\":%.2f}",
            (i > 0) ? "," : "",
            (unsigned long)pt->timestamp_s,
            pt->env_temp, pt->env_humi,
            pt->soil_temp, pt->soil_humi,
            pt->lux, pt->ph);
        httpd_resp_send_chunk(req, line, len);
    }

    httpd_resp_sendstr_chunk(req, "]");
    httpd_resp_send_chunk(req, NULL, 0);  /* 结束 chunked 响应 */
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = root_get_handler,
        };
        httpd_register_uri_handler(server, &root_uri);

        httpd_uri_t api_uri = {
            .uri       = "/api/data",
            .method    = HTTP_GET,
            .handler   = api_data_handler,
        };
        httpd_register_uri_handler(server, &api_uri);

        httpd_uri_t history_uri = {
            .uri       = "/api/history",
            .method    = HTTP_GET,
            .handler   = api_history_handler,
        };
        httpd_register_uri_handler(server, &history_uri);

        ESP_LOGI(TAG, "HTTP 服务器已启动");
    }
    return server;
}

/* ====== OneNet MQTT ====== */
static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

/* URL 编码: 仅处理 Base64 中的特殊字符 +/= */
static void url_encode_b64(const char *src, char *dst, size_t dst_size)
{
    size_t si = 0, di = 0;
    while (src[si] && di + 3 < dst_size) {
        char c = src[si++];
        if (c == '+')      { dst[di++]='%'; dst[di++]='2'; dst[di++]='B'; }
        else if (c == '/') { dst[di++]='%'; dst[di++]='2'; dst[di++]='F'; }
        else if (c == '=') { dst[di++]='%'; dst[di++]='3'; dst[di++]='D'; }
        else               { dst[di++] = c; }
    }
    dst[di] = '\0';
}

/* 生成 OneNet MQTT 鉴权 token */
static int onenet_gen_token(char *out, size_t out_size)
{
    /* 1) Base64 解码 device_key */
    unsigned char key_raw[64];
    size_t key_len = 0;
    if (mbedtls_base64_decode(key_raw, sizeof(key_raw), &key_len,
                              (const unsigned char *)ONENET_DEVICE_KEY,
                              strlen(ONENET_DEVICE_KEY)) != 0) {
        ESP_LOGE(TAG, "Base64 解码 device_key 失败");
        return -1;
    }

    /* 2) 构造 StringToSign: et\nmethod\nres\nversion */
    char sts[256];
    snprintf(sts, sizeof(sts), "%s\n%s\n%s\n%s",
             ONENET_ET, "sha256", ONENET_RES, "2018-10-31");

    /* 3) HMAC-SHA256 */
    unsigned char hmac[32];
    if (mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                        key_raw, key_len,
                        (const unsigned char *)sts, strlen(sts),
                        hmac) != 0) {
        ESP_LOGE(TAG, "HMAC-SHA256 计算失败");
        return -1;
    }

    /* 4) Base64 编码 HMAC */
    char hmac_b64[64];
    size_t b64_len = 0;
    if (mbedtls_base64_encode((unsigned char *)hmac_b64, sizeof(hmac_b64),
                              &b64_len, hmac, 32) != 0) {
        ESP_LOGE(TAG, "Base64 编码 HMAC 失败");
        return -1;
    }
    hmac_b64[b64_len] = '\0';

    /* 5) URL 编码 sign 和 res */
    char sign_enc[128], res_enc[128];
    url_encode_b64(hmac_b64, sign_enc, sizeof(sign_enc));
    url_encode_b64(ONENET_RES, res_enc, sizeof(res_enc));

    /* 6) 组装 token */
    snprintf(out, out_size,
             "version=2018-10-31&res=%s&et=%s&method=sha256&sign=%s",
             res_enc, ONENET_ET, sign_enc);

    ESP_LOGI(TAG, "OneNet token 已生成 (len=%d)", (int)strlen(out));
    ESP_LOGI(TAG, "Token: %s", out);
    return 0;
}

/* OneNet 回复 topic */
#define ONENET_TOPIC_REPLY  ONENET_TOPIC_POST "/reply"

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT 已连接 OneNet");
        mqtt_connected = true;
        /* 订阅回复 topic，查看平台是否接受数据 */
        esp_mqtt_client_subscribe(mqtt_client, ONENET_TOPIC_REPLY, 0);
        ESP_LOGI(TAG, "已订阅: %s", ONENET_TOPIC_REPLY);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT 连接断开");
        mqtt_connected = false;
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT 消息已送达 broker, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT 收到回复 topic=%.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "MQTT 回复内容=%.*s", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR: {
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "MQTT 传输层错误");
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGE(TAG, "MQTT 连接被拒绝, code=%d", event->error_handle->connect_return_code);
        }
        break;
    }
    default:
        break;
    }
}

static void mqtt_publish_data(void)
{
    if (!mqtt_connected) return;

    static uint32_t msg_id = 0;
    msg_id++;

    char payload[512];
    if (has_data) {
        snprintf(payload, sizeof(payload),
            "{\"id\":\"%lu\",\"version\":\"1.0\",\"params\":{"
            "\"lux\":{\"value\":%.2f},"
            "\"env_temp\":{\"value\":%.1f},"
            "\"env_humi\":{\"value\":%.1f},"
            "\"press\":{\"value\":%.1f},"
            "\"soil_temp\":{\"value\":%.1f},"
            "\"soil_humi\":{\"value\":%.1f},"
            "\"ph\":{\"value\":%.2f},"
            "\"n\":{\"value\":%u},"
            "\"p\":{\"value\":%u},"
            "\"k\":{\"value\":%u}}}",
            (unsigned long)msg_id,
            latest_pkt.lux, latest_pkt.env_temp_c, latest_pkt.env_humi_pct,
            latest_pkt.press_kpa, latest_pkt.soil_temp_c, latest_pkt.soil_humi_pct,
            latest_pkt.ph, latest_pkt.n, latest_pkt.p, latest_pkt.k);
    } else {
        /* 无 LoRa 数据时发送测试数据，验证云平台通路 */
        snprintf(payload, sizeof(payload),
            "{\"id\":\"%lu\",\"version\":\"1.0\",\"params\":{"
            "\"lux\":{\"value\":1234.56},"
            "\"env_temp\":{\"value\":25.5},"
            "\"env_humi\":{\"value\":60.0},"
            "\"press\":{\"value\":101.3},"
            "\"soil_temp\":{\"value\":22.0},"
            "\"soil_humi\":{\"value\":45.0},"
            "\"ph\":{\"value\":6.80},"
            "\"n\":{\"value\":28},"
            "\"p\":{\"value\":15},"
            "\"k\":{\"value\":30}}}",
            (unsigned long)msg_id);
        ESP_LOGW(TAG, "无 LoRa 数据，发送测试数据");
    }

    ESP_LOGI(TAG, "MQTT Topic: %s", ONENET_TOPIC_POST);
    ESP_LOGI(TAG, "MQTT Payload: %s", payload);
    int ret = esp_mqtt_client_publish(mqtt_client, ONENET_TOPIC_POST, payload, 0, 1, 0);
    ESP_LOGI(TAG, "MQTT 上报数据 id=%lu, ret=%d", (unsigned long)msg_id, ret);
}

static void mqtt_pub_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(5000));  /* 启动后等5秒(确保MQTT已连接) */
    while (1) {
        mqtt_publish_data();
        vTaskDelay(pdMS_TO_TICKS(10000));  /* 每10秒上报一次 */
    }
}

static void mqtt_init(void)
{
    static char token[512];
    if (onenet_gen_token(token, sizeof(token)) != 0) {
        ESP_LOGE(TAG, "OneNet token 生成失败，MQTT 不启动");
        return;
    }

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri  = ONENET_BROKER_URL,
        .broker.address.port = 1883,
        .credentials.client_id = ONENET_DEVICE_NAME,
        .credentials.username  = ONENET_PRODUCT_ID,
        .credentials.authentication.password = token,
    };

    mqtt_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);

    ESP_LOGI(TAG, "MQTT 客户端已启动，正在连接 OneNet...");
    xTaskCreate(mqtt_pub_task, "mqtt_pub", 4096, NULL, 5, NULL);
}

/* ====== 硬件初始化 ====== */
static void a39c_mode_init(void)
{
    ESP_ERROR_CHECK(gpio_reset_pin(A39C_MD0_PIN));
    ESP_ERROR_CHECK(gpio_reset_pin(A39C_MD1_PIN));

    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << A39C_MD0_PIN) | (1ULL << A39C_MD1_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&out_cfg));
    ESP_ERROR_CHECK(gpio_set_level(A39C_MD0_PIN, A39C_MD0_LEVEL));
    ESP_ERROR_CHECK(gpio_set_level(A39C_MD1_PIN, A39C_MD1_LEVEL));
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGI(TAG, "模式脚设置: 目标 MD0=%d MD1=%d, 读回 MD0=%d MD1=%d",
             A39C_MD0_LEVEL,
             A39C_MD1_LEVEL,
             gpio_get_level(A39C_MD0_PIN),
             gpio_get_level(A39C_MD1_PIN));

    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << A39C_AUX_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&in_cfg));
}

static void uart_init_a39c(void)
{
    const uart_config_t cfg = {
        .baud_rate = A39C_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(A39C_UART_NUM, 2048, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(A39C_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(A39C_UART_NUM, A39C_TX_PIN, A39C_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "UART init done: %d 8N1", A39C_BAUD);
}

/* ====== LoRa 接收任务 ====== */
static void lora_rx_task(void *arg)
{
    uint8_t buf[FRAME_TOTAL_LEN];
    int state = 0;   /* 0: 找AA 1: 找55 2: 收剩余 */
    int idx = 0;
    TickType_t last_report_tick = xTaskGetTickCount();
    uint32_t last_rx_bytes = 0;
    uint32_t last_aux_low_count = 0;

    while (1) {
        if (gpio_get_level(A39C_AUX_PIN) == 0) {
            aux_low_count++;
        }

        uint8_t b;
        int n = uart_read_bytes(A39C_UART_NUM, &b, 1, pdMS_TO_TICKS(100));
        if (n <= 0) {
            TickType_t now_tick = xTaskGetTickCount();
            if ((now_tick - last_report_tick) >= pdMS_TO_TICKS(5000)) {
                uint32_t delta_rx = rx_bytes - last_rx_bytes;
                uint32_t delta_aux_low = aux_low_count - last_aux_low_count;
                ESP_LOGI(TAG, "5秒统计: RX字节=%lu, OK=%lu, FAIL=%lu",
                         (unsigned long)rx_bytes,
                         (unsigned long)ok_count,
                         (unsigned long)fail_count);
                ESP_LOGI(TAG, "5秒增量: RX新增=%lu, AUX低电平采样=%lu, 当前AUX=%d",
                         (unsigned long)delta_rx,
                         (unsigned long)delta_aux_low,
                         gpio_get_level(A39C_AUX_PIN));

                if (delta_rx == 0 && delta_aux_low == 0) {
                    ESP_LOGW(TAG, "未见AUX活动，接收模块可能没有收到空中数据，请检查信道/地址/空口速率");
                } else if (delta_rx == 0 && delta_aux_low > 0) {
                    ESP_LOGW(TAG, "AUX有活动但UART无字节，请检查A39C TXD->ESP32-S3 GPIO%d 以及模块串口参数", A39C_RX_PIN);
                }
                last_report_tick = now_tick;
                last_rx_bytes = rx_bytes;
                last_aux_low_count = aux_low_count;
            }
            continue;
        }
        rx_bytes += (uint32_t)n;

        if (state == 0) {
            if (b == FRAME_HEAD1) {
                buf[0] = b;
                state = 1;
            }
        } else if (state == 1) {
            if (b == FRAME_HEAD2) {
                buf[1] = b;
                idx = 2;
                state = 2;
            } else if (b == FRAME_HEAD1) {
                buf[0] = b;
                state = 1;
            } else {
                state = 0;
            }
        } else {
            buf[idx++] = b;
            if (idx >= FRAME_TOTAL_LEN) {
                sensor_packet_t pkt;
                if (parse_frame(buf, &pkt)) {
                    ok_count++;
                    log_packet(&pkt);
                    /* 更新最新数据供网页读取 */
                    latest_pkt = pkt;
                    has_data = true;
                    history_push(&pkt);
                } else {
                    fail_count++;
                    ESP_LOGW(TAG, "Frame invalid. OK=%lu FAIL=%lu 原始帧=",
                             (unsigned long)ok_count, (unsigned long)fail_count);
                    ESP_LOG_BUFFER_HEX_LEVEL(TAG, buf, FRAME_TOTAL_LEN, ESP_LOG_WARN);
                }
                state = 0;
                idx = 0;
            }
        }
    }
}

void app_main(void)
{
    /* NVS 初始化（WiFi 需要） */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 先初始化 LoRa 接收硬件 */
    a39c_mode_init();
    uart_init_a39c();

    /* 启动 WiFi STA（连接路由器） */
    wifi_init_sta();

    /* 启动 HTTP 服务器 */
    start_webserver();

    /* 启动 MQTT 连接 OneNet */
    mqtt_init();

    ESP_LOGI(TAG, "A39C RX start, MD0=%d MD1=%d AUX=%d",
             gpio_get_level(A39C_MD0_PIN),
             gpio_get_level(A39C_MD1_PIN),
             gpio_get_level(A39C_AUX_PIN));

    /* 在独立任务中运行 LoRa 接收循环 */
    ESP_LOGI(TAG, "空闲堆内存: %lu bytes", (unsigned long)esp_get_free_heap_size());
    xTaskCreate(lora_rx_task, "lora_rx", 8192, NULL, 10, NULL);
}
