idf_component_register(SRCS "mb_master.c" "modbus_params.c"
                    INCLUDE_DIRS .
                   REQUIRES console freemodbus json cmd_nvs)

		