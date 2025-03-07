//idf.py -D SDKCONFIG="sdkconfig.local" reconfigure

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

#define WIFI_SSID CONFIG_EXAMPLE_WIFI_SSID
#define WIFI_PASSWORD CONFIG_EXAMPLE_WIFI_PASSWORD
#define OTA_URL CONFIG_OTA_UPDATE_URL

static void ota_task(void *pvParameter) {
    const int CHECK_INTERVAL_MS = 300000; // 5min
    
    while(1) {
        esp_http_client_config_t config = {
            .url = OTA_URL,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .timeout_ms = 10000
        };
        
        esp_https_ota_handle_t ota_handle = NULL;
        esp_err_t ota_finish_err = ESP_OK;
        
        esp_https_ota_config_t ota_config = {
            .http_config = &config,
            .partial_http_download = true
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

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());

    const esp_app_desc_t *desc = esp_app_get_description();
    ESP_LOGI(TAG, "Current version: %s", desc->version);

    xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, NULL);
}
