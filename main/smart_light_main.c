/*
 * @Author: Alex.Pan
 * @Date: 2020-10-30 16:03:01
 * @LastEditTime: 2020-11-22 21:15:34
 * @LastEditors: Alex.Pan
 * @Description:
 * @FilePath: \ESP32\project\SmartLight\main\smart_light_main.c
 * @Copyright 2010-2015 LEKTEC or its affiliates. All Rights Reserved.
 */
/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include "sdkconfig.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "esp_netif.h"
#include "esp_console.h"
#include "esp_event.h"

#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "cmd_system.h"
#include "cmd_nvs.h"
#include "cmd_wifi.h"
#include "ymodem_cmd.h"
#include "cmd_fs.h"
#include "esp_spiffs.h"
#include "esp_littlefs.h"
#include <esp_http_server.h>
#include "web_server.h"
#include "mb_master.h"
#include "driver/gpio.h"

#define BLINK_GPIO 2

static const char *TAG = "app";

static void initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

#if 1

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    wifi_event_ap_stadisconnected_t *event = NULL;

    switch (event_id)
    {
    case WIFI_EVENT_WIFI_READY:
        ESP_LOGI(TAG, "wifi ready.");
        break;
    case WIFI_EVENT_AP_STACONNECTED:
        event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
        break;
    case WIFI_EVENT_AP_STADISCONNECTED:
        event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
        break;
    case WIFI_EVENT_AP_START:
        ESP_LOGI(TAG, "wifi ap start.");
        break;
    case WIFI_EVENT_AP_STOP:
        ESP_LOGI(TAG, "wifi ap stop.");
        break;
    }
}

/* Initialize wifi with tcp/ip adapter */
static void initialize_wifi(void)
{
    uint8_t mac[6];

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    ESP_EVENT_ANY_ID,
                    &wifi_event_handler,
                    NULL,
                    NULL));

    wifi_config_t wifi_config =
    {
        .ap = {
            .ssid = "SmartLight_",
            .ssid_len = strlen("SmartLight_"),
            .channel = 1,
            .password = "123456789",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };


    if (0 == strlen((char *)&wifi_config.ap.ssid[0]))
    {
        if (strlen("123456789") == 0)
        {
            wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        }
        esp_wifi_get_mac(ESP_IF_WIFI_AP, mac);
        char *ssidstr = (char *)&wifi_config.ap.ssid[0];
        sprintf(ssidstr + strlen("SmartLight_"), MACSTR, MAC2STR(mac));
        wifi_config.ap.ssid_len = strlen((char *)&wifi_config.ap.ssid[0]);
    }
    else
    {
        esp_wifi_get_config(WIFI_IF_AP, &wifi_config);
        ESP_LOGI(TAG, "AP mode, %s %s", wifi_config.ap.ssid, wifi_config.ap.password);
    }

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    // ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
#endif




void littlefs_init(void)
{
    ESP_LOGI(TAG, "Initializing LittleFS");

    esp_vfs_littlefs_conf_t conf =
    {
        .base_path = "/lfs",
        .partition_label = "storage",
        .dont_mount = false,
        .format_if_mount_failed = true
    };

    // Use settings defined above to initialize and mount littlefs filesystem.
    // Note: esp_vfs_littlefs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get littlefs partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    DIR *web_dir = NULL;
    web_dir = opendir("/lfs/web");
    if (NULL == web_dir)
    {
        if (ESP_OK == mkdir("/lfs/web", 0))
        {
            ESP_LOGI(TAG, "web folder create success");
        }
        else
        {
            ESP_LOGW(TAG, "web folder create fail");
        }
    }
    else
    {
        ESP_LOGI(TAG, "web folder already here");
        closedir(web_dir);
        web_dir = NULL;
    }


    DIR *modbus_dir = NULL;
    modbus_dir = opendir("/lfs/modbus");
    if (NULL == modbus_dir)
    {
        if (ESP_OK == mkdir("/lfs/modbus", 0))
        {
            ESP_LOGI(TAG, "modbus folder create success");
        }
        else
        {
            ESP_LOGW(TAG, "modbus folder create fail");
        }
    }
    else
    {
        ESP_LOGI(TAG, "modbus folder already here");
        closedir(modbus_dir);
        modbus_dir = NULL;
    }

}

void app_main(void)
{
    // static httpd_handle_t server = NULL;

    initialize_nvs();
    // spiffs_init();
    littlefs_init();
    /* Initialize WiFi */
    initialize_wifi();
    // initialize_wifi(&server);

    web_ser_init();
    /*创建并启动console task*/
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

    repl_config.prompt = "smartlight>";
    // repl_config.task_stack_size = 1024*8;
    // init console REPL environment
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    register_system();
    register_nvs();
    register_wifi();
    register_ymodem();
    // register_ymodem();
    // register_ym_ota();
    register_fs_cmd();
    register_modbus();
    // start console REPL
    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    master_init();

    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    while (true)
    {
        // char *buf = NULL;
        // if(ESP_OK == modbus_param_get(buf))
        // {
        //     ESP_LOGI(TAG,"modbus param %s",buf);
        //     free(buf);
        // }
        // char *buf = modbus_param_get();
        // if(buf != NULL)
        // {
        //     ESP_LOGI(TAG,"modbus param %s",buf);
        //     free(buf);
        // }
        // master_operation_func(NULL);

        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(250 / portTICK_PERIOD_MS);
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

}
