# ESP32-S3 Camera → Wi-Fi nhà (MJPEG)

ESP32-S3 kết nối Wi-Fi nhà và phát video MJPEG. Xem bằng trình duyệt, VLC, OBS hoặc Home Assistant.

| | Địa chỉ |
| --- | --- |
| Trang xem | `http://esp32cam.local` hoặc `http://<IP>` |
| Luồng MJPEG | `http://<IP>:81/stream` |

IP được in ra log khi kết nối Wi-Fi.

## Cấu trúc

```text
main/
  config.h                       ← SỬA FILE NÀY: Wi-Fi, chọn board, độ phân giải
  boards/
    board_esp32s3_ov5640.h       ← chân cắm ESP32-S3 + OV5640
    board_xiao_esp32s3_sense.h   ← chân cắm XIAO ESP32S3 Sense (OV2640)
  board.h                        chọn file board theo config.h
  camera.c/.h                    bật/tắt camera, lấy frame
  wifi.c/.h                      Wi-Fi STA + tự nối lại + mDNS
  stream.c/.h                    MJPEG cổng 81 + trang xem cổng 80
  main.c
sdkconfig.defaults               tối ưu Wi-Fi/PSRAM/camera
```

## Cấu hình

Mở `main/config.h`:

1. `WIFI_SSID`, `WIFI_PASSWORD`: Wi-Fi nhà (chỉ hỗ trợ 2.4 GHz).
2. `CAM_BOARD`: `BOARD_ESP32S3_OV5640` hoặc `BOARD_XIAO_ESP32S3_SENSE`.
3. `VIDEO_FRAME_SIZE`: mặc định `FRAMESIZE_VGA` (640×480). `VIDEO_JPEG_QUALITY`: mặc định 12.

## Build và nạp (ESP-IDF 5.5)

Lần đầu sau khi đổi sang bản này, **xóa `sdkconfig` và thư mục `build`** để các thiết lập tối ưu trong `sdkconfig.defaults` được áp dụng.

```powershell
# Board OV5640 (mặc định trong config.h)
idf.py build
idf.py -p COM4 flash monitor

# XIAO ESP32S3 Sense mà không cần sửa config.h (dùng thư mục build riêng)
idf.py -B build_xiao -DCAM_BOARD=XIAO build
idf.py -B build_xiao -p COM5 flash monitor
```

Thay `COM4`/`COM5` bằng cổng thật. Thoát monitor bằng `Ctrl+]`.

## Để stream mượt nhất

- Đã làm sẵn: tắt tiết kiệm điện Wi-Fi, 2 frame buffer trong PSRAM với chế độ lấy frame mới nhất (Wi-Fi chậm thì bỏ frame cũ, không dồn trễ), gửi qua socket TCP thô kèm `TCP_NODELAY`, tăng buffer Wi-Fi/TCP, task stream chạy riêng trên core 1.
- Log in mỗi 5 giây: `fps | KB/frame | KB/s | RSSI`. RSSI nên trên -70 dBm. Nếu yếu hơn, đặt camera gần router hoặc dùng anten ngoài (XIAO có sẵn anten rời).
- Nếu vẫn giật: tăng `VIDEO_JPEG_QUALITY` lên 14–16, hoặc hạ xuống `FRAMESIZE_HVGA`.
- Nếu ảnh bị sọc hoặc lỗi màu: hạ `CAM_XCLK_HZ` trong file board xuống `10000000`.
- Mỗi lúc chỉ một người xem. Người mở sau sẽ chiếm luồng, nên tải lại trang không bị báo bận.
- Camera tự tắt sau 15 giây không có người xem cho đỡ nóng (`CAMERA_IDLE_OFF_MS`, đặt 0 nếu muốn luôn bật).

## Phần cứng

Cả hai board được cấu hình flash 8 MB và PSRAM octal (N8R8). Nếu board OV5640 của bạn là loại PSRAM quad (ví dụ N16R2), đổi `CONFIG_SPIRAM_MODE_OCT=y` thành `CONFIG_SPIRAM_MODE_QUAD=y` trong `sdkconfig.defaults`.
