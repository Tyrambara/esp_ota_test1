#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_crt_bundle.h"

static const char *TAG = "OTA_UPDATE";

#define WIFI_SSID CONFIG_EXAMPLE_WIFI_SSID
#define WIFI_PASS CONFIG_EXAMPLE_WIFI_PASSWORD
#define OTA_URL CONFIG_OTA_UPDATE_URL
#define CHECK_INTERVAL_MS 30000  // 30 секунд между проверками

void wifi_event_handler(void* arg, esp_event_base_t event_base,
                       int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGW(TAG, "WiFi disconnected, reconnecting...");
            esp_wifi_connect();
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void ota_task(void *pvParameter)
{
    while(1) {
        ESP_LOGI(TAG, "Checking for OTA updates...");
        
        esp_http_client_config_t http_config = {
            .url = OTA_URL,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .timeout_ms = 10000,
            .buffer_size_tx = 2048
        };
        
        esp_https_ota_config_t ota_config = {
            .http_config = &http_config,
            .partial_http_download = true,
            .max_http_request_size = 4096
        };
        
        esp_https_ota_handle_t ota_handle = NULL;
        esp_err_t err = esp_https_ota_begin(&ota_config, &ota_handle);
        
        if (err == ESP_OK) {
            esp_app_desc_t new_desc;
            esp_https_ota_get_img_desc(ota_handle, &new_desc);
            const esp_app_desc_t *curr_desc = esp_app_get_description();
            
            if (strcmp(new_desc.version, curr_desc->version) != 0) {
                ESP_LOGI(TAG, "New firmware detected: %s", new_desc.version);
                
                while(1) {
                    err = esp_https_ota_perform(ota_handle);
                    if(err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) break;
                }
                
                if(err == ESP_OK) {
                    if(esp_https_ota_is_complete_data_received(ota_handle)) {
                        esp_err_t ota_finish_err = esp_https_ota_finish(ota_handle);
                        
                        if(ota_finish_err == ESP_OK) {
                            const esp_partition_t *updated = esp_ota_get_next_update_partition(NULL);
                            esp_ota_set_boot_partition(updated);
                            
                            ESP_LOGI(TAG, "Update successful! Rebooting...");
                            esp_restart();
                        }
                    }
                }
            }
            esp_https_ota_finish(ota_handle);
        }
        else {
            ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(err));
        }
        
        vTaskDelay(pdMS_TO_TICKS(CHECK_INTERVAL_MS));
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(ret);

    wifi_init();

    xTaskCreate(ota_task, "ota_task", 8192, NULL, 5, NULL);

    //const esp_app_desc_t *desc = esp_app_get_description();
    //ESP_LOGI(TAG, "Current firmware version: %s", desc->version);
    //const esp_partition_t *running = esp_ota_get_running_partition();
    //ESP_LOGI(TAG, "Current Firmware Partition: %-5s", running->label);
}