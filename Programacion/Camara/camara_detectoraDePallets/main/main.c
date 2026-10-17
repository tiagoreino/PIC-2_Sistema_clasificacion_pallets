#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <esp_camera.h>

#include <network_provisioning/manager.h>
#include <network_provisioning/scheme_ble.h>

#include <lwip/sockets.h>
#include <lwip/inet.h>

#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK     0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_Y9      35
#define CAM_PIN_Y8      34
#define CAM_PIN_Y7      39
#define CAM_PIN_Y6      36
#define CAM_PIN_Y5      21
#define CAM_PIN_Y4      19
#define CAM_PIN_Y3      18
#define CAM_PIN_Y2       5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

#define LISTEN_PORT 5000

#define EXAMPLE_PROV_SEC2_USERNAME "CamaraPallets"
#define EXAMPLE_PROV_SEC2_PWD      "utec2026"

static const char *TAG = "app";

static const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;

static esp_err_t init_camera(void);
static void tcp_server_task(void *pvParameters);
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data);
static void get_device_service_name(char *service_name, size_t max);
static void wifi_init_sta(void);

static const char sec2_salt[] = {
    0xf3, 0xdb, 0xba, 0xe1, 0x3c, 0xdf, 0x9a, 0xa0, 0x97, 0x53, 0x7f, 0xd4, 0x54, 0x89, 0x3c, 0xe1
};

static const char sec2_verifier[] = {
    0x13, 0x35, 0x4c, 0x59, 0x09, 0x6a, 0x34, 0x56, 0x62, 0x17, 0x23, 0xb2, 0x5b, 0x1d, 0xfd, 0xfa,
    0xb9, 0x49, 0x58, 0x25, 0x8e, 0x67, 0xa2, 0xd8, 0x64, 0xe2, 0x9b, 0x40, 0x8c, 0x18, 0x3f, 0x85,
    0x65, 0x86, 0xa8, 0xe5, 0x41, 0x4e, 0x71, 0x97, 0xca, 0x28, 0x95, 0xc7, 0x7a, 0xd9, 0x28, 0x2e,
    0x5e, 0x59, 0x5b, 0xe4, 0x0a, 0x54, 0x65, 0x3c, 0x45, 0xba, 0x9e, 0x96, 0xd9, 0x15, 0x52, 0x5d,
    0x56, 0x6c, 0xf7, 0x5f, 0x3b, 0x2a, 0xd9, 0x6f, 0xae, 0x7f, 0x83, 0x5f, 0x2e, 0xd1, 0xbf, 0x4d,
    0x94, 0xfe, 0xe6, 0x7c, 0xa0, 0xbf, 0x51, 0x84, 0x87, 0x53, 0x9e, 0x22, 0x93, 0xcd, 0x4b, 0x78,
    0x2e, 0x6f, 0x62, 0xf2, 0x40, 0x82, 0x13, 0x0c, 0x0e, 0x3e, 0x50, 0xed, 0xe0, 0x68, 0x20, 0x02,
    0xd0, 0x96, 0xc1, 0xe3, 0x62, 0x6e, 0x7c, 0xe4, 0xd1, 0x76, 0xad, 0x7a, 0x8c, 0x12, 0x52, 0x8a,
    0xa5, 0xec, 0x8a, 0x9b, 0xd4, 0x4d, 0x33, 0x86, 0xd5, 0x1c, 0xdc, 0x13, 0x2c, 0x26, 0x5c, 0x51,
    0x6a, 0x27, 0xfd, 0xbf, 0xcb, 0x51, 0xc7, 0x05, 0xce, 0xb5, 0xfb, 0x93, 0xd3, 0xa6, 0xe5, 0x2d,
    0xb5, 0x00, 0xec, 0x21, 0x56, 0xd9, 0xb5, 0x00, 0x88, 0xf2, 0xd4, 0x56, 0x19, 0x8a, 0xac, 0x86,
    0xa3, 0x6d, 0x0f, 0xa5, 0x53, 0x87, 0xb9, 0x9f, 0xf4, 0x88, 0x0e, 0x78, 0xad, 0xf7, 0x54, 0x1a,
    0xfe, 0x5b, 0x79, 0x12, 0xc0, 0x94, 0x00, 0x26, 0x67, 0x6f, 0xfc, 0x7e, 0x32, 0xc7, 0x49, 0x13,
    0x53, 0xe5, 0x25, 0x51, 0x33, 0xdf, 0xb5, 0x72, 0x2e, 0xbe, 0x11, 0x12, 0xf6, 0x3d, 0x38, 0x0b,
    0xc6, 0xba, 0xed, 0x85, 0x51, 0x72, 0x61, 0xcc, 0x55, 0x89, 0xb4, 0xf3, 0xfd, 0x02, 0x61, 0x6e,
    0xa5, 0x3b, 0x2f, 0x44, 0x13, 0x90, 0x8d, 0xcd, 0xa4, 0x99, 0x85, 0x71, 0x21, 0x73, 0x3e, 0x47,
    0x4f, 0x4e, 0x54, 0x4e, 0xda, 0x36, 0xe9, 0x52, 0x66, 0x7a, 0xd5, 0x80, 0x54, 0xf4, 0xe6, 0x8b,
    0xb7, 0xe0, 0xbb, 0x1c, 0xb4, 0xab, 0xc6, 0x1f, 0xe7, 0x9b, 0x65, 0x34, 0x0f, 0x91, 0xae, 0xd1,
    0xe9, 0xf0, 0x8d, 0x69, 0x87, 0x09, 0x3a, 0x39, 0x3c, 0xe3, 0x6d, 0xad, 0xe2, 0x5d, 0x18, 0x55,
    0x2f, 0xd3, 0x03, 0x45, 0xbe, 0xf7, 0x10, 0xce, 0x24, 0x71, 0xa6, 0xbc, 0xa5, 0x4c, 0x6f, 0xc1,
    0x4a, 0x35, 0x1a, 0x5e, 0xa2, 0x72, 0x7e, 0xfa, 0x76, 0xf7, 0x24, 0xf8, 0x14, 0xb1, 0x3e, 0x9d,
    0xa5, 0xdc, 0xae, 0xef, 0xec, 0xcf, 0xfa, 0x37, 0x44, 0x1d, 0x62, 0x32, 0xb3, 0x96, 0xba, 0x9d,
    0x96, 0x75, 0x38, 0xbc, 0x97, 0x5a, 0x0c, 0xf5, 0x02, 0x7e, 0x8b, 0x11, 0xc8, 0xc7, 0xe4, 0xfd,
    0x7b, 0x08, 0xbf, 0x89, 0x06, 0xfa, 0xdd, 0xf2, 0x8a, 0x51, 0xf3, 0xed, 0xaa, 0xda, 0xee, 0x2a
};

static esp_err_t init_camera(void)
{
    camera_config_t config = {0};

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = CAM_PIN_Y2;
    config.pin_d1 = CAM_PIN_Y3;
    config.pin_d2 = CAM_PIN_Y4;
    config.pin_d3 = CAM_PIN_Y5;
    config.pin_d4 = CAM_PIN_Y6;
    config.pin_d5 = CAM_PIN_Y7;
    config.pin_d6 = CAM_PIN_Y8;
    config.pin_d7 = CAM_PIN_Y9;
    config.pin_xclk = CAM_PIN_XCLK;
    config.pin_pclk = CAM_PIN_PCLK;
    config.pin_vsync = CAM_PIN_VSYNC;
    config.pin_href = CAM_PIN_HREF;
    config.pin_sccb_sda = CAM_PIN_SIOD;
    config.pin_sccb_scl = CAM_PIN_SIOC;
    config.pin_pwdn = CAM_PIN_PWDN;
    config.pin_reset = CAM_PIN_RESET;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    esp_err_t err = esp_camera_init(&config);

    if (err == ESP_OK) {
        sensor_t *s = esp_camera_sensor_get();
        s->set_vflip(s, 1);
        s->set_hmirror(s, 1);
    }

    return err;
}


static void tcp_server_task(void *pvParameters)
{
    char rx_buffer[128];

    struct sockaddr_in server_addr = {0};
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(LISTEN_PORT);

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

    if (listen_sock < 0) {
        ESP_LOGE(TAG, "No se pudo crear el socket de escucha");
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listen_sock, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Error en el bind del socket");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    if (listen(listen_sock, 1) < 0) {
        ESP_LOGE(TAG, "Error al iniciar la escucha TCP");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Servidor TCP escuchando en el puerto %d", LISTEN_PORT);

    while (1) {
        struct sockaddr_in source_addr = {0};
        socklen_t addr_len = sizeof(source_addr);

        int sock = accept(listen_sock,
                          (struct sockaddr *)&source_addr,
                          &addr_len);

        if (sock < 0) {
            ESP_LOGE(TAG, "Error al aceptar conexión");
            continue;
        }

        char ip_cliente[INET_ADDRSTRLEN] = {0};

        inet_ntop(AF_INET, &source_addr.sin_addr,
                  ip_cliente, sizeof(ip_cliente));

        ESP_LOGI(TAG, "Conexión TCP aceptada desde %s", ip_cliente);

        int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        ESP_LOGI(TAG, "recv del comando devolvió %d bytes", len);

        if (len <= 0) {
            ESP_LOGW(TAG, "Conexión cerrada sin comando completo");
            close(sock);
            continue;
        }

        rx_buffer[len] = '\0';
        ESP_LOGI(TAG, "Comando recibido: %s", rx_buffer);

        if (strcmp(rx_buffer, "TAKE_PHOTO") == 0) {
            camera_fb_t *fb_anterior = esp_camera_fb_get();

        if (fb_anterior != NULL) {
            esp_camera_fb_return(fb_anterior);
        }

        camera_fb_t *fb = esp_camera_fb_get();

            if (fb == NULL) {
                ESP_LOGE(TAG, "Fallo al capturar la foto");
                const char *err_msg = "ERROR";
                send(sock, err_msg, strlen(err_msg), 0);
            } else {
                ESP_LOGI(TAG, "Foto tomada. Tamaño: %u bytes",
                         (unsigned int)fb->len);

                uint32_t img_size = htonl((uint32_t)fb->len);
                size_t header_sent = 0;

                while (header_sent < sizeof(img_size)) {
                    ssize_t n = send(
                        sock,
                        ((uint8_t *)&img_size) + header_sent,
                        sizeof(img_size) - header_sent,
                        0
                    );

                    if (n <= 0) {
                        ESP_LOGE(TAG, "Error enviando encabezado: %d", errno);
                        break;
                    }

                    header_sent += (size_t)n;
                }

                if (header_sent == sizeof(img_size)) {
                    ESP_LOGI(TAG, "Encabezado enviado: %u bytes",
                             (unsigned int)header_sent);

                    size_t bytes_sent = 0;

                    while (bytes_sent < fb->len) {
                        ssize_t sent = send(
                            sock,
                            fb->buf + bytes_sent,
                            fb->len - bytes_sent,
                            0
                        );

                        if (sent <= 0) {
                            ESP_LOGE(TAG, "Error al transmitir imagen: %d",
                                     errno);
                            break;
                        }

                        bytes_sent += (size_t)sent;
                        ESP_LOGI(TAG, "Enviados %u / %u bytes",
                                 (unsigned int)bytes_sent,
                                 (unsigned int)fb->len);
                    }

                    if (bytes_sent == fb->len) {
                        ESP_LOGI(TAG, "Imagen enviada con éxito");
                    }
                } else {
                    ESP_LOGE(TAG, "Error al enviar el tamaño de la imagen");
                }

                esp_camera_fb_return(fb);
            }
        } else {
            ESP_LOGW(TAG, "Comando desconocido: %s", rx_buffer);
        }

        ESP_LOGI(TAG, "Cerrando socket del cliente %d", sock);

        int err = shutdown(sock, SHUT_WR);
        ESP_LOGI(TAG, "shutdown escritura: %d, errno: %d", err, errno);

        err = close(sock);
        ESP_LOGI(TAG, "close socket: %d, errno: %d", err, errno);
    }
}


static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == NETWORK_PROV_EVENT) {
        switch (event_id) {
        case NETWORK_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            break;

        case NETWORK_PROV_WIFI_CRED_RECV: {
            wifi_sta_config_t *wifi_sta_cfg =
                (wifi_sta_config_t *)event_data;

            ESP_LOGI(TAG, "Credenciales recibidas para SSID: %.*s",
                     (int)sizeof(wifi_sta_cfg->ssid),
                     (const char *)wifi_sta_cfg->ssid);
            break;
        }

        case NETWORK_PROV_WIFI_CRED_FAIL: {
            network_prov_wifi_sta_fail_reason_t *reason =
                (network_prov_wifi_sta_fail_reason_t *)event_data;

            ESP_LOGE(TAG, "Provisioning failed: %s",
                     (*reason == NETWORK_PROV_WIFI_STA_AUTH_ERROR)
                     ? "Wi-Fi station authentication failed"
                     : "Wi-Fi access-point not found");
            break;
        }

        case NETWORK_PROV_WIFI_CRED_SUCCESS:
            ESP_LOGI(TAG, "Provisioning successful");
            break;

        case NETWORK_PROV_END: {
            esp_err_t err = network_prov_mgr_deinit();

            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to de-initialize provisioning manager: %s",
                         esp_err_to_name(err));
            }
            break;
        }

        default:
            break;
        }
    } else if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected. Connecting to the AP again...");
            esp_wifi_connect();
            break;

        default:
            break;
        }
    } else if (event_base == IP_EVENT &&
               event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG, "Connected with IP Address: " IPSTR,
                 IP2STR(&event->ip_info.ip));

        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
    } else if (event_base == PROTOCOMM_TRANSPORT_BLE_EVENT) {
        switch (event_id) {
        case PROTOCOMM_TRANSPORT_BLE_CONNECTED:
            ESP_LOGI(TAG, "BLE transport: Connected!");
            break;

        case PROTOCOMM_TRANSPORT_BLE_DISCONNECTED:
            ESP_LOGI(TAG, "BLE transport: Disconnected!");
            break;

        default:
            break;
        }
    } else if (event_base == PROTOCOMM_SECURITY_SESSION_EVENT) {
        switch (event_id) {
        case PROTOCOMM_SECURITY_SESSION_SETUP_OK:
            ESP_LOGI(TAG, "Secured session established!");
            break;

        case PROTOCOMM_SECURITY_SESSION_INVALID_SECURITY_PARAMS:
            ESP_LOGE(TAG, "Invalid security parameters");
            break;

        case PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH:
            ESP_LOGE(TAG, "Incorrect username or proof of possession");
            break;

        default:
            break;
        }
    }
}

static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = "PROV_";

    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, eth_mac));

    snprintf(service_name, max, "%s%02X%02X%02X",
             ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

esp_err_t custom_prov_data_handler(uint32_t session_id,
                                   const uint8_t *inbuf,
                                   ssize_t inlen,
                                   uint8_t **outbuf,
                                   ssize_t *outlen,
                                   void *priv_data)
{
    if (inbuf != NULL) {
        ESP_LOGI(TAG, "Received data: %.*s", (int)inlen, (char *)inbuf);
    }

    char response[] = "SUCCESS";

    *outbuf = (uint8_t *)strdup(response);

    if (*outbuf == NULL) {
        ESP_LOGE(TAG, "System out of memory");
        return ESP_ERR_NO_MEM;
    }

    *outlen = strlen(response) + 1;

    return ESP_OK;
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_event_group = xEventGroupCreate();

    if (wifi_event_group == NULL) {
        ESP_LOGE(TAG, "No se pudo crear el grupo de eventos");
        return;
    }

    ESP_ERROR_CHECK(esp_event_handler_register(
        NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(
        PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID,
        &event_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(
        PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID,
        &event_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    network_prov_mgr_config_t config = {
        .scheme = network_prov_scheme_ble,
        .scheme_event_handler =
            NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
    };

    ESP_ERROR_CHECK(network_prov_mgr_init(config));

    bool provisioned = false;

    ESP_ERROR_CHECK(
        network_prov_mgr_is_wifi_provisioned(&provisioned));

    if (!provisioned) {
        ESP_LOGI(TAG, "Starting provisioning");

        char service_name[12];
        get_device_service_name(service_name, sizeof(service_name));

        network_prov_security2_params_t sec2_params = {0};

        sec2_params.salt = sec2_salt;
        sec2_params.salt_len = sizeof(sec2_salt);
        sec2_params.verifier = sec2_verifier;
        sec2_params.verifier_len = sizeof(sec2_verifier);

        const char *service_key = NULL;

        uint8_t custom_service_uuid[] = {
            0xb4, 0xdf, 0x5a, 0x1c,
            0x3f, 0x6b, 0xf4, 0xbf,
            0xea, 0x4a, 0x82, 0x03,
            0x04, 0x90, 0x1a, 0x02
        };

        network_prov_scheme_ble_set_service_uuid(custom_service_uuid);

        ESP_ERROR_CHECK(
            network_prov_mgr_endpoint_create("custom-data"));

        ESP_LOGI(TAG, "Salt inicial: %02x %02x %02x %02x",
            (unsigned char)sec2_salt[0],
            (unsigned char)sec2_salt[1],
            (unsigned char)sec2_salt[2],
            (unsigned char)sec2_salt[3]);

        ESP_LOGI(TAG, "Salt len: %u, verifier len: %u",
            (unsigned int)sec2_params.salt_len,
            (unsigned int)sec2_params.verifier_len);

        ESP_ERROR_CHECK(network_prov_mgr_start_provisioning(
            NETWORK_PROV_SECURITY_2,
            (const void *)&sec2_params,
            service_name,
            service_key));

        ESP_ERROR_CHECK(network_prov_mgr_endpoint_register(
            "custom-data", custom_prov_data_handler, NULL));
    } else {
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");

        ESP_ERROR_CHECK(network_prov_mgr_deinit());

        ESP_ERROR_CHECK(esp_event_handler_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

        wifi_init_sta();
    }

    xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_EVENT,
        pdTRUE,
        pdTRUE,
        portMAX_DELAY
    );

    ESP_LOGI(TAG, "Wi-Fi conectado. Inicializando cámara...");

    if (init_camera() != ESP_OK) {
        ESP_LOGE(TAG, "Error al inicializar la cámara");
        return;
    }

    ESP_LOGI(TAG, "Cámara inicializada correctamente");

    BaseType_t resultado = xTaskCreate(
        tcp_server_task,
        "tcp_server",
        8192,
        NULL,
        5,
        NULL
    );

    if (resultado != pdPASS) {
        ESP_LOGE(TAG, "No se pudo crear la tarea del servidor TCP");
        return;
    }

    ESP_LOGI(TAG, "Servidor de cámara iniciado en el puerto %d",
             LISTEN_PORT);
}