/*
 * @Author: Alex.Pan
 * @Date: 2020-11-13 14:08:49
 * @LastEditTime: 2020-11-19 16:17:12
 * @LastEditors: Alex.Pan
 * @Description: 
 * @FilePath: \ESP32\project\SmartLight\components\modbus\mb_master.h
 * @Copyright 2010-2015 LEKTEC or its affiliates. All Rights Reserved.
 */
#ifndef _MB_MASTER_H_
#define _MB_MASTER_H_
#include "esp_log.h"

esp_err_t master_init(void);
void master_operation_func(void *arg);
void register_modbus(void);
// esp_err_t modbus_param_get(char *buf);
char *modbus_param_get(void);
esp_err_t modbus_param_set(char *buf);
#endif