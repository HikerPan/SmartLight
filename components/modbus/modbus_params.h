/*
 * @Author: Alex.Pan
 * @Date: 2020-11-13 14:23:52
 * @LastEditTime: 2020-11-24 14:12:32
 * @LastEditors: Alex.Pan
 * @Description: 
 * @FilePath: \ESP32\project\SmartLight\components\modbus\modbus_params.h
 * @Copyright 2010-2015 LEKTEC or its affiliates. All Rights Reserved.
 */
/*=====================================================================================
 * Description:
 *   The Modbus parameter structures used to define Modbus instances that
 *   can be addressed by Modbus protocol. Define these structures per your needs in
 *   your application. Below is just an example of possible parameters.
 *====================================================================================*/
#ifndef _DEVICE_PARAMS
#define _DEVICE_PARAMS

// This file defines structure of modbus parameters which reflect correspond modbus address space
// for each modbus register type (coils, discreet inputs, holding registers, input registers)
// #pragma pack(push, 1)
// typedef struct
// {
//     uint8_t discrete_input0:1;
//     uint8_t discrete_input1:1;
//     uint8_t discrete_input2:1;
//     uint8_t discrete_input3:1;
//     uint8_t discrete_input4:1;
//     uint8_t discrete_input5:1;
//     uint8_t discrete_input6:1;
//     uint8_t discrete_input7:1;
//     uint8_t discrete_input_port1:8;
// } discrete_reg_params_t;
// #pragma pack(pop)

// #pragma pack(push, 1)
// typedef struct
// {
//     uint8_t coils_port0;
//     uint8_t coils_port1;
// } coil_reg_params_t;
// #pragma pack(pop)

// #pragma pack(push, 1)
// typedef struct
// {
//     float input_data0;
//     float input_data1;
//     float input_data2;
//     float input_data3;
//     uint16_t data[150];
// } input_reg_params_t;
// #pragma pack(pop)

// #pragma pack(push, 1)
// typedef struct
// {
//     float holding_data0;
//     float holding_data1;
//     float holding_data2;
//     float holding_data3;
//     uint16_t test_regs[150];
// } holding_reg_params_t;
// #pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    uint16_t smart_switch;
    uint32_t current_A;
    uint32_t current_B;
    uint32_t current_C;
    uint32_t voltage_A;
    uint32_t voltage_B;
    uint32_t voltage_C;
    uint32_t act_power_A;
    uint32_t act_power_B;
    uint32_t act_power_C;
    uint32_t cos_A;
    uint32_t cos_B;
    uint32_t cos_C;
    uint32_t te_A;
    uint32_t te_B;
    uint32_t te_C;
    uint16_t freq;
    uint16_t CT_A;
    uint16_t CT_B;
    uint16_t CT_C;
    uint16_t contorl;
    uint16_t ps_value;
    uint16_t ps_value_on;
    uint16_t ps_value_off;
} holding_reg_params_sms_t;
#pragma pack(pop)

// #pragma pack(push, 1)
// typedef struct
// {
//     uint8_t coils_d01;
//     uint8_t coils_d02;
//     uint8_t coils_d03;
//     uint8_t coils_d04;
//     uint8_t coils_mode;
//     uint8_t coils_d0gate;
// } coil_reg_params_sms_t;
// #pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    uint8_t coils_byte0;
} coil_reg_params_sms_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    uint8_t discrete_byte0;
    uint8_t discrete_byte1;
    uint8_t discrete_byte2;
    uint8_t discrete_byte3;
    uint8_t discrete_byte4;
    uint8_t discrete_byte5;
    uint8_t discrete_byte6;
    uint8_t discrete_byte7;
} discrete_reg_params_sms_t;
#pragma pack(pop)

extern coil_reg_params_sms_t coil_reg_params_sms;
extern holding_reg_params_sms_t holding_reg_params_sms;
extern discrete_reg_params_sms_t discrete_reg_params_sms;
// extern holding_reg_params_t holding_reg_params;
// extern input_reg_params_t input_reg_params;
// extern coil_reg_params_t coil_reg_params;
// extern discrete_reg_params_t discrete_reg_params;

#endif // !defined(_DEVICE_PARAMS)
