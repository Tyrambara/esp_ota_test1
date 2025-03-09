#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_crt_bundle.h"

static const char *TAG = "OTA_UPDATE";

#define HASH_LEN 32

#define WIFI_SSID CONFIG_EXAMPLE_WIFI_SSID
#define WIFI_PASSWORD CONFIG_EXAMPLE_WIFI_PASSWORD
#define OTA_URL CONFIG_OTA_UPDATE_URL

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

static void ota_task(void *pvParameter) {
    const int CHECK_INTERVAL_MS = 30000;
    
    while(1) {
        esp_http_client_config_t config = {
            .url = OTA_URL,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .keep_alive_enable = true,
            .event_handler = _http_event_handler,
            //.timeout_ms = 30000,
            //.buffer_size = 4096,
            //.buffer_size_tx = 2048
        };
        
        esp_https_ota_handle_t ota_handle = NULL;
        esp_err_t ota_finish_err = ESP_OK;
        
        esp_https_ota_config_t ota_config = {
            .http_config = &config,
            //.partial_http_download = true,
            //.max_http_request_size = 4096
        };
        
        esp_err_t err = esp_https_ota_begin(&ota_config, &ota_handle);
        
        if(err == ESP_OK) {
            esp_app_desc_t new_desc;
            esp_https_ota_get_img_desc(ota_handle, &new_desc);
            const esp_app_desc_t *curr_desc = esp_app_get_description();
            
            if(strcmp(new_desc.version, curr_desc->version) != 0) {
                while(1) {
                    err = esp_https_ota_perform(ota_handle);
                    if(err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) break;
                }
                
                if(err == ESP_OK) {
                    ota_finish_err = esp_https_ota_finish(ota_handle);
                    if(ota_finish_err == ESP_OK) {
                        ESP_LOGI(TAG, "Restarting for update...");
                        esp_restart();
                    }
                }
            }
            esp_https_ota_finish(ota_handle);
        }
        vTaskDelay(pdMS_TO_TICKS(CHECK_INTERVAL_MS));
    }
}

static void NetworkEventHandler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI("wifi", "Retrying to connect to Wifi...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("wifi", "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void InitNVS() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void InitWifi() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());    
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD
        }
    };

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &NetworkEventHandler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &NetworkEventHandler, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void print_sha256(const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i) {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG, "%s %s", label, hash_print);
}

static void get_sha256_of_partitions(void)
{
    uint8_t sha_256[HASH_LEN] = { 0 };
    esp_partition_t partition;

    partition.address   = ESP_BOOTLOADER_OFFSET;
    partition.size      = ESP_PARTITION_TABLE_OFFSET;
    partition.type      = ESP_PARTITION_TYPE_APP;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for bootloader: ");

    esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
    print_sha256(sha_256, "SHA-256 for current firmware: ");
}

void app_main(void) {
    InitNVS();
    get_sha256_of_partitions();
    InitWifi();

    const esp_app_desc_t *desc = esp_app_get_description();
    ESP_LOGI(TAG, "Current version: %s", desc->version);

    xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, NULL);
}
