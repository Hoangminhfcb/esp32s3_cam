/*
 * MJPEG stream qua socket TCP thô (không chunked encoding của httpd) để giảm
 * overhead và độ trễ. Một người xem tại một thời điểm: người kết nối sau sẽ
 * "chiếm" luồng, nên tải lại trang không bao giờ bị báo bận.
 */
#include "stream.h"
#include "config.h"
#include "camera.h"
#include "wifi.h"
#include <string.h>
#include <errno.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_http_server.h"
#include "lwip/sockets.h"

static const char *TAG = "stream";

#define BOUNDARY "frame"
static const char STREAM_HEADER[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: multipart/x-mixed-replace;boundary=" BOUNDARY "\r\n"
    "Cache-Control: no-cache, no-store\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Connection: close\r\n\r\n";

/* ---------------- socket helpers ---------------- */

static bool send_all(int fd, const void *data, size_t len)
{
    const uint8_t *p = data;
    while (len) {
        int n = send(fd, p, len, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        p += n;
        len -= n;
    }
    return true;
}

/* Đọc request HTTP, trả về true nếu là GET hợp lệ và đã gửi header stream. */
static bool accept_viewer(int fd)
{
    struct timeval tv = {.tv_sec = 2};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    tv.tv_sec = 3;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)); /* frame gửi đi ngay */

    char req[512];
    size_t got = 0;
    while (got < sizeof(req) - 1) {
        int n = recv(fd, req + got, sizeof(req) - 1 - got, 0);
        if (n <= 0) break;
        got += n;
        req[got] = 0;
        if (strstr(req, "\r\n\r\n")) break;
    }
    req[got] = 0;
    if (strncmp(req, "GET /", 5) != 0) return false;
    return send_all(fd, STREAM_HEADER, sizeof(STREAM_HEADER) - 1);
}

/* Có kết nối mới đang chờ trên listen socket? (không chặn) */
static bool pending(int listen_fd, int timeout_ms)
{
    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(listen_fd, &rd);
    struct timeval tv = {.tv_sec = timeout_ms / 1000, .tv_usec = (timeout_ms % 1000) * 1000};
    return select(listen_fd + 1, &rd, NULL, NULL, &tv) > 0;
}

static int open_listener(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return -1;
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(STREAM_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(fd, 2) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void drop(int *fd)
{
    if (*fd >= 0) { shutdown(*fd, SHUT_RDWR); close(*fd); *fd = -1; }
}

/* ---------------- stream task ---------------- */

static void stream_task(void *arg)
{
    int lfd;
    while ((lfd = open_listener()) < 0) vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "MJPEG sẵn sàng trên cổng %d", STREAM_PORT);

    int client = -1;
    int64_t idle_since = esp_timer_get_time();
    int fails = 0;
    uint32_t frames = 0;
    uint64_t bytes = 0;
    int64_t stat_start = 0;

    for (;;) {
        bool incoming = pending(lfd, client < 0 ? 500 : 0);

        /* ---- Chưa có người xem: chờ kết nối, tắt camera nếu rảnh lâu ---- */
        if (client < 0 && !incoming) {
            if (CAMERA_IDLE_OFF_MS > 0 && camera_running() &&
                esp_timer_get_time() - idle_since > (int64_t)CAMERA_IDLE_OFF_MS * 1000)
                camera_stop();
            continue;
        }

        /* ---- Có kết nối mới: người xem mới thay người cũ ---- */
        if (incoming) {
            int fd = accept(lfd, NULL, NULL);
            if (fd >= 0 && accept_viewer(fd) && camera_start() == ESP_OK) {
                if (client >= 0) ESP_LOGI(TAG, "Người xem mới, ngắt người xem cũ");
                drop(&client);
                client = fd;
                fails = 0;
                frames = 0; bytes = 0;
                stat_start = esp_timer_get_time();
                ESP_LOGI(TAG, "Bắt đầu stream");
            } else {
                drop(&fd);
            }
            if (client < 0) continue;
        }

        /* ---- Gửi một frame ---- */
        {
            camera_fb_t *fb = camera_grab();
            if (!fb) {
                ESP_LOGW(TAG, "Không lấy được frame");
                if (++fails >= 5) { /* khởi động lại camera nếu treo */
                    camera_stop();
                    if (camera_start() != ESP_OK) { drop(&client); idle_since = esp_timer_get_time(); }
                    fails = 0;
                }
                continue;
            }
            fails = 0;

            char part[96];
            int n = snprintf(part, sizeof(part),
                             "\r\n--" BOUNDARY "\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                             (unsigned)fb->len);
            bool ok = send_all(client, part, n) && send_all(client, fb->buf, fb->len);
            size_t len = fb->len;
            camera_release(fb);

            if (!ok) {
                ESP_LOGI(TAG, "Người xem đã ngắt");
                drop(&client);
                idle_since = esp_timer_get_time();
                continue;
            }

            ++frames;
            bytes += len;
            int64_t el = esp_timer_get_time() - stat_start;
            if (el >= 5000000) {
                ESP_LOGI(TAG, "%.1f fps | %u KB/frame | %.0f KB/s | RSSI %d dBm",
                         frames * 1e6 / el, (unsigned)(bytes / frames / 1024),
                         bytes * 1e6 / el / 1024, wifi_rssi());
                frames = 0; bytes = 0;
                stat_start = esp_timer_get_time();
            }
        }
    }
}

void stream_server_start(void)
{
    /* Core 1: tách khỏi Wi-Fi/lwIP (core 0) để gửi frame đều đặn. */
    xTaskCreatePinnedToCore(stream_task, "stream", 6144, NULL, 5, NULL, 1);
}

/* ---------------- trang xem (cổng 80) ---------------- */

#define STR_(x) #x
#define STR(x) STR_(x)
static const char INDEX_HTML[] =
"<!doctype html><html lang='vi'><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>ESP32-S3 Camera</title><style>"
"html,body{margin:0;height:100%;background:#000;color:#ccc;font:14px system-ui}"
"#v{display:block;width:100%;height:100%;object-fit:contain}"
"#bar{position:fixed;left:8px;bottom:8px;display:flex;gap:6px}"
"button{background:#222c;color:#eee;border:1px solid #444;border-radius:6px;padding:6px 10px}"
"</style></head><body><img id='v' alt=''>"
"<div id='bar'><button onclick='go()'>Tải lại</button>"
"<button onclick='document.documentElement.requestFullscreen()'>Toàn màn hình</button></div>"
"<script>const v=document.getElementById('v');"
"function go(){v.src='http://'+location.hostname+':" STR(STREAM_PORT) "/stream?'+Date.now()}"
"v.onerror=()=>setTimeout(go,1000);go();</script></body></html>";

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, INDEX_HTML, sizeof(INDEX_HTML) - 1);
}

void web_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = WEB_PORT;
    cfg.core_id = 0;
    httpd_handle_t server;
    ESP_ERROR_CHECK(httpd_start(&server, &cfg));
    const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = index_handler};
    httpd_register_uri_handler(server, &root);
}
