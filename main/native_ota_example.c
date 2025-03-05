#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "protocol_examples_common.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_crt_bundle.h"

static const char *TAG = "OTA_UPDATE";

#define OTA_URL CONFIG_EXAMPLE_FIRMWARE_UPG_URL

static void ota_task(void *pvParameter) {
    const int CHECK_INTERVAL_MS = 36000; // 1 час
    
    while(1) {
        esp_http_client_config_t config = {
            .url = CONFIG_EXAMPLE_FIRMWARE_UPG_URL,
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
    
    ESP_ERROR_CHECK(example_connect());

    xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, NULL);
}