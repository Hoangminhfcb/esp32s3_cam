#include "fast_stream.h"
#include "link_config.h"
#include "video_protocol.h"
#include <errno.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "lwip/tcp.h"

static const char *TAG = "FAST_CAM";
static fast_stream_camera_t camera;

static bool send_all(int fd, const void *data, size_t bytes, int flags)
{
    const uint8_t *p = data;
    while (bytes) {
        int n = send(fd, p, bytes, flags);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        p += n;
        bytes -= n;
    }
    return true;
}

static void serve_viewer(int fd)
{
    int enabled = 1;
    struct timeval timeout = {.tv_sec = 2};
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled)) < 0 ||
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0 ||
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) return;

    /* Wait for a real request before powering up the sensor. */
    uint8_t request;
    if (recv(fd, &request, 1, 0) != 1 || request != VIDEO_REQUEST_NEXT || !camera.begin()) return;
    uint32_t sequence = 0, frames = 0;
    uint64_t bytes = 0, capture_us = 0, send_us = 0;
    int64_t window_start = esp_timer_get_time();
    do {
        int64_t capture_start = esp_timer_get_time();
        camera_fb_t *frame = camera.take();
        if (!frame) break;
        int64_t now = esp_timer_get_time();
        capture_us += now - capture_start;
        bool valid = frame->format == PIXFORMAT_JPEG && frame->len >= 4 &&
            frame->len <= VIDEO_WIRE_MAX_JPEG && frame->width == VIDEO_WIRE_WIDTH && frame->height == VIDEO_WIRE_HEIGHT;
        int64_t captured_at = (int64_t)frame->timestamp.tv_sec * 1000000 + frame->timestamp.tv_usec;
        int64_t age = now - captured_at;
        video_header_t info = {.jpeg_bytes = frame->len, .sequence = sequence++,
            .capture_age_us = age > UINT32_MAX ? UINT32_MAX : (age > 0 ? (uint32_t)age : 0),
            .width = frame->width, .height = frame->height};
        uint8_t header[VIDEO_HEADER_BYTES];
        video_header_encode(header, &info);
        /* The JPEG is sent directly from the camera frame buffer. */
        bool sent = valid && send_all(fd, header, sizeof(header), 0) &&
                    send_all(fd, frame->buf, frame->len, 0);
        bytes += frame->len;
        camera.release(frame); /* also release on partial/failed send */
        send_us += esp_timer_get_time() - now;
        if (!sent) break;
        ++frames;
        int64_t elapsed = esp_timer_get_time() - window_start;
        if (elapsed >= 3000000) {
            ESP_LOGI(TAG, "send=%.1f fps jpeg=%" PRIu64 " B capture_wait=%.1f ms send=%.1f ms",
                frames * 1000000.0 / elapsed, bytes / frames,
                capture_us / (1000.0 * frames), send_us / (1000.0 * frames));
            window_start = esp_timer_get_time();
            frames = 0; bytes = capture_us = send_us = 0;
        }
        /* Blocking for the next request yields CPU without a per-frame sleep. */
    } while (recv(fd, &request, 1, 0) == 1 && request == VIDEO_REQUEST_NEXT);
    camera.end();
}

static void server_task(void *arg)
{
    for (;;) {
        int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server < 0) { vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
        int enabled = 1;
        setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
        struct sockaddr_in address = {.sin_family = AF_INET, .sin_port = htons(LINK_VIDEO_PORT),
                                      .sin_addr.s_addr = htonl(INADDR_ANY)};
        if (bind(server, (struct sockaddr *)&address, sizeof(address)) < 0 || listen(server, 1) < 0) {
            close(server);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        ESP_LOGI(TAG, "Binary video listening on port %d", LINK_VIDEO_PORT);
        for (;;) {
            int client = accept(server, NULL, NULL);
            if (client < 0) break;
            serve_viewer(client);
            shutdown(client, SHUT_RDWR);
            close(client);
        }
        close(server);
    }
}

void fast_stream_start(const fast_stream_camera_t *callbacks)
{
    camera = *callbacks;
    BaseType_t ok = xTaskCreatePinnedToCore(server_task, "fast_video", 6144, NULL, 5, NULL, 1);
    ESP_ERROR_CHECK(ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
