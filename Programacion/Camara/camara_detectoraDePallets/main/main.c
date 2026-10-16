#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "esp_camera.h"

#define WIFI_SSID      "Residencia Fraycentro"
#define WIFI_PASS      "ignial1122"
#define LISTEN_PORT    5000

// Pines estándar de la ESP32-CAM (AI-Thinker)
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

static const char *TAG = "esp32_cam_tcp";
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static void event_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t init_camera(void) {
    camera_config_t config;
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

    // Configuración de la resolución (SVGA suele ser estable)
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12; // 0-63 (menor número significa mayor calidad)
    config.fb_count = 1;

    // Inicializa primero el sensor para poder aplicarle los cambios
    esp_err_t err = esp_camera_init(&config);
    if (err == ESP_OK) {
        sensor_t *s = esp_camera_sensor_get();
        s->set_vflip(s, 1);   // Invierte la foto verticalmente
        s->set_hmirror(s, 1); // Invierte la foto horizontalmente (Efecto espejo)
    }
    return err;
}

static void wifi_init(void) {
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "WiFi Conectado");
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    
    // Agregamos esto para ver la IP real asignada por el router:
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_get_ip_info(netif, &ip_info);
        ESP_LOGI(TAG, "WiFi Conectado! IP Asignada: " IPSTR, IP2STR(&ip_info.ip));
    }
}


static void tcp_server_task(void *pvParameters) {
    char rx_buffer[128];
    struct sockaddr_in server_addr;
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

    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Error en el bind del socket");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    listen(listen_sock, 1);
    ESP_LOGI(TAG, "Servidor TCP escuchando en el puerto %d...", LISTEN_PORT);

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Error al aceptar conexión");
            continue;
        }

        int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len > 0) {
            rx_buffer[len] = 0; // Terminar la cadena
            ESP_LOGI(TAG, "Comando recibido: %s", rx_buffer);

            if (strcmp(rx_buffer, "TAKE_PHOTO") == 0) {
                // Capturar foto
                camera_fb_t *fb = esp_camera_fb_get();
                if (!fb) {
                    ESP_LOGE(TAG, "Fallo al capturar la foto");
                    const char *err_msg = "ERROR";
                    send(sock, err_msg, strlen(err_msg), 0);
                } else {
                    ESP_LOGI(TAG, "Foto tomada. Tamaño: %d bytes", fb->len);
                    
                    // 1. Enviar el tamaño del archivo primero (entero de 32 bits)
                    uint32_t img_size = htonl(fb->len); // Big-endian para la red
                    send(sock, &img_size, sizeof(img_size), 0);

                    // 2. Enviar los datos binarios de la imagen
                    int bytes_sent = 0;
                    while (bytes_sent < fb->len) {
                        int sent = send(sock, fb->buf + bytes_sent, fb->len - bytes_sent, 0);
                        if (sent < 0) {
                            ESP_LOGE(TAG, "Error al transmitir imagen");
                            break;
                        }
                        bytes_sent += sent;
                    }
                    
                    esp_camera_fb_return(fb);
                    ESP_LOGI(TAG, "Imagen enviada con éxito");
                }
            }
        }
        close(sock);
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    
    if (init_camera() != ESP_OK) {
        ESP_LOGE(TAG, "Error crítico: Inicialización de cámara fallida");
        return;
    }
    
    wifi_init();
    
    // Aumentamos el stack de la tarea TCP ya que el manejo de red y cámara consume más recursos
    xTaskCreate(tcp_server_task, "tcp_server", 8192, NULL, 5, NULL);
}