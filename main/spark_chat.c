#include "spark_chat.h"

#include "esp_crt_bundle.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"

#define TAG "SparkChat"

static void safe_strcpy(char *dst, size_t dst_len, const char *src) {
    if (dst == NULL || dst_len == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_len - 1);
    dst[dst_len - 1] = '\0';
}

static uint32_t history_total_len(const spark_chat_client_t *client) {
    uint32_t total = 0;
    for (uint8_t i = 0; i < client->history_len; i++) {
        total += (uint32_t)strlen(client->history[i].content);
    }
    return total;
}

void spark_chat_init(spark_chat_client_t *client, const spark_chat_config_t *cfg) {
    if (client == NULL) {
        return;
    }

    memset(client, 0, sizeof(*client));

    if (cfg != NULL) {
        client->cfg = *cfg;
    }

    if (client->cfg.model == NULL) {
        client->cfg.model = "lite";  // 默认使用Spark Lite模型
    }
    // user_id 保持 NULL 不设默认值（Ark 不需要此字段）
    if (client->cfg.search_mode == NULL) {
        client->cfg.search_mode = "deep";
    }
    if (client->cfg.timeout_ms <= 0) {
        client->cfg.timeout_ms = 30000;
    }

    client->cfg.stream = true;

    client->is_first_content = true;
    client->http_client = NULL;
    client->connection_active = false;
}

void spark_chat_set_callbacks(spark_chat_client_t *client, const spark_chat_callbacks_t *cb) {
    if (client == NULL) {
        return;
    }
    if (cb == NULL) {
        memset(&client->cb, 0, sizeof(client->cb));
        return;
    }
    client->cb = *cb;
}

bool spark_chat_add_message(spark_chat_client_t *client, const char *role, const char *content) {
    if (client == NULL || role == NULL || content == NULL || content[0] == '\0') {
        return false;
    }

    if (client->history_len >= SPARK_CHAT_MAX_CHAT_HISTORY) {
        for (uint8_t i = 0; i < SPARK_CHAT_MAX_CHAT_HISTORY - 1; i++) {
            memcpy(&client->history[i], &client->history[i + 1], sizeof(spark_chat_message_t));
        }
        client->history_len--;
        memset(&client->history[client->history_len], 0, sizeof(spark_chat_message_t));
    }

    safe_strcpy(client->history[client->history_len].role, sizeof(client->history[client->history_len].role), role);
    safe_strcpy(client->history[client->history_len].content, sizeof(client->history[client->history_len].content), content);
    client->history_len++;
    return true;
}

void spark_chat_clear_history(spark_chat_client_t *client) {
    if (client == NULL) {
        return;
    }
    // 清除所有对话历史
    for (uint8_t i = 0; i < SPARK_CHAT_MAX_CHAT_HISTORY; i++) {
        memset(&client->history[i], 0, sizeof(spark_chat_message_t));
    }
    client->history_len = 0;

}

void spark_chat_trim_history(spark_chat_client_t *client) {
    if (client == NULL) {
        return;
    }

    while (history_total_len(client) > SPARK_CHAT_MAX_TOTAL_CONTENT) {
        if (client->history_len <= 1) {
            break;
        }
        for (uint8_t i = 0; i < client->history_len - 1; i++) {
            memcpy(&client->history[i], &client->history[i + 1], sizeof(spark_chat_message_t));
        }
        client->history_len--;
        memset(&client->history[client->history_len], 0, sizeof(spark_chat_message_t));

    }
}

static bool build_request_json(const spark_chat_client_t *client, char *json_buf, size_t buf_len) {
    if (client == NULL || json_buf == NULL || buf_len == 0) {
        return false;
    }
    memset(json_buf, 0, buf_len);

    // 兼容：
    // - Chat Completions: {model, stream, messages:[{role, content}]}
    // - Ark Responses:    {model, stream, input:[{role, content:[{type:"input_text", text:"..."}]}]}
    bool use_ark_responses = false;
    if (client->cfg.url != NULL) {
        // 简单判断：URL 包含 /responses 则使用 Responses 请求格式
        use_ark_responses = (strstr(client->cfg.url, "/responses") != NULL);
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return false;
    }

    cJSON_AddStringToObject(root, "model", client->cfg.model);
    cJSON_AddBoolToObject(root, "stream", client->cfg.stream);

    if (use_ark_responses) {
        // Ark Responses 格式（官方文档）：
        // - system 消息 → instructions 字段
        // - 其余消息 → input 数组，每项必须含 type:"message"
        // - 关闭思考以加速响应
        cJSON *thinking = cJSON_CreateObject();
        cJSON_AddStringToObject(thinking, "type", "disabled");
        cJSON_AddItemToObject(root, "thinking", thinking);

        cJSON *input = cJSON_CreateArray();
        for (uint8_t i = 0; i < client->history_len; i++) {
            if (strcmp(client->history[i].role, "system") == 0) {
                // system 消息放到顶层 instructions 字段
                cJSON_AddStringToObject(root, "instructions", client->history[i].content);
            } else {
                cJSON *msg = cJSON_CreateObject();
                cJSON_AddStringToObject(msg, "type", "message");
                cJSON_AddStringToObject(msg, "role", client->history[i].role);
                // 简单文本用字符串格式（非数组），兼容性最好
                cJSON_AddStringToObject(msg, "content", client->history[i].content);
                cJSON_AddItemToArray(input, msg);
            }
        }
        cJSON_AddItemToObject(root, "input", input);
    } else {
        // Chat Completions messages 结构
        if (client->cfg.user_id != NULL) {
            cJSON_AddStringToObject(root, "user", client->cfg.user_id);
        }
        cJSON *messages = cJSON_CreateArray();
        for (uint8_t i = 0; i < client->history_len; i++) {
            cJSON *msg = cJSON_CreateObject();
            cJSON_AddStringToObject(msg, "role", client->history[i].role);
            cJSON_AddStringToObject(msg, "content", client->history[i].content);
            cJSON_AddItemToArray(messages, msg);
        }
        cJSON_AddItemToObject(root, "messages", messages);
    }

    if (!use_ark_responses && client->cfg.enable_web_search) {
        /* 豆包 Ark Chat Completions: web_search 作为顶层参数 */
        cJSON *web_search = cJSON_CreateObject();
        cJSON_AddBoolToObject(web_search, "enable", true);
        cJSON_AddStringToObject(web_search, "search_mode",
            client->cfg.search_mode != NULL ? client->cfg.search_mode : "auto");
        cJSON_AddItemToObject(root, "web_search", web_search);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str == NULL || strlen(json_str) >= buf_len) {
        cJSON_Delete(root);
        if (json_str != NULL) {
            free(json_str);
        }
        return false;
    }

    strcpy(json_buf, json_str);
    free(json_str);
    cJSON_Delete(root);
    return true;
}

// 思维链输出已禁用，节省内存
static void stream_reasoning(spark_chat_client_t *client, const char *text) {
    (void)client;
    (void)text;
    // 不输出思维链内容
}

static void stream_content(spark_chat_client_t *client, const char *text) {
    if (text == NULL || text[0] == '\0') {
        return;
    }

    if (client->is_first_content) {
        client->is_first_content = false;
    }

    if (client->cb.on_content != NULL) {
        client->cb.on_content(text, client->cb.user_ctx);
    } else {
        printf("%s", text);
        fflush(stdout);
    }

    strncat(client->full_response, text, SPARK_CHAT_MAX_FULL_RESPONSE - strlen(client->full_response) - 1);
}

/**
 * @brief 处理单个 SSE 数据行
 */
static void process_sse_line(spark_chat_client_t *client, const char *line) {
    if (line == NULL || line[0] == '\0') {
        return;
    }
    
    // 跳过空行
    if (line[0] == '\n' || line[0] == '\r') {
        return;
    }
    
    // 检查 [DONE] 标记
    if (strstr(line, "[DONE]") != NULL) {
        return;
    }
    
    // 解析 "data: {json}" 格式
    const char *json_start = line;
    if (strncmp(line, "data:", 5) == 0) {
        json_start = line + 5;
        while (*json_start == ' ') {
            json_start++;
        }
    }
    
    if (json_start[0] == '\0') {
        return;
    }
    
    cJSON *root = cJSON_Parse(json_start);
    if (root == NULL) {
        // JSON 解析失败，可能是不完整的数据
        return;
    }

    // OpenAI 兼容错误格式：{"error": {"message": "..."}}
    cJSON *error = cJSON_GetObjectItem(root, "error");
    if (error != NULL && cJSON_IsObject(error)) {
        cJSON *message = cJSON_GetObjectItem(error, "message");
        const char *msg = (message && cJSON_IsString(message)) ? message->valuestring : "未知错误";
        ESP_LOGE(TAG, "API错误: %s", msg);
        cJSON_Delete(root);
        return;
    }

    // Ark/OpenAI Responses 流式：data: {"type":"response.output_text.delta","delta":"..."}
    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (type != NULL && cJSON_IsString(type) && type->valuestring != NULL) {
        const char *t = type->valuestring;
        if (strstr(t, "output_text") != NULL) {
            cJSON *delta = cJSON_GetObjectItem(root, "delta");
            if (delta != NULL && cJSON_IsString(delta) && delta->valuestring != NULL) {
                stream_content(client, delta->valuestring);
                cJSON_Delete(root);
                return;
            }
            cJSON *text = cJSON_GetObjectItem(root, "text");
            if (text != NULL && cJSON_IsString(text) && text->valuestring != NULL) {
                stream_content(client, text->valuestring);
                cJSON_Delete(root);
                return;
            }
        }
        // 其他 type 事件（completed/created 等）忽略
    }
    
    // 检查是否是错误响应
    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (code != NULL && cJSON_IsNumber(code) && code->valueint != 0) {
        cJSON *message = cJSON_GetObjectItem(root, "message");
        const char *msg = (message && cJSON_IsString(message)) ? message->valuestring : "未知错误";
        ESP_LOGE(TAG, "API错误 code=%d: %s", code->valueint, msg);
        cJSON_Delete(root);
        return;
    }
    
    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    if (choices == NULL || !cJSON_IsArray(choices) || cJSON_GetArraySize(choices) == 0) {
        cJSON_Delete(root);
        return;
    }
    
    cJSON *first_choice = cJSON_GetArrayItem(choices, 0);
    cJSON *delta = cJSON_GetObjectItem(first_choice, "delta");
    if (delta == NULL) {
        cJSON_Delete(root);
        return;
    }
    
    cJSON *reasoning = cJSON_GetObjectItem(delta, "reasoning_content");
    if (reasoning != NULL && cJSON_IsString(reasoning) && reasoning->valuestring != NULL) {
        stream_reasoning(client, reasoning->valuestring);
    }
    
    cJSON *content = cJSON_GetObjectItem(delta, "content");
    if (content != NULL && cJSON_IsString(content) && content->valuestring != NULL) {
        stream_content(client, content->valuestring);
    }
    
    cJSON_Delete(root);
}

static esp_err_t spark_http_event_handler(esp_http_client_event_t *evt) {
    if (evt == NULL) {
        return ESP_OK;
    }

    spark_chat_client_t *client = (spark_chat_client_t *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA: {
            if (client == NULL || evt->data == NULL || evt->data_len <= 0) {
                break;
            }
            
            // 将新数据追加到 SSE 缓冲区
            size_t space_left = SPARK_CHAT_SSE_BUFFER_SIZE - client->sse_buffer_len - 1;
            size_t copy_len = (evt->data_len < space_left) ? evt->data_len : space_left;
            if (copy_len > 0) {
                memcpy(client->sse_buffer + client->sse_buffer_len, evt->data, copy_len);
                client->sse_buffer_len += copy_len;
                client->sse_buffer[client->sse_buffer_len] = '\0';
            }
            
            // 按行处理 SSE 数据（每行以 \n\n 或 \n 结束）
            char *line_start = client->sse_buffer;
            char *newline;
            
            while ((newline = strstr(line_start, "\n")) != NULL) {
                // 找到一行完整数据
                *newline = '\0';
                
                // 处理这一行
                if (strlen(line_start) > 0) {
                    process_sse_line(client, line_start);
                }
                
                // 移动到下一行
                line_start = newline + 1;
                // 跳过可能的额外换行符
                while (*line_start == '\n' || *line_start == '\r') {
                    line_start++;
                }
            }
            
            // 将未处理的数据移到缓冲区开头
            if (line_start != client->sse_buffer) {
                size_t remaining = strlen(line_start);
                if (remaining > 0) {
                    memmove(client->sse_buffer, line_start, remaining + 1);
                    client->sse_buffer_len = remaining;
                } else {
                    client->sse_buffer[0] = '\0';
                    client->sse_buffer_len = 0;
                }
            }
            break;
        }

        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP请求错误");
            break;

        case HTTP_EVENT_ON_CONNECTED:

            // 清空 SSE 缓冲区
            if (client != NULL) {
                client->sse_buffer[0] = '\0';
                client->sse_buffer_len = 0;
            }
            break;

        case HTTP_EVENT_DISCONNECTED:

            // 处理缓冲区中剩余的数据
            if (client != NULL && client->sse_buffer_len > 0) {
                process_sse_line(client, client->sse_buffer);
                client->sse_buffer[0] = '\0';
                client->sse_buffer_len = 0;
            }
            break;

        default:
            break;
    }

    return ESP_OK;
}

bool spark_chat_request(spark_chat_client_t *client) {
    if (client == NULL || client->cfg.url == NULL || client->cfg.api_key == NULL) {
        return false;
    }

    memset(client->full_response, 0, sizeof(client->full_response));
    client->is_first_content = true;

    char request_json[SPARK_CHAT_MAX_REQUEST_JSON] = {0};
    if (!build_request_json(client, request_json, sizeof(request_json))) {
        ESP_LOGE(TAG, "构建请求JSON失败");
        return false;
    }

    ESP_LOGD(TAG, "请求 %s (%d bytes)", client->cfg.url, strlen(request_json));

    // 如果没有活跃连接，创建新的 HTTP 客户端
    if (client->http_client == NULL) {
        uint32_t timeout = client->cfg.timeout_ms;
        if (timeout > 15000) timeout = 15000;  // 限制超时时间
        
        esp_http_client_config_t config = {
            .url = client->cfg.url,
            .event_handler = spark_http_event_handler,
            .user_data = client,
            .method = HTTP_METHOD_POST,
            .timeout_ms = timeout,
            // 使用证书包校验 HTTPS（同时也会确保设置 hostname/SNI）
            .crt_bundle_attach = esp_crt_bundle_attach,
            .cert_pem = NULL,
            .skip_cert_common_name_check = false,
            .use_global_ca_store = false,
            .buffer_size = 1024,
            .buffer_size_tx = 1024,
            .is_async = false,
            .keep_alive_enable = false,  // 禁用 keep-alive 减少复杂性
            .disable_auto_redirect = false,
        };

        client->http_client = esp_http_client_init(&config);
        if (client->http_client == NULL) {
            ESP_LOGE(TAG, "初始化HTTP客户端失败");
            return false;
        }
        
    }

    // 构建认证头（检查是否已包含 Bearer 前缀）
    char auth_header[512];
    if (strncmp(client->cfg.api_key, "Bearer ", 7) == 0) {
        // 已经包含 Bearer 前缀，直接使用
        snprintf(auth_header, sizeof(auth_header), "%s", client->cfg.api_key);
    } else {
        // 添加 Bearer 前缀
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", client->cfg.api_key);
    }
    
    esp_http_client_set_header(client->http_client, "Authorization", auth_header);
    esp_http_client_set_header(client->http_client, "Content-Type", "application/json");
    esp_http_client_set_header(client->http_client, "Accept", "text/event-stream");
    esp_http_client_set_header(client->http_client, "Connection", "close");
    esp_http_client_set_post_field(client->http_client, request_json, (int)strlen(request_json));

    esp_err_t err = esp_http_client_perform(client->http_client);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP请求失败: %s (0x%x)", esp_err_to_name(err), err);
        // 连接失败，清理并下次重新创建
        esp_http_client_cleanup(client->http_client);
        client->http_client = NULL;
        client->connection_active = false;
        return false;
    }

    int http_code = esp_http_client_get_status_code(client->http_client);
    
    if (http_code != 200) {
        ESP_LOGE(TAG, "HTTP响应错误，状态码: %d", http_code);
        // 错误响应，清理连接
        esp_http_client_cleanup(client->http_client);
        client->http_client = NULL;
        client->connection_active = false;
        return false;
    }

    client->connection_active = true;
    printf("\n");
    return true;
}

void spark_chat_close_connection(spark_chat_client_t *client) {
    if (client == NULL) {
        return;
    }
    
    if (client->http_client != NULL) {
        esp_http_client_cleanup(client->http_client);
        client->http_client = NULL;
    }
    client->connection_active = false;
}

const char *spark_chat_get_last_response(const spark_chat_client_t *client) {
    if (client == NULL) {
        return "";
    }
    return client->full_response;
}
