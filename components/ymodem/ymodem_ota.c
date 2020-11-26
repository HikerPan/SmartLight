/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-01-30     armink       the first version
 * 2018-08-27     Murphy       update log
 */

#include <stdio.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"
#include "ymodem.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "esp_system.h"

#if CONFIG_YMODEM_OTA_ENABLE
static const char *TAG = "ymodem/ota";

static size_t update_file_total_size, update_file_cur_size;
static const esp_partition_t *update_partition;
static esp_ota_handle_t ota_handle;

static enum rym_code ymodem_on_begin(struct rym_ctx *ctx, uint8_t *buf, size_t len)
{
    char *file_name, *file_size;
    esp_err_t err = ESP_OK;

    /* calculate and store file size */
    file_name = (char *)&buf[0];
    file_size = (char *)&buf[strlen(file_name) + 1];
    update_file_total_size = atol(file_size);
    ESP_LOGI(TAG,"Ymodem file_size:%d\n", update_file_total_size);

    update_file_cur_size = 0;

    /* Get download partition information and erase download partition data */
    /*查找升级分区*/
    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG,"Find partion (%s), address 0x%x, size %d",update_partition->label,
                                                    update_partition->address,
                                                    update_partition->size);
    if (update_partition == NULL)
    {
        ESP_LOGE(TAG,"Firmware download failed! Get next update partition  error!");
        return RYM_CODE_CAN;
    }

    ESP_LOGI(TAG,"esp ymodem ota begin");
    err = esp_ota_begin(update_partition,update_file_total_size,&ota_handle);
    if(ESP_OK != err)
    {
        ESP_LOGE(TAG,"ymodem ota begin failed (%s)",esp_err_to_name(err));
        return RYM_CODE_CAN;
    }

    return RYM_CODE_ACK;
}

static enum rym_code ymodem_on_data(struct rym_ctx *ctx, uint8_t *buf, size_t len)
{
    esp_err_t err = ESP_OK;

    /* write data of application to download partition  */
    err = esp_ota_write(ota_handle,buf, len);
    if(ESP_OK != err)
    {
        ESP_LOGE(TAG,"ymodem ota write failed (%s)",esp_err_to_name(err));
        return RYM_CODE_CAN;
    }

    return RYM_CODE_ACK;
}

static enum rym_code ymodem_on_end(struct rym_ctx *ctx,uint8_t *buf,size_t len)
{
    esp_ota_end(ota_handle);

    return RYM_CODE_ACK;
}

void ymodem_ota(void)
{
    struct rym_ctx rctx;


    ESP_LOGI(TAG,"Warning: Ymodem has started! This operator will not recovery.\n");
    ESP_LOGI(TAG,"Please select the ota firmware file and use Ymodem to send.\n");

    if (!rym_recv_on_device(&rctx,ymodem_on_begin, ymodem_on_data, ymodem_on_end, 60))
    {
        ESP_LOGI(TAG,"Download firmware to flash success.\n");
        ESP_LOGI(TAG,"System now will restart...\r\n");

        esp_ota_set_boot_partition(update_partition);
        vTaskDelay(100);

        /* Reset the device, Start new firmware */
        esp_restart();

    }
    else
    {
        /* wait some time for terminal response finish */
        vTaskDelay(100);
        ESP_LOGE(TAG,"Update firmware fail.\n");
    }

    return;
}
#endif

#if 0
/** Arguments used by 'ry' function */
static struct {
    struct arg_str *path;
    struct arg_end *end;
} ym_ota_args;

static int ym_ota(int argc, char **argv)
{

    int nerrors = arg_parse(argc, argv, (void **) &ym_ota_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, ym_ota_args.end, argv[0]);
        return 1;
    }

    ymodem_ota();
    return ESP_OK;
}


void register_ym_ota(void)
{
    ym_ota_args.path = arg_str0(NULL, NULL, "<path>", "the path of ymodem received file.");
    ym_ota_args.end = arg_end(1);

    const esp_console_cmd_t ym_ota_cmd = {
        .command = "ymodem_ota",
        .help = "ymodem ota funtion",
        .hint = NULL,
        .func = &ym_ota,
        .argtable = &ym_ota_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&ym_ota_cmd) );
}
#endif


