#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_camera.h"
#include "sensor.h"
#include "driver/gpio.h"

static const char *TAG = "ESP32S3_CAM_STREAM";

// --- WI-FI CONFIG ---
#define WIFI_SSID       "Kien"
#define WIFI_PASS       "hoithangtuan"

// --- PINOUT OV5640 ---
#define CAM_PIN_PWDN    38
#define CAM_PIN_RESET   -1
#define CAM_PIN_VSYNC   6
#define CAM_PIN_HREF    7
#define CAM_PIN_PCLK    13
#define CAM_PIN_XCLK    15
#define CAM_PIN_SIOD    4
#define CAM_PIN_SIOC    5
#define CAM_PIN_D0      11
#define CAM_PIN_D1      9
#define CAM_PIN_D2      8
#define CAM_PIN_D3      10
#define CAM_PIN_D4      12
#define CAM_PIN_D5      18
#define CAM_PIN_D6      17
#define CAM_PIN_D7      16

// Cấu hình Header chuẩn cho luồng MJPEG Stream
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static SemaphoreHandle_t cam_mutex = NULL;
static bool is_camera_initialized = false;

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,
    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,
    
    .xclk_freq_hz = 20000000,           // Tăng lên 20MHz để FPS mượt hơn
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_VGA,        // 640x480 (Có thể đổi sang FRAMESIZE_QVGA 320x240 để tăng FPS)
    .jpeg_quality = 12,                 // 1-63: Số nhỏ hơn = chất lượng cao hơn nhưng tốn dung lượng
    .fb_count = 2,                      // Sử dụng Double Buffer để tăng tốc độ truyền khung hình
    .grab_mode = CAMERA_GRAB_LATEST
};

// --- HÀM TẮT CAMERA TRIỆT ĐỂ (HẠ NHIỆT) ---
void ov5640_power_down(void)
{
    if (!is_camera_initialized) return;

    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL && s->id.PID == OV5640_PID) {
        s->set_reg(s, 0x3008, 0xFF, 0x42); // Software Standby
    }

    esp_camera_deinit();

    gpio_reset_pin(CAM_PIN_XCLK);
    gpio_set_direction(CAM_PIN_XCLK, GPIO_MODE_INPUT);

    gpio_reset_pin(CAM_PIN_PWDN);
    gpio_set_direction(CAM_PIN_PWDN, GPIO_MODE_OUTPUT);
    gpio_set_level(CAM_PIN_PWDN, 1); // Cắt nguồn phần cứng

    gpio_reset_pin(CAM_PIN_SIOD);
    gpio_reset_pin(CAM_PIN_SIOC);

    is_camera_initialized = false;
    ESP_LOGI(TAG, "Camera đã dừng Stream và được ngắt nguồn hoàn toàn.");
}

// --- HÀM BẬT CAMERA ---
esp_err_t ov5640_power_up(void)
{
    if (is_camera_initialized) return ESP_OK;

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Khởi tạo Camera thất bại: 0x%x", err);
        ov5640_power_down();
        return err;
    }

    // Bỏ qua các khung hình ban đầu để AEC/AGC cân bằng ánh sáng
    for (int i = 0; i < 3; i++) {
        camera_fb_t *dummy_fb = esp_camera_fb_get();
        if (dummy_fb) {
            esp_camera_fb_return(dummy_fb);
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    is_camera_initialized = true;
    ESP_LOGI(TAG, "Camera đã được bật thành công.");
    return ESP_OK;
}

// --- GIAO DIỆN WEB ---
static const char INDEX_HTML[] = 
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<title>ESP32-S3 Video Stream</title>"
    "<style>"
    "  body { font-family: Arial, sans-serif; text-align: center; margin: 0; padding: 20px; background: #121212; color: #fff; }"
    "  h2 { color: #00e676; margin-bottom: 10px; }"
    "  .container { display: inline-block; background: #1e1e1e; padding: 15px; border-radius: 12px; box-shadow: 0 4px 20px rgba(0,0,0,0.8); }"
    "  img { max-width: 100%; height: auto; border-radius: 8px; background: #000; min-width: 320px; min-height: 240px; }"
    "  .status { margin-top: 12px; font-size: 14px; color: #b0bec5; }"
    "</style></head><body>"
    "  <div class='container'>"
    "    <h2>ESP32-S3 Real-Time Video</h2>"
    "    <img src='/stream' alt='Video Stream Unavailable'>"
    "    <div class='status'>Đang phát video trực tiếp. Đóng tab để tắt Camera.</div>"
    "  </div>"
    "</body></html>";

static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

// --- HTTP HANDLER: PHÁT VIDEO STREAM (MJPEG) ---
static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    char part_buf[64];

    if (xSemaphoreTake(cam_mutex, pdMS_TO_TICKS(3000)) != pdTRUE) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // 1. Kích hoạt camera khi có Client truy cập link Stream
    if (ov5640_power_up() != ESP_OK) {
        xSemaphoreGive(cam_mutex);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // 2. Thiết lập Header dạng multipart cho Client
    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        xSemaphoreGive(cam_mutex);
        return res;
    }

    ESP_LOGI(TAG, "Bắt đầu truyền Video Stream...");

    // 3. Vòng lặp gửi khung hình liên tục (Tạo Video)
    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Lỗi lấy khung hình!");
            res = ESP_FAIL;
            break;
        }

        // Gửi Boundary
        res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        if (res != ESP_OK) {
            esp_camera_fb_return(fb);
            break;
        }

        // Gửi Header từng khung ảnh JPEG
        size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, fb->len);
        res = httpd_resp_send_chunk(req, part_buf, hlen);
        if (res != ESP_OK) {
            esp_camera_fb_return(fb);
            break;
        }

        // Gửi dữ liệu ảnh JPEG
        res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        esp_camera_fb_return(fb);
        if (res != ESP_OK) {
            break; // Ngắt vòng lặp khi Người dùng đóng trình duyệt / mất kết nối
        }
    }

    // 4. Ngắt kết nối & Tắt Camera để hạ nhiệt
    ESP_LOGI(TAG, "Client đã ngắt kết nối Stream.");
    ov5640_power_down();
    xSemaphoreGive(cam_mutex);

    return res;
}

static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 8192;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler };
        httpd_register_uri_handler(server, &index_uri);

        httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler };
        httpd_register_uri_handler(server, &stream_uri);
    }
    return server;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "==========================================");
        ESP_LOGI(TAG, "Wi-Fi Connected! Xem Video tại: http://" IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "==========================================");
        start_webserver();
    }
}

void wifi_init_sta(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

void app_main(void) {
    cam_mutex = xSemaphoreCreateMutex();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Tắt camera ngay khi khởi động
    ov5640_power_down();

    // Khởi chạy Wi-Fi
    wifi_init_sta();
}