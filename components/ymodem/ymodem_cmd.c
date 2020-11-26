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
#include "ymodem_file.h"
#include "ymodem_ota.h"
// #include "ymodem_web.h"

static const char *TAG="ymodem/cmd";

/** Arguments used by 'ry' function */
static struct {
    struct arg_str *subcmd;
    struct arg_str *path;
    struct arg_end *end;
} ymodem_args;

static int ymodem_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &ymodem_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, ymodem_args.end, argv[0]);
        return 1;
    }

    if(!strcmp(ymodem_args.subcmd->sval[0],"rcv"))
    {
        return(rym_download_file(ymodem_args.path->sval[0]));
    }
    else if(!strcmp(ymodem_args.subcmd->sval[0],"send"))
    {
        return(rym_upload_file(ymodem_args.path->sval[0]));
    }
    else if(!strcmp(ymodem_args.subcmd->sval[0],"ota"))
    {
        ymodem_ota();
    }
    // else if(!strcmp(ymodem_args.subcmd->sval[0],"web"))
    // {
    //     return(web_download_file(ymodem_args.path->sval[0]));
    // }
    else
    {
        ESP_LOGW(TAG,"sub command %s not support!",ymodem_args.subcmd->sval[0]);
        ESP_LOGW(TAG,"pls use %s or %s or %s ","rcv","send","ota");
    }
    

    return ESP_OK;
}


void register_ymodem(void)
{
    ymodem_args.subcmd = arg_str0(NULL, NULL, "<subcmb>", "sub command of ymodem. rcv/send file or ota.");
    ymodem_args.path = arg_str0(NULL, NULL, "<path>", "the path of ymodem rcv/send file.");
    ymodem_args.end = arg_end(1);

    const esp_console_cmd_t ym_cmd = {
        .command = "ymodem",
        .help = "ymodem protocol for file or ota",
        .hint = NULL,
        .func = &ymodem_cmd,
        .argtable = &ymodem_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&ym_cmd) );
}


