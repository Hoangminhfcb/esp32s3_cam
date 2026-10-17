#pragma once
#include <stdbool.h>

void wifi_start(void);        /* kết nối Wi-Fi nhà, tự nối lại khi rớt */
bool wifi_wait_connected(int timeout_ms);
int  wifi_rssi(void);         /* dBm, 0 nếu chưa kết nối */
