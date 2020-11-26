idf_component_register(SRCS "web_server.c"
                    INCLUDE_DIRS .
                    REQUIRES esp_http_server spiffs json esp_littlefs esp_https_server esp_https_ota modbus app_update
					EMBED_TXTFILES "certs/cacert.pem"
                                   "certs/prvtkey.pem")