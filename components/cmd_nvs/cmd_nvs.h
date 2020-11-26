/*
 * @Author: Alex.Pan
 * @Date: 2020-11-02 15:45:50
 * @LastEditTime: 2020-11-20 17:29:06
 * @LastEditors: Alex.Pan
 * @Description: 
 * @FilePath: \ESP32\project\SmartLight\components\cmd_nvs\cmd_nvs.h
 * @Copyright 2010-2015 LEKTEC or its affiliates. All Rights Reserved.
 */
/* Console example — declarations of command registration functions.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif
esp_err_t set_str_in_nvs(const char *key, const char *str_value);
char* get_str_from_nvs(const char *key);
// Register NVS functions
void register_nvs(void);

#ifdef __cplusplus
}
#endif

