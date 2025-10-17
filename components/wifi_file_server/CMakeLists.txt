idf_component_register(
    SRCS "wifi_file_server.c"
    INCLUDE_DIRS "include"
    EMBED_FILES
        "www/index.html"
        "www/style.css"
        "www/app.js"
    REQUIRES
        esp_wifi
        esp_netif
        esp_event
        esp_http_server
        json
)
