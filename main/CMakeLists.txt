idf_component_register(
    SRCS "secure_fota.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_gpio app_update nvs_flash esp_event esp_wifi esp_http_client esp_https_ota esp_http_server
	
)
