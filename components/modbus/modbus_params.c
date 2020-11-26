/*
 * @Author: Alex.Pan
 * @Date: 2020-11-13 14:24:36
 * @LastEditTime: 2020-11-24 10:44:07
 * @LastEditors: Alex.Pan
 * @Description: 
 * @FilePath: \ESP32\project\SmartLight\components\modbus\modbus_params.c
 * @Copyright 2010-2015 LEKTEC or its affiliates. All Rights Reserved.
 */
/*=====================================================================================
 * Description:
 *   C file to define parameter storage instances
 *====================================================================================*/
#include <stdint.h>
#include "modbus_params.h"

// Here are the user defined instances for device parameters packed by 1 byte
// These are keep the values that can be accessed from Modbus master
holding_reg_params_sms_t holding_reg_params_sms = { 0 };
coil_reg_params_sms_t coil_reg_params_sms = {0}; 
discrete_reg_params_sms_t discrete_reg_params_sms={0};
// holding_reg_params_t holding_reg_params = { 0 };

// input_reg_params_t input_reg_params = { 0 };

// coil_reg_params_t coil_reg_params = { 0 };

// discrete_reg_params_t discrete_reg_params = { 0 };

