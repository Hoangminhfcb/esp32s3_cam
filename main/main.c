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

static const char *TAG = "ON_DEMAND_CAM";

// --- WI-FI CONFIG ---
#define WIFI_SSID      "Kien"
#define WIFI_PASS      "hoithangtuan"

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

static SemaphoreHandle_t cam_mutex = NULL;

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
    
    .xclk_freq_hz = 10000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_VGA,    // 640x480
    .jpeg_quality = 12,
    .fb_count = 1,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY
};

// --- HÀM TẮT CAMERA TRIỆT ĐỂ (HẠ NHIỆT) ---
void ov5640_power_down(void)
{
    // 1. Gửi lệnh Software Standby tới OV5640 qua thanh ghi 0x3008 nếu camera đang chạy
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL && s->id.PID == OV5640_PID) {
        s->set_reg(s, 0x3008, 0xFF, 0x42);
    }

    // 2. Tắt phần mềm driver trong ESP-IDF
    esp_camera_deinit();

    // 3. Ngắt hoàn toàn xung XCLK (GPIO 15)
    gpio_reset_pin(CAM_PIN_XCLK);
    gpio_set_direction(CAM_PIN_XCLK, GPIO_MODE_INPUT);

    // 4. Ép PWDN (GPIO 38) lên HIGH (1) để cắt nguồn phần cứng cảm biến
    gpio_reset_pin(CAM_PIN_PWDN);
    gpio_set_direction(CAM_PIN_PWDN, GPIO_MODE_OUTPUT);
    gpio_set_level(CAM_PIN_PWDN, 1);

    // 5. Ngắt bus I2C để tránh rò dòng
    gpio_reset_pin(CAM_PIN_SIOD);
    gpio_reset_pin(CAM_PIN_SIOC);

    ESP_LOGI(TAG, "Camera đã được ngắt hoàn toàn. Trạng thái: NGHỈ MÁT.");
}

// --- GIAO DIỆN HTML WEB ---
static const char INDEX_HTML[] = 
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<title>ESP32-S3 Cool Camera</title>"
    "<style>"
    "  body { font-family: Arial; text-align: center; margin-top: 30px; background: #181818; color: #fff; }"
    "  img { max-width: 90%; border-radius: 8px; box-shadow: 0 4px 12px rgba(0,0,0,0.6); margin-top: 15px; min-height: 200px; background: #2a2a2a; }"
    "  button { padding: 14px 28px; font-size: 16px; font-weight: bold; background: #28a745; color: white; border: none; border-radius: 6px; cursor: pointer; transition: 0.2s; }"
    "  button:hover { background: #218838; }"
    "  button:disabled { background: #6c757d; cursor: not-allowed; }"
    "  #status { margin-top: 10px; color: #aaa; font-style: italic; }"
    "</style></head><body>"
    "<h2>ESP32-S3 On-Demand Camera Viewer</h2>"
    "<button id='btn' onclick='takePhoto()'>Chụp Ảnh (Kích hoạt Cam)</button>"
    "<div id='status'>Trạng thái: Camera đang TẮT (Mát)</div>"
    "<br><img id='photo' src='' alt='Bấm Chụp Ảnh để kích hoạt camera'><script>"
    "function takePhoto() {"
    "  var btn = document.getElementById('btn');"
    "  var status = document.getElementById('status');"
    "  btn.disabled = true;"
    "  status.innerText = 'Đang bật Camera & Cân bằng ánh sáng...';"
    "  var img = document.getElementById('photo');"
    "  img.src = '/capture?t=' + new Date().getTime();"
    "  img.onload = function() {"
    "    btn.disabled = false;"
    "    status.innerText = 'Đã chụp xong! Camera đã TẮT trở lại.';"
    "  };"
    "  img.onerror = function() {"
    "    btn.disabled = false;"
    "    status.innerText = 'Lỗi chụp ảnh!';"
    "  };"
    "}"
    "</script></body></html>";

static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

// --- HTTP HANDLER: BẬT CAM -> CHỤP -> TẮT CAM ---
static esp_err_t capture_handler(httpd_req_t *req) {
    // Tránh việc bấm chụp dồn dập làm chập luồng
    if (xSemaphoreTake(cam_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Nhận yêu cầu chụp: Đang BẬT Camera...");

    // 1. Kích hoạt/Khởi tạo lại Camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Bật Camera thất bại: 0x%x", err);
        ov5640_power_down();
        xSemaphoreGive(cam_mutex);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // 2. Bỏ qua 2 khung hình đầu tiên để thuật toán Phơi sáng tự động (AEC) của OV5640 kịp cân bằng
    for (int i = 0; i < 2; i++) {
        camera_fb_t *dummy_fb = esp_camera_fb_get();
        if (dummy_fb) {
            esp_camera_fb_return(dummy_fb);
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }

    // 3. Chụp bức ảnh chính thức
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Không lấy được khung hình!");
        ov5640_power_down();
        xSemaphoreGive(cam_mutex);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // 4. Gửi mảng ảnh JPEG về cho Trình duyệt
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);

    // Trả lại buffer
    esp_camera_fb_return(fb);

    // 5. TẮT CẢM BIẾN LẬP TỨC ĐỂ TRÁNH TỎA NHIỆT
    ov5640_power_down();

    xSemaphoreGive(cam_mutex);
    return res;
}

static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler };
        httpd_register_uri_handler(server, &index_uri);

        httpd_uri_t capture_uri = { .uri = "/capture", .method = HTTP_GET, .handler = capture_handler };
        httpd_register_uri_handler(server, &capture_uri);
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
        ESP_LOGI(TAG, "Đã kết nối Wi-Fi! IP: http://" IPSTR, IP2STR(&event->ip_info.ip));
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
    // Tạo Mutex chống xung đột bộ nhớ
    cam_mutex = xSemaphoreCreateMutex();

    // 1. Khởi tạo Flash NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Ép Camera ngắt nguồn ngay khi vừa bật nguồn ESP32-S3 (Để đảm bảo mát ngay từ đầu)
    ov5640_power_down();

    // 3. Khởi chạy Wi-Fi
    wifi_init_sta();
}