/*
 * @Author: Alex.Pan
 * @Date: 2020-11-10 10:24:17
 * @LastEditTime: 2020-11-10 10:39:08
 * @LastEditors: Alex.Pan
 * @Description:
 * @FilePath: \ESP32\project\SmartLight\components\ymodem\ymodem_file.h
 * @Copyright 2010-2015 LEKTEC or its affiliates. All Rights Reserved.
 */
#ifndef _YMODEM_FILE_
#define _YMODEM_FILE_
int rym_upload_file(const char *file_path);
int rym_download_file(const char *path);
#endif