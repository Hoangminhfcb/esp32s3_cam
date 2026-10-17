#include "wifi.h"
#include "config.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "mdns.h"

static const char *TAG = "wifi";
static EventGroupHandle_t events;
#define CONNECTED_BIT BIT0

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(events, CONNECTED_BIT);
        wifi_event_sta_disconnected_t *e = data;
        ESP_LOGW(TAG, "Mất Wi-Fi (reason %d), đang kết nối lại...", e->reason);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "==============================================");
        ESP_LOGI(TAG, " Xem video: http://" IPSTR "  hoặc  http://%s.local",
                 IP2STR(&e->ip_info.ip), DEVICE_HOSTNAME);
        ESP_LOGI(TAG, " Luồng MJPEG: http://" IPSTR ":%d/stream",
                 IP2STR(&e->ip_info.ip), STREAM_PORT);
        ESP_LOGI(TAG, "==============================================");
        xEventGroupSetBits(events, CONNECTED_BIT);
    }
}

static void start_mdns(void)
{
    if (mdns_init() != ESP_OK) { ESP_LOGW(TAG, "mDNS không khởi tạo được"); return; }
    mdns_hostname_set(DEVICE_HOSTNAME);
    mdns_instance_name_set("ESP32-S3 Camera");
    mdns_service_add(NULL, "_http", "_tcp", WEB_PORT, NULL, 0);
}

void wifi_start(void)
{
    events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(netif, DEVICE_HOSTNAME);

#if USE_STATIC_IP
    esp_netif_dhcpc_stop(netif);
    esp_netif_ip_info_t ip = {0};
    esp_netif_str_to_ip4(STATIC_IP, &ip.ip);
    esp_netif_str_to_ip4(STATIC_GATEWAY, &ip.gw);
    esp_netif_str_to_ip4(STATIC_NETMASK, &ip.netmask);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip));
    esp_netif_dns_info_t dns = {0};
    esp_netif_str_to_ip4(STATIC_DNS, &dns.ip.u_addr.ip4);
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns);
#endif

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));

    wifi_config_t wc = {0};
    strlcpy((char *)wc.sta.ssid, WIFI_SSID, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, WIFI_PASSWORD, sizeof(wc.sta.password));
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wc.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;      /* router WPA2/WPA3 đều được */
    wc.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;  /* chọn AP mạnh nhất nếu có nhiều */
    wc.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* Tắt tiết kiệm điện: quan trọng nhất cho stream mượt/độ trễ thấp. */
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    start_mdns();
    ESP_LOGI(TAG, "Đang kết nối Wi-Fi \"%s\"...", WIFI_SSID);
}

bool wifi_wait_connected(int timeout_ms)
{
    return xEventGroupWaitBits(events, CONNECTED_BIT, pdFALSE, pdTRUE,
                               pdMS_TO_TICKS(timeout_ms)) & CONNECTED_BIT;
}

int wifi_rssi(void)
{
    wifi_ap_record_t ap;
    return esp_wifi_sta_get_ap_info(&ap) == ESP_OK ? ap.rssi : 0;
}
