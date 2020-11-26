idf_component_register(SRCS "cmd_fs.c"
                    INCLUDE_DIRS .
                    REQUIRES console vfs esp_littlefs)