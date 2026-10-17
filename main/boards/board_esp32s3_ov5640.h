#pragma once
/* ESP32-S3 + OV5640 (chân lấy từ cấu hình gốc của project).
 * Thứ tự dữ liệu ESP-IDF: D0 = Y2 ... D7 = Y9. */
#define BOARD_NAME        "ESP32-S3 + OV5640"

#define CAM_PIN_PWDN      38
#define CAM_PIN_RESET     -1
#define CAM_PIN_XCLK      15
#define CAM_PIN_SIOD      4
#define CAM_PIN_SIOC      5
#define CAM_PIN_D0        11
#define CAM_PIN_D1        9
#define CAM_PIN_D2        8
#define CAM_PIN_D3        10
#define CAM_PIN_D4        12
#define CAM_PIN_D5        18
#define CAM_PIN_D6        17
#define CAM_PIN_D7        16
#define CAM_PIN_VSYNC     6
#define CAM_PIN_HREF      7
#define CAM_PIN_PCLK      13

/* 20 MHz cho FPS cao. Nếu ảnh lỗi/sọc, hạ xuống 10000000. */
#define CAM_XCLK_HZ       20000000

/* Lật ảnh nếu camera lắp ngược (0/1). */
#define CAM_VFLIP         0
#define CAM_HMIRROR       0
