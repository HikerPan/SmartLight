/*
 * Copyright (c) 2019 Fuzhou Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-12-09     Steven Liu   the first version
 * 2020-11-5      Alex Pan     port to esp32 esp-idf for 
 *                             smart light project
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "ymodem.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "web_server.h"


static const char *TAG = "ymodem/web";
struct custom_ctx
{
    struct rym_ctx parent;
    FILE* fd;
    int flen;
    char fpath[128];
};

/* ׼����ʼ�����ļ�����Ҫ�Ǵ����ļ�*/
static enum rym_code _rym_recv_begin(
    struct rym_ctx *ctx,
    uint8_t *buf,
    size_t len)
{
    struct custom_ctx *cctx = (struct custom_ctx *)ctx;
    uint16_t prelen = 0;
    char *name = NULL;
    size_t nemelen = 0;
    
    
    nemelen = strlen((const char *)buf)-3;
    name = (char *)malloc(nemelen+1);
    if(NULL != name)
    {
        memcpy(name,(const char *)buf,nemelen);
    }
    else
    {
        return ESP_ERR_NO_MEM;
    }

    // ·��ǰ׺�ĳ���
    prelen = strlen(cctx->fpath);
    
    // �����ļ�����·��ǰ׺�Ժ�
    memcpy(&cctx->fpath[prelen], (const char *)buf, nemelen);

    /*�½��ļ���������ļ��Ѿ����ڣ���գ������д*/
    cctx->fd = fopen(cctx->fpath, "wb+");
    if (cctx->fd == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing.");
        return ESP_FAIL;
    }
    
    cctx->flen = atoi((const char *)buf + strnlen((const char *)buf, len - 1)+1);
    if (cctx->flen == 0)
        cctx->flen = -1;

    
    ymodem_htmllist_update(name,cctx->flen);
    free(name);

    return RYM_CODE_ACK;
}

/* �������ݽ����ļ� */
static enum rym_code _rym_recv_data(
    struct rym_ctx *ctx,
    uint8_t *buf,
    size_t len)
{
    struct custom_ctx *cctx = (struct custom_ctx *)ctx;

    if (cctx->flen == -1)
    {
        fwrite(buf,1,len,cctx->fd);
    }
    else
    {
        int wlen = len > cctx->flen ? cctx->flen : len;
        fwrite(buf,1,wlen,cctx->fd);
        cctx->flen -= wlen;
    }


    return RYM_CODE_ACK;
}

static enum rym_code _rym_recv_end(
    struct rym_ctx *ctx,
    uint8_t *buf,
    size_t len)
{
    struct custom_ctx *cctx = (struct custom_ctx *)ctx;

    fclose(cctx->fd);
    cctx->fd = NULL;

    return RYM_CODE_ACK;
}

/*2020.11.5 ����һ��path������������ļ�·��*/
int web_download_file(const char *path)
{
    int res;
    struct custom_ctx *ctx = calloc(1, sizeof(*ctx));

    if (!ctx)
    {
        ESP_LOGE(TAG,"calloc failed\n");
        return -1;
    }
    strcpy(ctx->fpath,path);
    // strncpy(ctx->fpath,path,strlen(path));
    ESP_LOGI(TAG,"receive file to path %s",ctx->fpath);
    // ��ʱ5ms������û����ʱymodemֻ���ӡ��һ��C��ԭ��δ֪
    vTaskDelay(5);
    res = rym_recv_on_device(&ctx->parent, _rym_recv_begin, _rym_recv_data, _rym_recv_end, 60);
    free(ctx);

    return res;
}






