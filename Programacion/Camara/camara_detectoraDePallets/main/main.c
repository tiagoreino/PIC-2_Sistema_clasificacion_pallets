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

#include "esp_eap_client.h"
#include "esp_check.h"
#include "driver/gpio.h"

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

#define DISCOVERY_PORT     5001
#define DISCOVERY_REQUEST  "DISCOVER_PALLET_CAM"

#define LISTEN_PORT 5000

#define EXAMPLE_PROV_SEC2_USERNAME "CamaraPallets"
#define EXAMPLE_PROV_SEC2_PWD      "utec2026"

#define WIFI_ENTERPRISE_CREDENTIALS_EVENT BIT1
#define UTEC_SSID "UTEC-2.4Ghz"

#define LED_R GPIO_NUM_13
#define LED_G GPIO_NUM_14
#define LED_B GPIO_NUM_15

static void led_rgb_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << LED_R) |
                        (1ULL << LED_G) |
                        (1ULL << LED_B),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&cfg));

    gpio_set_level(LED_R, 0);
    gpio_set_level(LED_G, 0);
    gpio_set_level(LED_B, 0);
}

static void led_rgb(bool r, bool g, bool b)
{
    gpio_set_level(LED_R, r);
    gpio_set_level(LED_G, g);
    gpio_set_level(LED_B, b);
}

typedef enum {
    WIFI_MODE_CASA,
    WIFI_MODE_UTEC
} wifi_profile_t;

static wifi_profile_t wifi_profile = WIFI_MODE_UTEC;

static char wifi_ssid[33];
static char wifi_password[128];
static char eap_identity[128];

static void enterprise_connect_task(void *pvParameters);

static const char *TAG = "app";

static const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;

static esp_err_t init_camera(void);
static void udp_discovery_task(void *pvParameters);
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

static void udp_discovery_task(void *pvParameters)
{
    char rx_buffer[64];
 
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(DISCOVERY_PORT);
 
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "No se pudo crear el socket UDP de descubrimiento");
        vTaskDelete(NULL);
        return;
    }
 
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Error en el bind del socket UDP");
        close(sock);
        vTaskDelete(NULL);
        return;
    }
 
    /* La respuesta es siempre la misma: se arma una sola vez.
       Se usa la misma MAC (STA) con la que se arma el nombre PROV_xxxxxx. */
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
 
    char reply[64];
    snprintf(reply, sizeof(reply), "PALLET_CAM;%02X:%02X:%02X:%02X:%02X:%02X;%d",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], LISTEN_PORT);
 


    ESP_LOGI(TAG, "Descubrimiento UDP escuchando en el puerto %d", DISCOVERY_PORT);
 
    while (1) {
        struct sockaddr_in source_addr = {0};
        socklen_t source_len = sizeof(source_addr);
 
        /* Bloquea hasta que llega un datagrama; guarda en source_addr quien lo mando */
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                           (struct sockaddr *)&source_addr, &source_len);
 
        if (len < 0) {
            ESP_LOGE(TAG, "Error en recvfrom: %d", errno);
            vTaskDelay(pdMS_TO_TICKS(100));   /* evita un bucle de errores a toda velocidad */
            continue;
        }
 
        rx_buffer[len] = '\0';

        char ip_origen[INET_ADDRSTRLEN];
        inet_ntoa_r(source_addr.sin_addr, ip_origen, sizeof(ip_origen));

        ESP_LOGI(TAG, "UDP recibido desde %s:%d, bytes=%d, mensaje='%s'",
                ip_origen, ntohs(source_addr.sin_port), len, rx_buffer);

        if (strcmp(rx_buffer, DISCOVERY_REQUEST) == 0) {
            int enviados = sendto(sock, reply, strlen(reply), 0,
                                (struct sockaddr *)&source_addr,
                                sizeof(source_addr));

            if (enviados < 0) {
                ESP_LOGE(TAG, "Error enviando respuesta UDP: errno=%d", errno);
            } else {
                ESP_LOGI(TAG, "Respuesta UDP enviada a %s:%d: %s",
                        ip_origen, ntohs(source_addr.sin_port), reply);
            }
        } else {
            ESP_LOGW(TAG, "Mensaje de descubrimiento no reconocido");
        }
    }
}

static void tcp_server_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Entré en tcp_server_task()");

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
        case WIFI_EVENT_STA_START: {
            ESP_LOGI(TAG, "Evento STA_START recibido; intentando conectar");

            esp_err_t err = esp_wifi_connect();

            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Error en esp_wifi_connect(): %s",
                        esp_err_to_name(err));
            }
            break;
        }

        case WIFI_EVENT_STA_DISCONNECTED: {
            led_rgb(true, false, false);  // Rojo: desconectado

            wifi_event_sta_disconnected_t *event =
                (wifi_event_sta_disconnected_t *)event_data;

            ESP_LOGW(TAG, "Wi-Fi desconectado. Motivo: %d",
                    event->reason);

            esp_err_t err = esp_wifi_connect();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Error al reintentar: %s",
                        esp_err_to_name(err));
            }
            break;
        }
        default:
            break;
        }
    } else if (event_base == IP_EVENT &&
               event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG, "Connected with IP Address: " IPSTR,
                 IP2STR(&event->ip_info.ip));

        led_rgb(false, true, false);  // Verde: Wi-Fi conectado

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

esp_err_t custom_prov_data_handler(
    uint32_t session_id,
    const uint8_t *inbuf,
    ssize_t inlen,
    uint8_t **outbuf,
    ssize_t *outlen,
    void *priv_data)
{
    char datos[320];

    if (inbuf == NULL || inlen <= 0 ||
        inlen >= (ssize_t)sizeof(datos)) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(datos, inbuf, inlen);
    datos[inlen] = '\0';

    // Quitar terminadores finales, si el cliente los incluyó.
    size_t longitud = strlen(datos);
    while (longitud > 0 &&
           (datos[longitud - 1] == '\r' ||
            datos[longitud - 1] == '\0')) {
        datos[--longitud] = '\0';
    }

    char *modo = datos;
    char *sep1 = strchr(modo, '\n');

    if (sep1 == NULL) {
        ESP_LOGE(TAG, "Falta el primer separador BLE");
        return ESP_ERR_INVALID_ARG;
    }

    *sep1 = '\0';
    char *valor_wifi = sep1 + 1;

    char *sep2 = strchr(valor_wifi, '\n');

    if (sep2 == NULL) {
        ESP_LOGE(TAG, "Falta el segundo separador BLE");
        return ESP_ERR_INVALID_ARG;
    }

    *sep2 = '\0';
    char *password = sep2 + 1;

    // Quitar CR de cada campo si se recibió CRLF.
    char *campos[] = { modo, valor_wifi, password };

    for (size_t i = 0; i < 3; i++) {
        size_t n = strlen(campos[i]);
        if (n > 0 && campos[i][n - 1] == '\r') {
            campos[i][n - 1] = '\0';
        }
    }

    if (valor_wifi[0] == '\0' || password[0] == '\0') {
        ESP_LOGE(TAG, "SSID/usuario o contraseña vacíos");
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(modo, "UTEC") == 0) {
        if (strlen(valor_wifi) >= sizeof(eap_identity) ||
            strlen(password) >= sizeof(wifi_password)) {
            return ESP_ERR_INVALID_ARG;
        }

        wifi_profile = WIFI_MODE_UTEC;

        strncpy(wifi_ssid, UTEC_SSID, sizeof(wifi_ssid) - 1);
        wifi_ssid[sizeof(wifi_ssid) - 1] = '\0';

        strcpy(eap_identity, valor_wifi);
        strcpy(wifi_password, password);

        ESP_LOGI(TAG, "Configuración recibida: UTEC Enterprise PEAP");

        led_rgb(true, true, false);  // Amarillo: credenciales recibidas

    } else if (strcmp(modo, "CASA") == 0) {
        if (strlen(valor_wifi) > 32 ||
            strlen(password) >= 63) {
            ESP_LOGE(TAG, "SSID o contraseña demasiado largos");
            return ESP_ERR_INVALID_ARG;
        }

        wifi_profile = WIFI_MODE_CASA;

        strcpy(wifi_ssid, valor_wifi);
        strcpy(wifi_password, password);
        eap_identity[0] = '\0';

        ESP_LOGI(TAG, "Configuración recibida: Casa WPA2-Personal");
    } else {
        ESP_LOGE(TAG, "Modo Wi-Fi desconocido");
        return ESP_ERR_INVALID_ARG;
    }

    char *response = strdup("SUCCESS");

    if (response == NULL) {
        return ESP_ERR_NO_MEM;
    }

    *outbuf = (uint8_t *)response;
    *outlen = strlen(response) + 1;

    xEventGroupSetBits(
        wifi_event_group,
        WIFI_ENTERPRISE_CREDENTIALS_EVENT
    );

    return ESP_OK;
}

static void enterprise_connect_task(void *pvParameters)
{
    xEventGroupWaitBits(
        wifi_event_group,
        WIFI_ENTERPRISE_CREDENTIALS_EVENT,
        pdTRUE,
        pdFALSE,
        portMAX_DELAY
    );

    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "Preparando conexión Wi-Fi");

    ESP_ERROR_CHECK(network_prov_mgr_deinit());

    wifi_config_t wifi_config = {0};

    memcpy(
        wifi_config.sta.ssid,
        wifi_ssid,
        strlen(wifi_ssid)
    );

    if (wifi_profile == WIFI_MODE_CASA) {
        // Wi-Fi doméstico: WPA2-Personal.
        strncpy(
            (char *)wifi_config.sta.password,
            wifi_password,
            sizeof(wifi_config.sta.password) - 1
        );

        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        ESP_LOGI(TAG, "Autenticación WPA2-Personal");
    } else {
        // Wi-Fi institucional: WPA2-Enterprise PEAP.
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_ENTERPRISE;

        ESP_ERROR_CHECK(
            esp_eap_client_set_eap_methods(ESP_EAP_TYPE_PEAP)
        );

        ESP_ERROR_CHECK(
            esp_eap_client_set_identity(
                (const unsigned char *)eap_identity,
                strlen(eap_identity)
            )
        );

        ESP_ERROR_CHECK(
            esp_eap_client_set_username(
                (const unsigned char *)eap_identity,
                strlen(eap_identity)
            )
        );

        ESP_ERROR_CHECK(
            esp_eap_client_set_password(
                (const unsigned char *)wifi_password,
                strlen(wifi_password)
            )
        );

        ESP_ERROR_CHECK(esp_wifi_sta_enterprise_enable());

        ESP_LOGI(TAG, "Autenticación WPA2-Enterprise PEAP");
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_ERROR_CHECK(
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config)
    );

    ESP_LOGI(TAG, "SSID configurado: %s", wifi_ssid);
    ESP_LOGI(TAG, "Iniciando Wi-Fi");

    esp_err_t err = esp_wifi_start();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al iniciar Wi-Fi: %s",
                 esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };

    uint16_t ap_count = 0;

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

    wifi_ap_record_t *aps = calloc(ap_count, sizeof(wifi_ap_record_t));

    if (aps != NULL && ap_count > 0) {
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, aps));

        for (int i = 0; i < ap_count; i++) {
            ESP_LOGI(TAG, "Red encontrada: %s | RSSI: %d dBm",
                    (char *)aps[i].ssid, aps[i].rssi);
        }

        free(aps);
    }

    // Se mantiene la llamada explícita que ya utilizabas.
    err = esp_wifi_connect();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al solicitar conexión: %s",
                 esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Solicitud de conexión enviada");
    }

    vTaskDelete(NULL);
}

void app_main(void)
{

    led_rgb_init();
    led_rgb(true, true, true);  // Blanco: arrancando

    ESP_LOGI(TAG, "=== Firmware con descubrimiento UDP ===");

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

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Registrar los eventos ANTES de iniciar Wi-Fi.
    ESP_ERROR_CHECK(
        esp_event_handler_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &event_handler,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &event_handler,
            NULL
        )
    );

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    // El Wi-Fi queda inicializado, pero todavía no se inicia.
    // Primero recibimos las credenciales por BLE.
    network_prov_mgr_config_t prov_config = {
        .scheme = network_prov_scheme_ble,
        .scheme_event_handler =
            NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
    };

    ESP_ERROR_CHECK(network_prov_mgr_init(prov_config));

    ESP_ERROR_CHECK(
        network_prov_mgr_endpoint_create("custom-data")
    );

    // Crear el servicio BLE y la sesión de seguridad.
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

    led_rgb(true, false, true);  // Violeta: esperando credenciales BLE

    ESP_ERROR_CHECK(
        network_prov_mgr_start_provisioning(
            NETWORK_PROV_SECURITY_2,
            (const void *)&sec2_params,
            service_name,
            service_key
        )
    );

    // Registrar el handler después de iniciar el servicio.
    ESP_ERROR_CHECK(
        network_prov_mgr_endpoint_register(
            "custom-data",
            custom_prov_data_handler,
            NULL
        )
    );

    // La tarea espera hasta recibir las credenciales.
    BaseType_t resultado = xTaskCreate(
        enterprise_connect_task,
        "enterprise_connect",
        4096,
        NULL,
        5,
        NULL
    );

    if (resultado != pdPASS) {
        ESP_LOGE(TAG, "No se pudo crear la tarea Enterprise");
        return;
    }

    // Esperar hasta obtener una IP, aunque el aprovisionamiento BLE demore.
    EventBits_t bits = xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_EVENT,
        pdTRUE,
        pdTRUE,
        portMAX_DELAY
    );

    if (!(bits & WIFI_CONNECTED_EVENT)) {
        ESP_LOGE(TAG, "No se obtuvo el evento de conexión Wi-Fi");
        return;
    }

    ESP_LOGI(TAG, "Wi-Fi conectado. Iniciando servicios de cámara.");

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    xTaskCreate(
        udp_discovery_task,
        "udp_discovery",
        4096,
        NULL,
        5,
        NULL
    );

    if (init_camera() != ESP_OK) {
        ESP_LOGE(TAG, "Error al inicializar la cámara");

        while (1) {
            led_rgb(true, false, false);
            vTaskDelay(pdMS_TO_TICKS(300));

            led_rgb(false, false, false);
            vTaskDelay(pdMS_TO_TICKS(300));
        }
    }

    ESP_LOGI(TAG, "Cámara inicializada correctamente");
    led_rgb(false, false, true);  // Azul: cámara inicializada

    resultado = xTaskCreate(
        tcp_server_task,
        "tcp_server",
        8192,
        NULL,
        5,
        NULL
    );

    if (resultado != pdPASS) {
        ESP_LOGE(TAG, "No se pudo crear el servidor TCP");
        led_rgb(true, false, false);
        return;
    }

    led_rgb(false, true, false);  // Verde: sistema listo

    ESP_LOGI(TAG, "Servidor de cámara iniciado en el puerto %d",
            LISTEN_PORT);
} 