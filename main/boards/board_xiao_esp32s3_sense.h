#pragma once
/* Seeed Studio XIAO ESP32S3 Sense + OV2640 đi kèm.
 * Thứ tự dữ liệu ESP-IDF: D0 = Y2 ... D7 = Y9. */
#define BOARD_NAME        "XIAO ESP32S3 Sense (OV2640)"

#define CAM_PIN_PWDN      -1
#define CAM_PIN_RESET     -1
#define CAM_PIN_XCLK      10
#define CAM_PIN_SIOD      40
#define CAM_PIN_SIOC      39
#define CAM_PIN_D0        15
#define CAM_PIN_D1        17
#define CAM_PIN_D2        18
#define CAM_PIN_D3        16
#define CAM_PIN_D4        14
#define CAM_PIN_D5        12
#define CAM_PIN_D6        11
#define CAM_PIN_D7        48
#define CAM_PIN_VSYNC     38
#define CAM_PIN_HREF      47
#define CAM_PIN_PCLK      13

/* 20 MHz cho FPS cao. Nếu ảnh lỗi/sọc, hạ xuống 10000000. */
#define CAM_XCLK_HZ       20000000

#define CAM_VFLIP         0
#define CAM_HMIRROR       0
