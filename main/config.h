#pragma once
/*
 * ============================================================
 *  CẤU HÌNH CHÍNH — chỉ cần sửa file này
 * ============================================================
 */

/* ---------- 1. Chọn board ----------
 * BOARD_ESP32S3_OV5640      : ESP32-S3 + camera OV5640 (board bạn đang dùng)
 * BOARD_XIAO_ESP32S3_SENSE  : Seeed XIAO ESP32S3 Sense (OV2640)
 * Có thể ghi đè khi build:  idf.py -DCAM_BOARD=XIAO build
 * Chân cắm từng board nằm trong main/boards/.
 */
#define BOARD_ESP32S3_OV5640      1
#define BOARD_XIAO_ESP32S3_SENSE  2

#ifndef CAM_BOARD
#define CAM_BOARD  BOARD_ESP32S3_OV5640
#endif

/* ---------- 2. Wi-Fi nhà ---------- */
#define WIFI_SSID       "Kien"
#define WIFI_PASSWORD   "hoithangtuan"

/* Tên mạng của camera: mở http://esp32cam.local (hoặc IP in ra log). */
#define DEVICE_HOSTNAME "esp32cam"

/* IP tĩnh (tùy chọn). Để 0 thì dùng DHCP của router. */
#define USE_STATIC_IP   0
#define STATIC_IP       "192.168.1.50"
#define STATIC_GATEWAY  "192.168.1.1"
#define STATIC_NETMASK  "255.255.255.0"
#define STATIC_DNS      "192.168.1.1"

/* ---------- 3. Video ----------
 * Độ phân giải (dưới HD):
 *   FRAMESIZE_QVGA  320x240   — nhẹ nhất
 *   FRAMESIZE_HVGA  480x320
 *   FRAMESIZE_VGA   640x480   — mặc định, cân bằng nét/mượt
 *   FRAMESIZE_SVGA  800x600   — nét hơn, FPS giảm
 * JPEG quality: 10..63, số nhỏ = nét hơn nhưng nặng hơn.
 * Nếu hình giật, tăng quality lên 14–16 hoặc hạ độ phân giải.
 */
#define VIDEO_FRAME_SIZE    FRAMESIZE_VGA
#define VIDEO_JPEG_QUALITY  12

/* Cổng: trang xem ở :80, luồng MJPEG ở :81/stream */
#define WEB_PORT     80
#define STREAM_PORT  81

/* Tắt camera sau bao lâu không có người xem (ms) để đỡ nóng.
 * 0 = luôn bật (mở stream nhanh hơn một chút). */
#define CAMERA_IDLE_OFF_MS  15000
