#pragma once

#include "esp_camera.h"

/*
 * Hardware profile selector.
 * To support another camera board, add a profile header in board_profiles/ and
 * change only this include. Application and Wi-Fi code stay untouched.
 */
#include "board_profiles/xiao_esp32s3_sense.h"

static inline camera_config_t camera_board_make_config(void)
{
    return (camera_config_t){
        .pin_pwdn = CAMERA_PIN_PWDN,
        .pin_reset = CAMERA_PIN_RESET,
        .pin_xclk = CAMERA_PIN_XCLK,
        .pin_sscb_sda = CAMERA_PIN_SIOD,
        .pin_sscb_scl = CAMERA_PIN_SIOC,
        .pin_d0 = CAMERA_PIN_D0,
        .pin_d1 = CAMERA_PIN_D1,
        .pin_d2 = CAMERA_PIN_D2,
        .pin_d3 = CAMERA_PIN_D3,
        .pin_d4 = CAMERA_PIN_D4,
        .pin_d5 = CAMERA_PIN_D5,
        .pin_d6 = CAMERA_PIN_D6,
        .pin_d7 = CAMERA_PIN_D7,
        .pin_vsync = CAMERA_PIN_VSYNC,
        .pin_href = CAMERA_PIN_HREF,
        .pin_pclk = CAMERA_PIN_PCLK,
        .xclk_freq_hz = CAMERA_XCLK_HZ,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = CAMERA_FRAME_SIZE,
        .jpeg_quality = CAMERA_JPEG_QUALITY,
        .fb_count = CAMERA_FRAME_BUFFERS,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_MODE,
    };
}
