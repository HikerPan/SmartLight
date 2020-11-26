/*
 * @Author: Alex.Pan
 * @Date: 2020-11-11 10:34:24
 * @LastEditTime: 2020-11-11 10:35:04
 * @LastEditors: Alex.Pan
 * @Description: 
 * @FilePath: \ESP32\project\SmartLight\components\webserver\web_server.h
 * @Copyright 2010-2015 LEKTEC or its affiliates. All Rights Reserved.
 */
#ifndef _WEB_SERER_H_
#define _WEB_SERER_H_

void web_ser_init(void);
esp_err_t ymodem_htmllist_update(const char *name, size_t size);

#endif // ! 