/**
 * @file baidu_tts.h
 * @brief 百度TTS语音合成驱动
 * 
 * 功能：
 * - 支持在线TTS语音合成
 * - 支持多种音色和语速配置
 * - 自动处理音频格式转换
 * - 支持流式播放
 */

#ifndef BAIDU_TTS_H
#define BAIDU_TTS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "max98357a.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 百度TTS音色类型
 * 参考: https://ai.baidu.com/ai-doc/SPEECH/Rluv3uq3d
 */
typedef enum {
    // 基础音库
    BAIDU_TTS_VOICE_XIAOMEI = 0,     /*!< 度小美-标准女主播 */
    BAIDU_TTS_VOICE_XIAOYU = 1,      /*!< 度小宇-亲切男声 */
    BAIDU_TTS_VOICE_XIAOYAO = 3,     /*!< 度這遥-情感男声 */
    BAIDU_TTS_VOICE_YAYA = 4,        /*!< 度丫丫-童声 */
    
    // 精品音库（直接调用，无需额外开通）
    BAIDU_TTS_VOICE_XIAOJIAO = 5,    /*!< 度小娇-成熟女主播 */
    BAIDU_TTS_VOICE_XIAOLU = 5118,   /*!< 度小鹿-甜美女声（对话助手） */
    BAIDU_TTS_VOICE_BOWEN = 106,     /*!< 度博文-专业男主播 */
    BAIDU_TTS_VOICE_MIDUO = 103,     /*!< 度米朵-可爱童声 */
    BAIDU_TTS_VOICE_XIAOTONG = 110,  /*!< 度小童-童声主播 */
    BAIDU_TTS_VOICE_XIAOMENG = 111,  /*!< 度小萌-软萌妹子 */
    BAIDU_TTS_VOICE_XIAOYAO_JP = 5003, /*!< 度這遥-情感男声(精品) */
    
    // 臻品音库
    BAIDU_TTS_VOICE_XIAOLU_ZP = 4119, /*!< 度小鹿-甜美女声(臻品) */
    BAIDU_TTS_VOICE_LINGER = 4105,   /*!< 度灵儿-清激女声 */
    BAIDU_TTS_VOICE_XIAOQIAO = 4117, /*!< 度小乔-活泼女声 */
    BAIDU_TTS_VOICE_XIAOXIA = 4148,  /*!< 度小夏-甜美女声 */
    BAIDU_TTS_VOICE_WANWAN = 4141,   /*!< 度婉婉-甜美女声 */
    
    // 大模型音库（超拟人多情感，需要开通权限）
    BAIDU_TTS_VOICE_QINGYING = 4196, /*!< 度清影-甜美女声(超拟人多情感) */
    BAIDU_TTS_VOICE_HANZHU = 4189,   /*!< 度涵竹-开朗女声(超拟人多情感) */
    BAIDU_TTS_VOICE_YANRAN = 4194,   /*!< 度嫣然-活泼女声(超拟人多情感) */
    BAIDU_TTS_VOICE_ZEYAN = 4193,    /*!< 度泽言-开朗男声(超拟人多情感) */
    BAIDU_TTS_VOICE_HUAIAN = 4195,   /*!< 度怀安-磁性男声(超拟人多情感) */
    
    // 兼容旧名称
    BAIDU_TTS_VOICE_FEMALE = 0,
    BAIDU_TTS_VOICE_MALE = 1,
} baidu_tts_voice_t;

/**
 * @brief 百度TTS配置结构体
 */
typedef struct {
    const char *api_key;             /*!< 百度API Key */
    const char *secret_key;          /*!< 百度Secret Key */
    baidu_tts_voice_t voice;         /*!< 音色选择 */
    uint8_t speed;                   /*!< 语速(0-15)，默认5 */
    uint8_t pitch;                   /*!< 音调(0-15)，默认5 */
    uint8_t volume;                  /*!< 音量(0-15)，默认5 */
    uint32_t timeout_ms;             /*!< 请求超时时间(毫秒) */
} baidu_tts_config_t;

/**
 * @brief 百度TTS客户端句柄
 */
typedef struct {
    baidu_tts_config_t config;       /*!< TTS配置 */
    char api_key[64];                /*!< API Key副本 */
    char secret_key[64];             /*!< Secret Key副本 */
    char access_token[512];          /*!< 访问令牌 */
    int64_t token_expire_time;       /*!< 令牌过期时间(微秒) */
    max98357a_handle_t *audio_handle; /*!< 音频输出句柄 */
} baidu_tts_handle_t;

/**
 * @brief 初始化百度TTS
 * 
 * @param handle TTS句柄指针
 * @param config TTS配置
 * @param audio_handle 音频输出句柄
 * @return 
 *     - ESP_OK: 成功
 *     - ESP_ERR_INVALID_ARG: 参数错误
 *     - ESP_ERR_NO_MEM: 内存不足
 */
esp_err_t baidu_tts_init(baidu_tts_handle_t *handle, 
                         const baidu_tts_config_t *config,
                         max98357a_handle_t *audio_handle);

/**
 * @brief 获取访问令牌
 * 
 * @param handle TTS句柄
 * @return 
 *     - ESP_OK: 成功
 *     - ESP_FAIL: 失败
 */
esp_err_t baidu_tts_get_token(baidu_tts_handle_t *handle);

/**
 * @brief 文本转语音并播放
 * 
 * @param handle TTS句柄
 * @param text 要合成的文本
 * @return 
 *     - ESP_OK: 成功
 *     - ESP_ERR_INVALID_ARG: 参数错误
 *     - ESP_FAIL: 合成或播放失败
 */
esp_err_t baidu_tts_speak(baidu_tts_handle_t *handle, const char *text);

/**
 * @brief 仅下载TTS音频数据（不播放）
 * 
 * @param handle TTS句柄
 * @param text 要合成的文本
 * @param out_buffer 输出缓冲区指针（由函数分配，调用者负责释放）
 * @param out_size 输出音频数据大小
 * @return 
 *     - ESP_OK: 成功
 *     - ESP_FAIL: 失败
 */
esp_err_t baidu_tts_synthesize(baidu_tts_handle_t *handle, const char *text,
                                int16_t **out_buffer, size_t *out_size);

/**
 * @brief 播放已下载的音频数据
 * 
 * @param handle TTS句柄
 * @param stereo_buffer 立体声音频缓冲区
 * @param size 缓冲区大小（字节）
 * @return 
 *     - ESP_OK: 成功
 *     - ESP_FAIL: 失败
 */
esp_err_t baidu_tts_play_buffer(baidu_tts_handle_t *handle, 
                                 const int16_t *stereo_buffer, size_t size);

/**
 * @brief 停止当前播放
 * 
 * @param handle TTS句柄
 * @return 
 *     - ESP_OK: 成功
 */
esp_err_t baidu_tts_stop(baidu_tts_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif /* BAIDU_TTS_H */