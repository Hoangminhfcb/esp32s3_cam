/*
 * ESP32-S3 camera -> Wi-Fi nhà -> MJPEG stream.
 *   Trang xem : http://<ip>/            (hoặc http://esp32cam.local)
 *   Luồng     : http://<ip>:81/stream   (VLC, OBS, Home Assistant, trình duyệt)
 * Cấu hình   : main/config.h
 */
#include "nvs_flash.h"
#include "esp_log.h"
#include "board.h"
#include "camera.h"
#include "wifi.h"
#include "stream.h"

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_LOGI("main", "Board: %s", BOARD_NAME);

    /* Thử camera một lần lúc khởi động để báo lỗi dây/chân sớm. */
    if (camera_start() != ESP_OK)
        ESP_LOGE("main", "Camera lỗi — kiểm tra board đã chọn trong config.h và dây cáp");
    if (CAMERA_IDLE_OFF_MS > 0) camera_stop();

    wifi_start();
    web_server_start();
    stream_server_start();
}
