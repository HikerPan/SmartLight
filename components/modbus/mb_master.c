// Copyright 2016-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "string.h"
#include "esp_log.h"
#include "modbus_params.h"  // for modbus parameters structures
#include "mbcontroller.h"
#include "sdkconfig.h"

#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "cJSON.h"
#include "cmd_nvs.h"
#include "nvs.h"

#define MB_PORT_NUM     (CONFIG_MB_UART_PORT_NUM)   // Number of UART port used for Modbus connection
#define MB_DEV_SPEED    (CONFIG_MB_UART_BAUD_RATE)  // The communication speed of the UART
// #define MB_NAMESPACE    ("sl.modbus")
#define MB_SL_ID        ("sl_id")


// Note: Some pins on target chip cannot be assigned for UART communication.
// See UART documentation for selected board and target to configure pins using Kconfig.

// The number of parameters that intended to be used in the particular control process
#define MASTER_MAX_CIDS num_device_parameters

// Number of reading of parameters from slave
#define MASTER_MAX_RETRY 30

// Timeout to update cid over Modbus
#define UPDATE_CIDS_TIMEOUT_MS          (500)
#define UPDATE_CIDS_TIMEOUT_TICS        (UPDATE_CIDS_TIMEOUT_MS / portTICK_RATE_MS)

// Timeout between polls
#define POLL_TIMEOUT_MS                 (5)
#define POLL_TIMEOUT_TICS               (POLL_TIMEOUT_MS / portTICK_RATE_MS)

#define MASTER_TAG "modbus"

#define MASTER_CHECK(a, ret_val, str, ...) \
    if (!(a)) { \
        ESP_LOGE(MASTER_TAG, "%s(%u): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        return (ret_val); \
    }

// The macro to get offset for parameter in the appropriate structure
#define HOLD_OFFSET(field) ((uint16_t)(offsetof(holding_reg_params_sms_t, field) + 1))
// #define INPUT_OFFSET(field) ((uint16_t)(offsetof(input_reg_params_t, field) + 1))
#define COIL_OFFSET(field) ((uint16_t)(offsetof(coil_reg_params_sms_t, field) + 1))
// Discrete offset macro
#define DISCR_OFFSET(field) ((uint16_t)(offsetof(discrete_reg_params_sms_t, field) + 1))

#define STR(fieldname) ((const char*)( fieldname ))
// Options can be used as bit masks or parameter limits
#define OPTS(min_val, max_val, step_val) { .opt1 = min_val, .opt2 = max_val, .opt3 = step_val }

SemaphoreHandle_t mb_mux = NULL;

// Enumeration of modbus device addresses accessed by master device
enum
{
    MB_DEVICE_ADDR1 = 1 // Only one slave device used for the test (add other slave addresses here)
};

// Enumeration of all supported CIDs for device (used in parameter definition table)
enum
{
    CID_HOLD_ALL = 0,
    CID_HOLD_SWITCH,
    CID_HOLD_CURRENT_A,
    CID_HOLD_CURRENT_B,
    CID_HOLD_CURRENT_C,
    CID_HOLD_VOLTAGE_A,
    CID_HOLD_VOLTAGE_B,
    CID_HOLD_VOLTAGE_C,
    CID_HOLD_ACTIVE_POWER_A,  // 有功功率
    CID_HOLD_ACTIVE_POWER_B,
    CID_HOLD_ACTIVE_POWER_C,
    CID_HOLD_COS_A, // 功率因素
    CID_HOLD_COS_B,
    CID_HOLD_COS_C,
    CID_HOLD_TE_A, // 累计电能
    CID_HOLD_TE_B,
    CID_HOLD_TE_C,
    CID_HOLD_EWF,  //电网频率
    CID_HOLD_CT_A, // CT比
    CID_HOLD_CT_B,
    CID_HOLD_CT_C,
    CID_HOLD_CONTROL_MODE, //控制模式
    CID_HOLD_PS_VALUE,      //光敏值
    CID_HOLD_PS_ON_VALUE,   //光敏开灯值
    CID_HOLD_PS_OFF_VALUE,  //光敏关灯值
    // CID_COIL_ALL,
    CID_COIL,
    // CID_COIL_D01,
    // CID_COIL_D02,
    // CID_COIL_D03,
    // CID_COIL_D04,
    // CID_COIL_MODE,
    // CID_COIL_D0GATE,
    // CID_DISCRETE_ALL,
    CID_DISCRETE,
    // CID_DISCRETE_BIT0,
    // CID_DISCRETE_BIT1,
    // CID_DISCRETE_BIT2,
    // CID_DISCRETE_BIT3,
    // CID_DISCRETE_BIT4,
    // CID_DISCRETE_BIT5,
    // CID_DISCRETE_BIT6,
    // CID_DISCRETE_BIT7,
    CID_COUNT
};

// Example Data (Object) Dictionary for Modbus parameters:
// The CID field in the table must be unique.
// Modbus Slave Addr field defines slave address of the device with correspond parameter.
// Modbus Reg Type - Type of Modbus register area (Holding register, Input Register and such).
// Reg Start field defines the start Modbus register number and Reg Size defines the number of registers for the characteristic accordingly.
// The Instance Offset defines offset in the appropriate parameter structure that will be used as instance to save parameter value.
// Data Type, Data Size specify type of the characteristic and its data size.
// Parameter Options field specifies the options that can be used to process parameter value (limits or masks).
// Access Mode - can be used to implement custom options for processing of characteristic (Read/Write restrictions, factory mode values and etc).
const mb_parameter_descriptor_t device_parameters[] =
{
    // { CID, Param Name, Units, Modbus Slave Addr, Modbus Reg Type, Reg Start, Reg Size, Instance Offset, Data Type, Data Size, Parameter Options, Access Mode}
    {
        CID_HOLD_ALL, STR("Holding All"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 0, 39,
        HOLD_OFFSET(smart_switch), PARAM_TYPE_U16, 2, OPTS(0, 10000000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_SWITCH, STR("Switch"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 0, 1,
        HOLD_OFFSET(smart_switch), PARAM_TYPE_U16, 2, OPTS(0, 31, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_CURRENT_A, STR("Current_A"), STR("A"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 1, 2,
        HOLD_OFFSET(current_A), PARAM_TYPE_U32, 4, OPTS(0, 1000000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_CURRENT_B, STR("Current_B"), STR("A"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 3, 2,
        HOLD_OFFSET(current_B), PARAM_TYPE_U32, 4, OPTS(0, 1000000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_CURRENT_C, STR("Current_C"), STR("A"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 5, 2,
        HOLD_OFFSET(current_C), PARAM_TYPE_U32, 4, OPTS(0, 1000000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_VOLTAGE_A, STR("Voltage_A"), STR("V"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 7, 2,
        HOLD_OFFSET(voltage_A), PARAM_TYPE_U32, 4, OPTS(0, 1000000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_VOLTAGE_B, STR("Voltage_B"), STR("V"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 9, 2,
        HOLD_OFFSET(voltage_B), PARAM_TYPE_U32, 4, OPTS(0, 1000000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_VOLTAGE_C, STR("Voltage_C"), STR("V"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 11, 2,
        HOLD_OFFSET(voltage_C), PARAM_TYPE_U32, 4, OPTS(0, 1000000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_ACTIVE_POWER_A, STR("Active_Power_A"), STR("W"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 13, 2,
        HOLD_OFFSET(act_power_A), PARAM_TYPE_U32, 4, OPTS(0, 1000000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_ACTIVE_POWER_B, STR("Active_Power_B"), STR("W"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 15, 2,
        HOLD_OFFSET(act_power_B), PARAM_TYPE_U32, 4, OPTS(0, 1000000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_ACTIVE_POWER_C, STR("Active_Power_C"), STR("W"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 17, 2,
        HOLD_OFFSET(act_power_C), PARAM_TYPE_U32, 4, OPTS(0, 1000000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_COS_A, STR("COS_A"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 19, 2,
        HOLD_OFFSET(cos_A), PARAM_TYPE_U32, 4, OPTS(0, 1000000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_COS_B, STR("COS_B"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 21, 2,
        HOLD_OFFSET(cos_B), PARAM_TYPE_U32, 4, OPTS(0, 1000000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_COS_C, STR("COS_C"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 23, 2,
        HOLD_OFFSET(cos_C), PARAM_TYPE_U32, 4, OPTS(0, 1000000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_TE_A, STR("TE_A"), STR("KWh"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 25, 2,
        HOLD_OFFSET(te_A), PARAM_TYPE_U32, 4, OPTS(0, 1000000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_TE_B, STR("TE_B"), STR("KWh"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 27, 2,
        HOLD_OFFSET(te_B), PARAM_TYPE_U32, 4, OPTS(0, 1000000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_TE_C, STR("TE_C"), STR("KWh"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 29, 2,
        HOLD_OFFSET(te_C), PARAM_TYPE_U32, 4, OPTS(0, 1000000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_EWF, STR("FREQUENCY"), STR("HZ"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 31, 1,
        HOLD_OFFSET(freq), PARAM_TYPE_U16, 2, OPTS(0, 100, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_CT_A, STR("CT_A"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 32, 1,
        HOLD_OFFSET(CT_A), PARAM_TYPE_U16, 2, OPTS(0, 1000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_CT_B, STR("CT_B"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 33, 1,
        HOLD_OFFSET(CT_B), PARAM_TYPE_U16, 2, OPTS(0, 1000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_CT_C, STR("CT_C"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 34, 1,
        HOLD_OFFSET(CT_C), PARAM_TYPE_U16, 2, OPTS(0, 1000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_CONTROL_MODE, STR("CONTROL"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 35, 1,
        HOLD_OFFSET(contorl), PARAM_TYPE_U16, 2, OPTS(0, 1000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_PS_VALUE, STR("PS_VALUE"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 36, 1,
        HOLD_OFFSET(ps_value), PARAM_TYPE_U16, 2, OPTS(0, 1000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_PS_ON_VALUE, STR("PS_ON_VALUE"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 37, 1,
        HOLD_OFFSET(ps_value_on), PARAM_TYPE_U16, 2, OPTS(0, 1000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_HOLD_PS_OFF_VALUE, STR("PS_OFF_VALUE"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 38, 1,
        HOLD_OFFSET(ps_value_off), PARAM_TYPE_U16, 2, OPTS(0, 1000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_COIL, STR("COIL"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_COIL, 0, 6,
        COIL_OFFSET(coils_byte0), PARAM_TYPE_U8, 1, OPTS(0, 0, 0), PAR_PERMS_READ_WRITE_TRIGGER
    },
    {
        CID_DISCRETE, STR("DISCRETE"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_DISCRETE, 0, 6,
        DISCR_OFFSET(discrete_byte0), PARAM_TYPE_U8, 1, OPTS(0, 0, 0), PAR_PERMS_READ_WRITE_TRIGGER
    },
    // {
    //     CID_COIL_D01, STR("D01"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_COIL, 0, 1,
    //     COIL_OFFSET(coils_byte0), PARAM_TYPE_U8, 1, OPTS(1, 0, 1), PAR_PERMS_READ_WRITE_TRIGGER
    // },
    // {
    //     CID_COIL_D02, STR("D02"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_COIL, 0, 1,
    //     COIL_OFFSET(coils_byte0), PARAM_TYPE_U8, 1, OPTS(1, 1, 1), PAR_PERMS_READ_WRITE_TRIGGER
    // },
    // {
    //     CID_COIL_D03, STR("D03"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_COIL, 0, 1,
    //     COIL_OFFSET(coils_byte0), PARAM_TYPE_U8, 1, OPTS(1, 2, 1), PAR_PERMS_READ_WRITE_TRIGGER
    // },
    // {
    //     CID_COIL_D04, STR("D04"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_COIL, 0, 1,
    //     COIL_OFFSET(coils_byte0), PARAM_TYPE_U8, 1, OPTS(1, 3, 1), PAR_PERMS_READ_WRITE_TRIGGER
    // },
    // {
    //     CID_COIL_MODE, STR("MODE"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_COIL, 0, 1,
    //     COIL_OFFSET(coils_byte0), PARAM_TYPE_U8, 1, OPTS(1, 4, 1), PAR_PERMS_READ_WRITE_TRIGGER
    // },
    // {
    //     CID_COIL_D0GATE, STR("D0GATE"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_COIL, 0, 1,
    //     COIL_OFFSET(coils_byte0), PARAM_TYPE_U8, 1, OPTS(1, 5, 1), PAR_PERMS_READ_WRITE_TRIGGER
    // },
    // {
    //     CID_DISCRETE_ALL, STR("DISCRETE ALL"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_DISCRETE, 0, 6,
    //     DISCR_OFFSET(discrete_byte0), PARAM_TYPE_U8, 1, OPTS(0, 1000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    // },
    // {
    //     CID_DISCRETE_BIT0, STR("DISCRETE bit0"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_DISCRETE, 0, 1,
    //     DISCR_OFFSET(discrete_byte0), PARAM_TYPE_U8, 1, OPTS(0, 1000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    // },
    // {
    //     CID_DISCRETE_BIT1, STR("DISCRETE bit1"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_DISCRETE, 1, 1,
    //     DISCR_OFFSET(discrete_byte0), PARAM_TYPE_U8, 1, OPTS(0, 1000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    // },
    // {
    //     CID_DISCRETE_BIT2, STR("DISCRETE bit2"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_DISCRETE, 2, 1,
    //     DISCR_OFFSET(discrete_byte0), PARAM_TYPE_U8, 1, OPTS(0, 1000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    // },
    // {
    //     CID_DISCRETE_BIT3, STR("DISCRETE bit3"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_DISCRETE, 3, 1,
    //     DISCR_OFFSET(discrete_byte0), PARAM_TYPE_U8, 1, OPTS(0, 1000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    // },
    // {
    //     CID_DISCRETE_BIT4, STR("DISCRETE bit4"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_DISCRETE, 4, 1,
    //     DISCR_OFFSET(discrete_byte0), PARAM_TYPE_U8, 1, OPTS(0, 1000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    // },
    // {
    //     CID_DISCRETE_BIT5, STR("DISCRETE bit5"), STR("__"), MB_DEVICE_ADDR1, MB_PARAM_DISCRETE, 5, 1,
    //     DISCR_OFFSET(discrete_byte0), PARAM_TYPE_U8, 1, OPTS(0, 1000, 1), PAR_PERMS_READ_WRITE_TRIGGER
    // },

};



// Calculate number of parameters in the table
const uint16_t num_device_parameters = (sizeof(device_parameters) / sizeof(device_parameters[0]));

// The function to get pointer to parameter storage (instance) according to parameter description table
// static void *master_get_param_data(const mb_parameter_descriptor_t *param_descriptor)
// {
//     assert(param_descriptor != NULL);
//     void *instance_ptr = NULL;
//     if (param_descriptor->param_offset != 0)
//     {
//         switch (param_descriptor->mb_param_type)
//         {
//         case MB_PARAM_HOLDING:
//             instance_ptr = ((void *)&holding_reg_params_sms + param_descriptor->param_offset - 1);
//             break;
//         case MB_PARAM_INPUT:
//             // instance_ptr = ((void *)&input_reg_params + param_descriptor->param_offset - 1);
//             break;
//         case MB_PARAM_COIL:
//             instance_ptr = ((void *)&coil_reg_params_sms + param_descriptor->param_offset - 1);
//             break;
//         case MB_PARAM_DISCRETE:
//             // instance_ptr = ((void *)&discrete_reg_params + param_descriptor->param_offset - 1);
//             break;
//         default:
//             instance_ptr = NULL;
//             break;
//         }
//     }
//     else
//     {
//         ESP_LOGE(MASTER_TAG, "Wrong parameter offset for CID #%d", param_descriptor->cid);
//         assert(instance_ptr != NULL);
//     }
//     return instance_ptr;
// }


esp_err_t modbus_param_set(char *buf)
{
    cJSON *pJsonArr = NULL;
    cJSON *pJsonIteam = NULL;
    size_t value = 0;
    uint32_t iCount = 0;
    esp_err_t err = ESP_OK;
    const mb_parameter_descriptor_t *param_descriptor = NULL;
    uint8_t type = 0;

    if (buf == NULL)
    {
        ESP_LOGI(MASTER_TAG, "PSR_ParseHtmlData, pData is NULL.");
        return ESP_FAIL;
    }
    xSemaphoreTake(mb_mux, portMAX_DELAY);
    cJSON *pJsonRoot = cJSON_Parse(buf);
    if (pJsonRoot == NULL)
    {
        ESP_LOGI(MASTER_TAG, "cJSON_Parse failed, pDateStr:%s.", buf);
        goto ERROR;
    }

    iCount = cJSON_GetArraySize(pJsonRoot);
    ESP_LOGI(MASTER_TAG, "JSON Arry num:%d.", iCount);

    for (size_t uiLoop = 0; uiLoop < iCount; uiLoop++)
    {
        pJsonArr = cJSON_GetArrayItem(pJsonRoot, uiLoop);
        if (!cJSON_IsInvalid(pJsonArr))
        {
            /*查找ID*/
            pJsonIteam = cJSON_GetObjectItem(pJsonArr, "ID");
            if (NULL != pJsonIteam)
            {
                if (cJSON_IsString(pJsonIteam))
                {
                    char *id = cJSON_GetStringValue(pJsonIteam);
                    if (ESP_OK == set_str_in_nvs(MB_SL_ID, id))
                    {
                        ESP_LOGI(MASTER_TAG, "key %s Set to %s success.", MB_SL_ID, id);
                    }
                }

            }
        }


        // for (size_t cid = 0; cid < MASTER_MAX_CIDS; cid++)
        // {
        //     err = mbc_master_get_cid_info(cid, &param_descriptor);
        //     if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL))
        //     {
        //         pJsonIteam = cJSON_GetObjectItem(pJsonArr, param_descriptor->param_key);
        //         if (NULL != pJsonIteam)
        //         {
        //             if ((MB_PARAM_COIL == param_descriptor->mb_param_type) || (MB_PARAM_DISCRETE == param_descriptor->mb_param_type))
        //             {
        //                 /*首先读出寄存器的值*/
        //                 err = mbc_master_get_parameter(cid, (char *)param_descriptor->param_key, (uint8_t *)&value, &type);
        //                 if (ESP_OK == err)
        //                 {
        //                     ESP_LOGI(MASTER_TAG, "pre value is %d",value);
        //                     if (1 == pJsonIteam->valueint)
        //                         value |= (1 << param_descriptor->param_opts.opt2);
        //                     else
        //                         value &= (~(1 << param_descriptor->param_opts.opt2));

        //                     ESP_LOGI(MASTER_TAG, "%s is %d", param_descriptor->param_key, value);

        //                     mbc_master_set_parameter(cid, (char *)param_descriptor->param_key,
        //                                              (uint8_t *)&value, &type);
        //                 }
        //             }
        //             else
        //             {
        //                 if (cJSON_IsNumber(pJsonIteam))
        //                 {
        //                     ESP_LOGI(MASTER_TAG, "set key %s value %d", param_descriptor->param_key, pJsonIteam->valueint);
        //                     value = pJsonIteam->valueint;
        //                     mbc_master_set_parameter(cid, (char *)param_descriptor->param_key,
        //                                              (uint8_t *)&value, &type);
        //                     break;
        //                 }
        //             }
        //         }
        //     }
        // }

        for (size_t cid = 0; cid < MASTER_MAX_CIDS; cid++)
        {
            err = mbc_master_get_cid_info(cid, &param_descriptor);
            if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL))
            {
                pJsonIteam = cJSON_GetObjectItem(pJsonArr, param_descriptor->param_key);
                if (cJSON_IsNumber(pJsonIteam))
                {
                    ESP_LOGI(MASTER_TAG, "set key %s value %d", param_descriptor->param_key, pJsonIteam->valueint);
                    value = pJsonIteam->valueint;
                    mbc_master_set_parameter(cid, (char *)param_descriptor->param_key,
                                             (uint8_t *)&value, &type);
                    break;
                }
            }
        }
    }

    xSemaphoreGive(mb_mux);
    cJSON_Delete(pJsonRoot);
    return ESP_OK;

ERROR:

    xSemaphoreGive(mb_mux);

    cJSON_Delete(pJsonRoot);
    return ESP_FAIL;

}

// char *modbus_param_get(void)
// {
//     esp_err_t err = ESP_OK;
//     const mb_parameter_descriptor_t *param_descriptor = NULL;
//     uint32_t value = 0;
//     uint8_t type = 0;
//     cJSON *mb_root = NULL;
//     cJSON *mb_param = NULL;
//     cJSON *id_param = NULL;
//     char *buf = NULL;

//     xSemaphoreTake(mb_mux, portMAX_DELAY);
//     mb_root = cJSON_CreateArray();
//     if (NULL == mb_root)
//     {
//         return NULL;
//     }
//     id_param = cJSON_CreateObject();
//     if (NULL == id_param)
//     {
//         goto ERROR;
//     }

//     /* 首先处理ID 字符串*/
//     char *str = get_str_from_nvs(MB_SL_ID);
//     if (NULL != str)
//     {
//         cJSON_AddStringToObject(id_param, "ID", str);
//         cJSON_AddItemToArray(mb_root, id_param);
//         free(str);
//     }
//     else
//     {
//         cJSON_AddNullToObject(id_param, "ID");
//         cJSON_AddItemToArray(mb_root, id_param);
//     }

//     // 一次性处理hold寄存器
//     // for (size_t cid = 0; cid < MASTER_MAX_CIDS; cid++)
//     {
//         err = mbc_master_get_cid_info(CID_HOLD_ALL, &param_descriptor);
//         if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL))
//         {
//             // 该cid一次性读取所有hold寄存器的数据
//             if(0==strcmp(param_descriptor->param_key,"Holding All"))
//             {
//                 // 整体读取
//                 err = mbc_master_get_parameter(CID_HOLD_ALL, (char *)param_descriptor->param_key,
//                                            (uint8_t *)&holding_reg_params_sms, &type);
//                 if (err == ESP_OK)
//                 {
//                     for(size_t i=CID_HOLD_ALL+1;i<MASTER_MAX_CIDS;i++)
//                     {
//                         err = mbc_master_get_cid_info(i, &param_descriptor);
//                         if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL))
//                         {
//                             if(MB_PARAM_HOLDING == param_descriptor->mb_param_type)
//                             {
//                                 uint8_t *pdate = (uint8_t *)&holding_reg_params_sms+param_descriptor->param_offset-1;
//                                 value = 0;
//                                 if(2 == param_descriptor->mb_size)
//                                 {
//                                     value += pdate[0];
//                                     value += pdate[1]<<8;
//                                     value += pdate[2]<<16;
//                                     value += pdate[3]<<24;
//                                 }
//                                 else if(1 == param_descriptor->mb_size)
//                                 {
//                                     value += pdate[0];
//                                     value += pdate[1]<<8;
//                                 }
//                                 else
//                                 {
//                                     ESP_LOGW(MASTER_TAG,"not support");
//                                 }

//                                 ESP_LOGD(MASTER_TAG,"key %s:value %d",param_descriptor->param_key,value);

//                                 mb_param = NULL;
//                                 mb_param = cJSON_CreateObject();
//                                 if (NULL == mb_param)
//                                 {
//                                     goto ERROR;
//                                 }

//                                 if (!strcmp(param_descriptor->param_key, "CT_A"))
//                                 {
//                                     cJSON_AddNumberToObject(mb_param, "CT_A_H", (value >> 8) & 0x000000FF);
//                                     cJSON_AddNumberToObject(mb_param, "CT_A_L", value & 0x000000FF);
//                                 }
//                                 else if (!strcmp(param_descriptor->param_key, "CT_B"))
//                                 {
//                                     cJSON_AddNumberToObject(mb_param, "CT_B_H", (value >> 8) & 0x000000FF);
//                                     cJSON_AddNumberToObject(mb_param, "CT_B_L", value & 0x000000FF);
//                                 }
//                                 else if (!strcmp(param_descriptor->param_key, "CT_C"))
//                                 {
//                                     cJSON_AddNumberToObject(mb_param, "CT_C_H", (value >> 8) & 0x000000FF);
//                                     cJSON_AddNumberToObject(mb_param, "CT_C_L", value & 0x000000FF);
//                                 }
//                                 else
//                                 {
//                                     cJSON_AddNumberToObject(mb_param, param_descriptor->param_key, value);
//                                 }

//                                 cJSON_AddItemToArray(mb_root, mb_param);

//                                 // ESP_LOGI(MASTER_TAG,"value = %d",value);
//                                 // ESP_LOG_BUFFER_HEXDUMP(MASTER_TAG, pdate, sizeof(holding_reg_params_sms), ESP_LOG_INFO);
//                             }
//                             else
//                             {
//                                 // holding 结束
//                                 break;
//                             }
//                         }
//                     }
//                 }
//             }
//         }
//     }

//     // 一次性处理coil寄存器
//     // for (size_t cid = 0; cid < MASTER_MAX_CIDS; cid++)
//     {
//         err = mbc_master_get_cid_info(CID_COIL_ALL, &param_descriptor);
//         if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL))
//         {
//             // 该cid一次性读取所有coil寄存器的数据
//             if(0==strcmp(param_descriptor->param_key,"COIL ALL"))
//             {
//                 // 整体读取
//                 err = mbc_master_get_parameter(CID_COIL_ALL, (char *)param_descriptor->param_key,
//                                            (uint8_t *)&coil_reg_params_sms, &type);
//                 if (err == ESP_OK)
//                 {
//                     for(size_t i=CID_COIL_ALL+1;i<MASTER_MAX_CIDS;i++)
//                     {
//                         err = mbc_master_get_cid_info(i, &param_descriptor);
//                         if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL))
//                         {
//                             if(MB_PARAM_COIL == param_descriptor->mb_param_type)
//                             {
//                                 // uint8_t *pdate = (uint8_t *)&coil_reg_params_sms+param_descriptor->param_offset-1;
//                                 uint8_t *pdate = (uint8_t *)&coil_reg_params_sms;
//                                 value = *pdate;

//                                 ESP_LOGI(MASTER_TAG,"key %s:value %d",param_descriptor->param_key,value);

//                                 mb_param = NULL;
//                                 mb_param = cJSON_CreateObject();
//                                 if (NULL == mb_param)
//                                 {
//                                     goto ERROR;
//                                 }

//                                 if(value&(1<<(i-(CID_COIL_ALL+1))))
//                                     cJSON_AddTrueToObject(mb_param, param_descriptor->param_key);
//                                 else
//                                     cJSON_AddFalseToObject(mb_param, param_descriptor->param_key);
//                                 // cJSON_AddNumberToObject(mb_param, param_descriptor->param_key, value);
//                                 cJSON_AddItemToArray(mb_root, mb_param);

//                                 // ESP_LOGI(MASTER_TAG,"value = %d",value);
//                                 // ESP_LOG_BUFFER_HEXDUMP(MASTER_TAG, pdate, sizeof(holding_reg_params_sms), ESP_LOG_INFO);
//                             }
//                             else
//                             {
//                                 // holding 结束
//                                 break;
//                             }
//                         }
//                     }
//                 }
//             }
//         }
//     }


// #if 0
//     for (size_t cid = 0; cid < MASTER_MAX_CIDS; cid++)
//     {
//         err = mbc_master_get_cid_info(cid, &param_descriptor);
//         if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL))
//         {
//             mb_param = NULL;
//             mb_param = cJSON_CreateObject();
//             if (NULL == mb_param)
//             {
//                 goto ERROR;
//             }
//             err = mbc_master_get_parameter(cid, (char *)param_descriptor->param_key,
//                                            (uint8_t *)&value, &type);
//             if (err == ESP_OK)
//             {
//                 if (!strcmp(param_descriptor->param_key, "CT_A"))
//                 {
//                     cJSON_AddNumberToObject(mb_param, "CT_A_H", (value >> 8) & 0x000000FF);
//                     cJSON_AddNumberToObject(mb_param, "CT_A_L", value & 0x000000FF);
//                 }
//                 else if (!strcmp(param_descriptor->param_key, "CT_B"))
//                 {
//                     cJSON_AddNumberToObject(mb_param, "CT_B_H", (value >> 8) & 0x000000FF);
//                     cJSON_AddNumberToObject(mb_param, "CT_B_L", value & 0x000000FF);
//                 }
//                 else if (!strcmp(param_descriptor->param_key, "CT_C"))
//                 {
//                     cJSON_AddNumberToObject(mb_param, "CT_C_H", (value >> 8) & 0x000000FF);
//                     cJSON_AddNumberToObject(mb_param, "CT_C_L", value & 0x000000FF);
//                 }
//                 else
//                 {
//                     cJSON_AddNumberToObject(mb_param, param_descriptor->param_key, value);
//                 }
//             }
//             else
//             {
//                 ESP_LOGW(MASTER_TAG,"cid %d error %s",cid,esp_err_to_name(err));
//                 if (!strcmp(param_descriptor->param_key, "CT_A"))
//                 {
//                     cJSON_AddNullToObject(mb_param, "CT_A_H");
//                     cJSON_AddNullToObject(mb_param, "CT_A_L");
//                 }
//                 else if (!strcmp(param_descriptor->param_key, "CT_B"))
//                 {
//                     cJSON_AddNullToObject(mb_param, "CT_B_H");
//                     cJSON_AddNullToObject(mb_param, "CT_B_L");
//                 }
//                 else if (!strcmp(param_descriptor->param_key, "CT_C"))
//                 {
//                     cJSON_AddNullToObject(mb_param, "CT_C_H");
//                     cJSON_AddNullToObject(mb_param, "CT_C_L");
//                 }
//                 else
//                 {
//                     cJSON_AddNullToObject(mb_param, param_descriptor->param_key);
//                 }
//             }
//             cJSON_AddItemToArray(mb_root, mb_param);
//         }
//         vTaskDelay(POLL_TIMEOUT_TICS);
//     }
// #endif
//     buf = cJSON_Print(mb_root);//cJSON_PrintUnformatted(mb_root);
//     if (NULL == buf)
//     {
//         goto ERROR;
//     }

//     if (NULL != mb_root)
//     {
//         cJSON_Delete(mb_root);
//         mb_root = NULL;
//     }

//     xSemaphoreGive(mb_mux);

//     return buf;

// ERROR:

//     if (NULL != mb_param)
//     {
//         cJSON_Delete(mb_param);
//         mb_root = NULL;
//     }

//     if (NULL != mb_root)
//     {
//         cJSON_Delete(mb_root);
//         mb_root = NULL;
//     }

//     if (NULL != buf)
//     {
//         free(buf);
//         buf = NULL;
//     }

//     xSemaphoreGive(mb_mux);

//     return buf;

// }


char *modbus_param_get(void)
{
    esp_err_t err = ESP_OK;
    const mb_parameter_descriptor_t *param_descriptor = NULL;
    uint32_t value = 0;
    uint8_t type = 0;
    cJSON *mb_root = NULL;
    cJSON *mb_param = NULL;
    cJSON *id_param = NULL;
    char *buf = NULL;

    xSemaphoreTake(mb_mux, portMAX_DELAY);
    mb_root = cJSON_CreateArray();
    if (NULL == mb_root)
    {
        return NULL;
    }
    id_param = cJSON_CreateObject();
    if (NULL == id_param)
    {
        goto ERROR;
    }

    /* 首先处理ID 字符串*/
    char *str = get_str_from_nvs(MB_SL_ID);
    if (NULL != str)
    {
        cJSON_AddStringToObject(id_param, "ID", str);
        cJSON_AddItemToArray(mb_root, id_param);
        free(str);
    }
    else
    {
        cJSON_AddNullToObject(id_param, "ID");
        cJSON_AddItemToArray(mb_root, id_param);
    }
#if 1
    // 一次性处理hold寄存器

    err = mbc_master_get_cid_info(CID_HOLD_ALL, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL))
    {
        // 该cid一次性读取所有hold寄存器的数据
        if (0 == strcmp(param_descriptor->param_key, "Holding All"))
        {

            // 整体读取
            err = ESP_ERR_TIMEOUT;
            for (int retry = 0; (retry < MASTER_MAX_RETRY) && (err != ESP_OK); retry++) {
                err = mbc_master_get_parameter(CID_HOLD_ALL, (char*)param_descriptor->param_key, 
                                                                (uint8_t*)&holding_reg_params_sms, &type);
            }
            // err = mbc_master_get_parameter(CID_HOLD_ALL, (char *)param_descriptor->param_key,
            //                                 (uint8_t *)&holding_reg_params_sms, &type);
            if (err == ESP_OK)
            {
                for (size_t i = CID_HOLD_ALL + 1; i < MASTER_MAX_CIDS; i++)
                {
                    err = mbc_master_get_cid_info(i, &param_descriptor);
                    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL))
                    {
                        if (MB_PARAM_HOLDING == param_descriptor->mb_param_type)
                        {
                            uint8_t *pdate = (uint8_t *)&holding_reg_params_sms + param_descriptor->param_offset - 1;
                            value = 0;
                            if (2 == param_descriptor->mb_size)
                            {
                                value += pdate[0];
                                value += pdate[1] << 8;
                                value += pdate[2] << 16;
                                value += pdate[3] << 24;
                            }
                            else if (1 == param_descriptor->mb_size)
                            {
                                value += pdate[0];
                                value += pdate[1] << 8;
                            }
                            else
                            {
                                ESP_LOGW(MASTER_TAG, "not support");
                            }

                            ESP_LOGD(MASTER_TAG, "key %s:value %d", param_descriptor->param_key, value);

                            mb_param = NULL;
                            mb_param = cJSON_CreateObject();
                            if (NULL == mb_param)
                            {
                                goto ERROR;
                            }

                            if (!strcmp(param_descriptor->param_key, "CT_A"))
                            {
                                cJSON_AddNumberToObject(mb_param, "CT_A_H", (value >> 8) & 0x000000FF);
                                cJSON_AddNumberToObject(mb_param, "CT_A_L", value & 0x000000FF);
                            }
                            else if (!strcmp(param_descriptor->param_key, "CT_B"))
                            {
                                cJSON_AddNumberToObject(mb_param, "CT_B_H", (value >> 8) & 0x000000FF);
                                cJSON_AddNumberToObject(mb_param, "CT_B_L", value & 0x000000FF);
                            }
                            else if (!strcmp(param_descriptor->param_key, "CT_C"))
                            {
                                cJSON_AddNumberToObject(mb_param, "CT_C_H", (value >> 8) & 0x000000FF);
                                cJSON_AddNumberToObject(mb_param, "CT_C_L", value & 0x000000FF);
                            }
                            else
                            {
                                cJSON_AddNumberToObject(mb_param, param_descriptor->param_key, value);
                            }

                            cJSON_AddItemToArray(mb_root, mb_param);

                            // ESP_LOGI(MASTER_TAG,"value = %d",value);
                            // ESP_LOG_BUFFER_HEXDUMP(MASTER_TAG, pdate, sizeof(holding_reg_params_sms), ESP_LOG_INFO);
                        }
                        else
                        {
                            // holding 结束
                            break;
                        }
                    }
                }
            }
        }
    }


    // 一次性处理coil寄存器
    err = mbc_master_get_cid_info(CID_COIL, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL))
    {
        err = ESP_ERR_TIMEOUT;
        for (int retry = 0; (retry < MASTER_MAX_RETRY) && (err != ESP_OK); retry++) {
            err = mbc_master_get_parameter(CID_COIL, (char*)param_descriptor->param_key, 
                                                            (uint8_t*)&coil_reg_params_sms, &type);
        }
        // err = mbc_master_get_parameter(CID_COIL, (char *)param_descriptor->param_key,
        //                                     (uint8_t *)&coil_reg_params_sms, &type);
        if(ESP_OK == err)
        {
            mb_param = NULL;
            mb_param = cJSON_CreateObject();
            if (NULL == mb_param)
            {
                goto ERROR;
            }
            cJSON_AddNumberToObject(mb_param, param_descriptor->param_key, coil_reg_params_sms.coils_byte0);
            cJSON_AddItemToArray(mb_root, mb_param);
        }
    }


    // 一次性处理DISCRETE寄存器
    err = mbc_master_get_cid_info(CID_DISCRETE, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL))
    {
        err = ESP_ERR_TIMEOUT;
        for (int retry = 0; (retry < MASTER_MAX_RETRY) && (err != ESP_OK); retry++) {
            err = mbc_master_get_parameter(CID_DISCRETE, (char*)param_descriptor->param_key, 
                                                            (uint8_t*)&discrete_reg_params_sms, &type);
        }
        // err = mbc_master_get_parameter(CID_DISCRETE, (char *)param_descriptor->param_key,
        //                                     (uint8_t *)&discrete_reg_params_sms, &type);
        if(ESP_OK == err)
        {
            mb_param = NULL;
            mb_param = cJSON_CreateObject();
            if (NULL == mb_param)
            {
                goto ERROR;
            }
            cJSON_AddNumberToObject(mb_param, param_descriptor->param_key, discrete_reg_params_sms.discrete_byte0);
            cJSON_AddItemToArray(mb_root, mb_param);
        } 
    }
#endif  

#if 0
    for (size_t cid = 0; cid < MASTER_MAX_CIDS; cid++)
    {
        err = mbc_master_get_cid_info(cid, &param_descriptor);
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL))
        {
            mb_param = NULL;
            mb_param = cJSON_CreateObject();
            if (NULL == mb_param)
            {
                goto ERROR;
            }

            err = ESP_ERR_TIMEOUT;
            for (int retry = 0; (retry < MASTER_MAX_RETRY) && (err != ESP_OK); retry++) {
                err = mbc_master_get_parameter(CID_DISCRETE, (char*)param_descriptor->param_key, 
                                                                (uint8_t*)&value, &type);
            }
            // err = mbc_master_get_parameter(cid, (char *)param_descriptor->param_key,
            //                                (uint8_t *)&value, &type);
            if (err == ESP_OK)
            {
                if (!strcmp(param_descriptor->param_key, "CT_A"))
                {
                    cJSON_AddNumberToObject(mb_param, "CT_A_H", (value >> 8) & 0x000000FF);
                    cJSON_AddNumberToObject(mb_param, "CT_A_L", value & 0x000000FF);
                }
                else if (!strcmp(param_descriptor->param_key, "CT_B"))
                {
                    cJSON_AddNumberToObject(mb_param, "CT_B_H", (value >> 8) & 0x000000FF);
                    cJSON_AddNumberToObject(mb_param, "CT_B_L", value & 0x000000FF);
                }
                else if (!strcmp(param_descriptor->param_key, "CT_C"))
                {
                    cJSON_AddNumberToObject(mb_param, "CT_C_H", (value >> 8) & 0x000000FF);
                    cJSON_AddNumberToObject(mb_param, "CT_C_L", value & 0x000000FF);
                }
                else
                {
                    cJSON_AddNumberToObject(mb_param, param_descriptor->param_key, value);
                }
            }
            else
            {
                ESP_LOGW(MASTER_TAG, "cid %d error %s", cid, esp_err_to_name(err));
                if (!strcmp(param_descriptor->param_key, "CT_A"))
                {
                    cJSON_AddNullToObject(mb_param, "CT_A_H");
                    cJSON_AddNullToObject(mb_param, "CT_A_L");
                }
                else if (!strcmp(param_descriptor->param_key, "CT_B"))
                {
                    cJSON_AddNullToObject(mb_param, "CT_B_H");
                    cJSON_AddNullToObject(mb_param, "CT_B_L");
                }
                else if (!strcmp(param_descriptor->param_key, "CT_C"))
                {
                    cJSON_AddNullToObject(mb_param, "CT_C_H");
                    cJSON_AddNullToObject(mb_param, "CT_C_L");
                }
                else
                {
                    cJSON_AddNullToObject(mb_param, param_descriptor->param_key);
                }
            }
            cJSON_AddItemToArray(mb_root, mb_param);
        }
        vTaskDelay(POLL_TIMEOUT_TICS);
    }
#endif
    buf = cJSON_Print(mb_root);//cJSON_PrintUnformatted(mb_root);
    if (NULL == buf)
    {
        goto ERROR;
    }

    if (NULL != mb_root)
    {
        cJSON_Delete(mb_root);
        mb_root = NULL;
    }

    xSemaphoreGive(mb_mux);

    return buf;

ERROR:

    if (NULL != mb_param)
    {
        cJSON_Delete(mb_param);
        mb_root = NULL;
    }

    if (NULL != mb_root)
    {
        cJSON_Delete(mb_root);
        mb_root = NULL;
    }

    if (NULL != buf)
    {
        free(buf);
        buf = NULL;
    }

    xSemaphoreGive(mb_mux);

    return buf;

}


// Modbus master initialization
esp_err_t master_init(void)
{
    // Initialize and start Modbus controller
    mb_communication_info_t comm =
    {
        .port = MB_PORT_NUM,
#if CONFIG_MB_COMM_MODE_ASCII
        .mode = MB_MODE_ASCII,
#elif CONFIG_MB_COMM_MODE_RTU
        .mode = MB_MODE_RTU,
#endif
        .baudrate = MB_DEV_SPEED,
        .parity = MB_PARITY_NONE
    };
    void *master_handler = NULL;

    esp_err_t err = mbc_master_init(MB_PORT_SERIAL_MASTER, &master_handler);
    MASTER_CHECK((master_handler != NULL), ESP_ERR_INVALID_STATE,
                 "mb controller initialization fail.");
    MASTER_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
                 "mb controller initialization fail, returns(0x%x).",
                 (uint32_t)err);
    err = mbc_master_setup((void *)&comm);
    MASTER_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
                 "mb controller setup fail, returns(0x%x).",
                 (uint32_t)err);

    // Set UART pin numbers
    err = uart_set_pin(MB_PORT_NUM, CONFIG_MB_UART_TXD, CONFIG_MB_UART_RXD,
                       CONFIG_MB_UART_RTS, UART_PIN_NO_CHANGE);

    err = mbc_master_start();
    MASTER_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
                 "mb controller start fail, returns(0x%x).",
                 (uint32_t)err);

    MASTER_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
                 "mb serial set pin failure, uart_set_pin() returned (0x%x).", (uint32_t)err);
    // Set driver mode to Half Duplex
    err = uart_set_mode(MB_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX);
    MASTER_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
                 "mb serial set mode failure, uart_set_mode() returned (0x%x).", (uint32_t)err);

    mb_mux = xSemaphoreCreateMutex();
    vTaskDelay(5);
    err = mbc_master_set_descriptor(&device_parameters[0], num_device_parameters);
    MASTER_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
                 "mb controller set descriptor fail, returns(0x%x).",
                 (uint32_t)err);
    ESP_LOGI(MASTER_TAG, "Modbus master stack initialized...");
    return err;
}


/** Arguments used by 'mb_test' function */
static struct
{
    struct arg_str *subcmd;
    struct arg_int *cid;
    struct arg_int *value;
    struct arg_end *end;
} mb_args;


static int mb_command(int argc, char **argv)
{
    uint32_t value = 0;
    uint8_t type = 0;
    const mb_parameter_descriptor_t *param_descriptor = NULL;
    // esp_err_t err = ESP_OK;
    // size_t cid = 0;

    int nerrors = arg_parse(argc, argv, (void **) &mb_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, mb_args.end, argv[0]);
        return 1;
    }

//    const uint16_t num_device_parameters = (sizeof(device_parameters)/sizeof(device_parameters[0]));
    // ESP_LOGW(MASTER_TAG,"sub cmd str %s",mb_args.subcmd->sval[0]);
    if (!strcmp(mb_args.subcmd->sval[0], "list"))
    {
        printf("\nList modbus parameter table:\n");
        printf("  CID   TYPE   reg start   KEY            \n");

        for (size_t i = 0; i < (sizeof(device_parameters) / sizeof(device_parameters[0])); i++)
        {
            printf(" %3d   %3d      0x%2x      %s          \n", device_parameters[i].cid, \
                   device_parameters[i].mb_param_type, \
                   device_parameters[i].mb_reg_start, \
                   device_parameters[i].param_key);
        }
    }
    else if (!strcmp(mb_args.subcmd->sval[0], "get"))
    {
        size_t cid = mb_args.cid->ival[0];
        xSemaphoreTake(mb_mux, portMAX_DELAY);
        esp_err_t err = mbc_master_get_cid_info(cid, &param_descriptor);
        type = param_descriptor->param_type;
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL))
        {
            err = mbc_master_get_parameter(cid, (char *)param_descriptor->param_key,
                                           (uint8_t *)&value, &type);
            // if ((err == ESP_OK)&&(type == param_descriptor->mb_param_type))
            if (err == ESP_OK)
            {
                ESP_LOGI(MASTER_TAG, "type %d, param_descriptor type %d", type, param_descriptor->mb_param_type);
                ESP_LOGI(MASTER_TAG, "Characteristic #%d %s (%s) value = %d read successful.",
                         param_descriptor->cid,
                         (char *)param_descriptor->param_key,
                         (char *)param_descriptor->param_units,
                         value);
            }
        }
        xSemaphoreGive(mb_mux);
    }
    else if (!strcmp(mb_args.subcmd->sval[0], "set"))
    {
        size_t cid = mb_args.cid->ival[0];
        xSemaphoreTake(mb_mux, portMAX_DELAY);
        esp_err_t err = mbc_master_get_cid_info(cid, &param_descriptor);
        // type = param_descriptor->param_type;
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL))
        {
            value = mb_args.value->ival[0];
            ESP_LOGW(MASTER_TAG, " key %s, set value %d", param_descriptor->param_key, value);
            err = mbc_master_set_parameter(cid, (char *)param_descriptor->param_key,
                                           (uint8_t *)&value, &type);
            if (err == ESP_OK)
            {
                ESP_LOGI(MASTER_TAG, "Characteristic #%d %s (%s) value = %d write successful.",
                         param_descriptor->cid,
                         (char *)param_descriptor->param_key,
                         (char *)param_descriptor->param_units,
                         value);
            }
        }
        xSemaphoreGive(mb_mux);
    }
    else
    {
        ESP_LOGE(MASTER_TAG, "cmd not support");
    }

    return 0;
}

void register_modbus(void)
{
    mb_args.subcmd = arg_str0(NULL, NULL, "<subcmd>", "the subcmd of modbus");
    mb_args.cid = arg_int0(NULL, NULL, "<cid>", "cid value");
    mb_args.value = arg_int0(NULL, NULL, "<value>", "value of key");
    mb_args.end = arg_end(2);

    const esp_console_cmd_t mb_cmd =
    {
        .command = "modbus",
        .help = "modbus command ",
        .hint = NULL,
        .func = &mb_command,
        .argtable = &mb_args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&mb_cmd));
}



