#include "camera.h"
#include "board.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "camera";
static bool running;

static const camera_config_t cfg = {
    .pin_pwdn     = CAM_PIN_PWDN,
    .pin_reset    = CAM_PIN_RESET,
    .pin_xclk     = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,
    .pin_d0 = CAM_PIN_D0, .pin_d1 = CAM_PIN_D1, .pin_d2 = CAM_PIN_D2, .pin_d3 = CAM_PIN_D3,
    .pin_d4 = CAM_PIN_D4, .pin_d5 = CAM_PIN_D5, .pin_d6 = CAM_PIN_D6, .pin_d7 = CAM_PIN_D7,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href  = CAM_PIN_HREF,
    .pin_pclk  = CAM_PIN_PCLK,

    .xclk_freq_hz = CAM_XCLK_HZ,
    .ledc_timer   = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG,
    .frame_size   = VIDEO_FRAME_SIZE,
    .jpeg_quality = VIDEO_JPEG_QUALITY,
    /* 2 buffer + GRAB_LATEST: camera chụp frame mới trong lúc frame cũ đang gửi,
     * và luôn trả frame mới nhất -> không dồn trễ khi Wi-Fi chậm. */
    .fb_count     = 2,
    .fb_location  = CAMERA_FB_IN_PSRAM,
    .grab_mode    = CAMERA_GRAB_LATEST,
};

static void release_pins(void)
{
    if (CAM_PIN_XCLK >= 0) {
        gpio_reset_pin(CAM_PIN_XCLK);
        gpio_set_direction(CAM_PIN_XCLK, GPIO_MODE_INPUT);
    }
    if (CAM_PIN_PWDN >= 0) { /* PWDN = 1: sensor vào chế độ nghỉ */
        gpio_reset_pin(CAM_PIN_PWDN);
        gpio_set_direction(CAM_PIN_PWDN, GPIO_MODE_OUTPUT);
        gpio_set_level(CAM_PIN_PWDN, 1);
    }
}

esp_err_t camera_start(void)
{
    if (running) return ESP_OK;
    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init lỗi: %s", esp_err_to_name(err));
        release_pins();
        return err;
    }
    running = true;

    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_vflip(s, CAM_VFLIP);
        s->set_hmirror(s, CAM_HMIRROR);
    }
    /* Bỏ vài frame đầu để cân bằng sáng ổn định. */
    for (int i = 0; i < 3; ++i) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) esp_camera_fb_return(fb);
    }
    ESP_LOGI(TAG, "Camera bật (%s, PID 0x%x)", BOARD_NAME, s ? s->id.PID : 0);
    return ESP_OK;
}

void camera_stop(void)
{
    if (!running) return;
    sensor_t *s = esp_camera_sensor_get();
    if (s && s->id.PID == OV5640_PID && s->set_reg)
        s->set_reg(s, 0x3008, 0xff, 0x42); /* OV5640 software standby */
    esp_camera_deinit();
    release_pins();
    running = false;
    ESP_LOGI(TAG, "Camera tắt (không có người xem)");
}

bool camera_running(void) { return running; }

camera_fb_t *camera_grab(void) { return running ? esp_camera_fb_get() : NULL; }

void camera_release(camera_fb_t *fb) { if (fb) esp_camera_fb_return(fb); }
