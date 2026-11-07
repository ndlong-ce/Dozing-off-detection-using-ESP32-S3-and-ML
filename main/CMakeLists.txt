idf_component_register(
    SRCS
        "main.cpp"
        "shared.cpp"
        "wifi_mgr.cpp"
        "web_server.cpp"
        "camera.cpp"
        "eye_model.cpp"
        "eye_monitor.cpp"
        "overlay.cpp"
        "buzzer.cpp"
        "led.cpp"
        "alarm_logic.cpp"
    INCLUDE_DIRS "."
    EMBED_FILES "eye_cnn.espdl"
    PRIV_REQUIRES driver esp_http_server esp_wifi esp_netif esp_event nvs_flash esp_timer
)
