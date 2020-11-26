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


static const char *TAG = "ymodem/file";
struct custom_ctx
{
    struct rym_ctx parent;
    FILE* fd;
    int flen;
    char fpath[128];
};

/* 准备开始接收文件，主要是创建文件*/
static enum rym_code _rym_recv_begin(
    struct rym_ctx *ctx,
    uint8_t *buf,
    size_t len)
{
    struct custom_ctx *cctx = (struct custom_ctx *)ctx;
    uint16_t prelen = 0;
    
    // 路径前缀的长度
    prelen = strlen(cctx->fpath);

    // 拷贝文件名至路径前缀以后
    strcpy(&cctx->fpath[prelen], (const char *)buf);

    /*新建文件，如果该文件已经存在，清空，允许读写*/
    cctx->fd = fopen(cctx->fpath, "wb+");
    if (cctx->fd == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing.");
        return ESP_FAIL;
    }
    
    cctx->flen = atoi((const char *)buf + strnlen((const char *)buf, len - 1)+1);
    if (cctx->flen == 0)
        cctx->flen = -1;

    return RYM_CODE_ACK;
}

/* 接收数据进入文件 */
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

/*2020.11.5 新增一个path，用于命令传递文件路径*/
int rym_download_file(const char *path)
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
    // 延时5ms，这里没有延时ymodem只会打印出一个C，原因未知
    vTaskDelay(5);
    res = rym_recv_on_device(&ctx->parent, _rym_recv_begin, _rym_recv_data, _rym_recv_end, 60);
    free(ctx);

    return res;
}

#if 1
static enum rym_code _rym_send_begin(
    struct rym_ctx *ctx,
    uint8_t *buf,
    size_t len)
{
    struct custom_ctx *cctx = (struct custom_ctx *)ctx;
    char insert_0 = '\0';
    struct stat file_buf;
    char *filename = NULL;
    char *pfind = NULL;

    int err = -1;
    cctx->fd = fopen(cctx->fpath,"rt");
    if(NULL == cctx->fd)
    {
        ESP_LOGE(TAG,"open file %s failed",cctx->fpath);
        return (enum rym_code)RYM_ERR_FILE;
    }

    err = stat(cctx->fpath, &file_buf);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG,"error open file.\n");
        return RYM_ERR_FILE;
    }

    // 从路径中检索出文件名
    pfind = filename = &cctx->fpath[0];
    while(pfind != NULL)
    {
        pfind = strchr(filename,'/');
        if(pfind != NULL)
        {
            filename = pfind+1;
            // ESP_LOGI(TAG,"filename %s",filename);
        }
        else
        {
            break;
        }
    }

    sprintf((char *)buf, "%s%c%ld", (char *) filename, insert_0, file_buf.st_size);

    return RYM_CODE_SOH;
}

static enum rym_code _rym_send_data(
    struct rym_ctx *ctx,
    uint8_t *buf,
    size_t len)
{
    struct custom_ctx *cctx = (struct custom_ctx *)ctx;
    size_t read_size;
    int retry_read;

    read_size = 0;
    for (retry_read = 0; retry_read < 10; retry_read++)
    {
        read_size += fread(buf + read_size, 1,len - read_size,cctx->fd );
        if (read_size == len)
            break;
    }

    if (read_size < len)
    {
        memset(buf + read_size, 0x1A, len - read_size);
        /* stage = RYM_STAGE_FINISHING */
        ctx->stage = RYM_STAGE_FINISHING;
    }

    return RYM_CODE_SOH;
}

static enum rym_code _rym_send_end(
    struct rym_ctx *ctx,
    uint8_t *buf,
    size_t len)
{
    struct custom_ctx *cctx = (struct custom_ctx *)ctx;
    memset(buf, 0, len);
    if(NULL != cctx->fd)
    {
        fclose(cctx->fd);
        cctx->fd = NULL;
    }

    return RYM_CODE_SOH;
}


int rym_upload_file(const char *file_path)
{
    int res = 0;

    struct custom_ctx *ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
    {
        ESP_LOGE(TAG,"malloc failed\n");
        return -1;
    }

    strcpy(ctx->fpath, file_path);
    res = rym_send_on_device(&ctx->parent, 
                             _rym_send_begin, _rym_send_data, _rym_send_end, 1000);
    free(ctx);

    return res;
}
#endif

#if 0
/** Arguments used by 'ry' function */
static struct {
    struct arg_str *path;
    struct arg_end *end;
} ry_args;

static int ry(int argc, char **argv)
{

    int nerrors = arg_parse(argc, argv, (void **) &ry_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, ry_args.end, argv[0]);
        return 1;
    }

    return(rym_download_file(ry_args.path->sval[0]));
}



void register_ry(void)
{
    // join_args.timeout = arg_int0(NULL, "timeout", "<t>", "Connection timeout, ms");
    ry_args.path = arg_str0(NULL, NULL, "<path>", "the path of ymodem received file.");
    ry_args.end = arg_end(1);

    const esp_console_cmd_t ry_cmd = {
        .command = "ry",
        .help = "Receive file form terminal",
        .hint = NULL,
        .func = &ry,
        .argtable = &ry_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&ry_cmd) );
}

/** Arguments used by 'ry' function */
static struct {
    struct arg_str *path;
    struct arg_end *end;
} sy_args;

static int sy(int argc, char **argv)
{

    int nerrors = arg_parse(argc, argv, (void **) &sy_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, sy_args.end, argv[0]);
        return 1;
    }
    return(rym_upload_file(sy_args.path->sval[0]));

}

void register_sy(void)
{
    // join_args.timeout = arg_int0(NULL, "timeout", "<t>", "Connection timeout, ms");
    sy_args.path = arg_str0(NULL, NULL, "<path>", "the path of ymodem received file.");
    sy_args.end = arg_end(1);

    const esp_console_cmd_t sy_cmd = {
        .command = "sy",
        .help = "Send file to terminal",
        .hint = NULL,
        .func = &sy,
        .argtable = &sy_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&sy_cmd) );
}
#endif

// static  SHELL_CMD  ymodem_CmdTbl [] = {
//     {"ym_ry", ry},
//     // {"ym_ryca", ryca},
//     {"ym_sy", sy},
//     {0,         0          }
// };

// CPU_BOOLEAN  Ym_Shell_Init (void)
// {
//     SHELL_ERR    err;
//     CPU_BOOLEAN  ok;


//     Shell_CmdTblAdd((CPU_CHAR *)"ym", ymodem_CmdTbl, &err);

//     ok = (err == SHELL_ERR_NONE) ? DEF_OK : DEF_FAIL;
//     return (ok);
// }


// void register_ymodem(void)
// {
//     register_ry();
//     register_sy();
// }

