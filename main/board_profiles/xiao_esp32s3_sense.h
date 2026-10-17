#pragma once

/* Seeed Studio XIAO ESP32S3 Sense with its supplied OV2640 camera module.
 * Pin names use ESP-IDF data order: D0=Y2 through D7=Y9. */
#define CAMERA_BOARD_NAME       "XIAO ESP32S3 Sense"
#define CAMERA_SENSOR_NAME      "OV2640"

#define CAMERA_PIN_PWDN         -1
#define CAMERA_PIN_RESET        -1
#define CAMERA_PIN_XCLK         10
#define CAMERA_PIN_SIOD         40
#define CAMERA_PIN_SIOC         39
#define CAMERA_PIN_D0           15
#define CAMERA_PIN_D1           17
#define CAMERA_PIN_D2           18
#define CAMERA_PIN_D3           16
#define CAMERA_PIN_D4           14
#define CAMERA_PIN_D5           12
#define CAMERA_PIN_D6           11
#define CAMERA_PIN_D7           48
#define CAMERA_PIN_VSYNC        38
#define CAMERA_PIN_HREF         47
#define CAMERA_PIN_PCLK         13

#define CAMERA_XCLK_HZ          10000000
/* Guition displays 320x240. Sending the same native resolution removes the
 * costly VGA decode/downscale step and reduces Wi-Fi traffic substantially. */
#define CAMERA_FRAME_SIZE       FRAMESIZE_QVGA
#define CAMERA_JPEG_QUALITY     14
/* XIAO Sense has PSRAM: capture while the preceding JPEG is sent. */
#define CAMERA_FRAME_BUFFERS    1
#define CAMERA_GRAB_MODE        CAMERA_GRAB_WHEN_EMPTY
