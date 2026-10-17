#pragma once
#include <stdbool.h>
#include "esp_camera.h"

/* Chỉ task stream gọi các hàm này, nên không cần mutex. */
esp_err_t   camera_start(void);   /* bật camera nếu đang tắt */
void        camera_stop(void);    /* tắt camera + đưa sensor về standby */
bool        camera_running(void);
camera_fb_t *camera_grab(void);
void        camera_release(camera_fb_t *fb);
