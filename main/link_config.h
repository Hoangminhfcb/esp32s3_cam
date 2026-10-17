#pragma once

/* Keep these values identical in both projects' main/link_config.h. */
#define LINK_WIFI_SSID       "GUITION_CAM"
#define LINK_WIFI_PASSWORD   "guition123"
#define LINK_WIFI_CHANNEL    6
#define LINK_AP_IP           "192.168.4.1"
#define LINK_CAM_IP          "192.168.4.2"
#define LINK_NETMASK         "255.255.255.0"
#define LINK_STREAM_PORT     81
#define LINK_VIDEO_PORT      82 /* binary low-latency viewer; 81 remains browser MJPEG */
#define LINK_STREAM_URL      "http://" LINK_CAM_IP ":81/stream"
