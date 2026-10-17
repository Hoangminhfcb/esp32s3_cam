#pragma once

#include <stdbool.h>
#include "esp_camera.h"

/* A session owns the camera until end(). A returned frame remains borrowed
 * until release(), including on network error. Callbacks serialize hardware. */
typedef struct {
    bool (*begin)(void);
    camera_fb_t *(*take)(void);
    void (*release)(camera_fb_t *frame);
    void (*end)(void);
} fast_stream_camera_t;

void fast_stream_start(const fast_stream_camera_t *camera);
