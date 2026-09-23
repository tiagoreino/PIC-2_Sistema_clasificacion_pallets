#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#define WIFI_SSID      "Residencia Fraycentro"
#define WIFI_PASS      "ignial1122"
#define PC_IP          "192.168.1.9"   // <-- cambiá esto por la IP real de tu PC
#define PC_PORT        5000
#define LED_GPIO       GPIO_NUM_4        // el flash led de la esp32-cam

static const char *TAG = "wifi_led_test";
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
    ESP_LOGI(TAG, "WiFi conectado");
}

static void tcp_task(void *pvParameters) {
    while (1) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(PC_IP);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PC_PORT);

        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "No se pudo crear el socket");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TAG, "No se pudo conectar a la PC");
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        ESP_LOGI(TAG, "Conectado a la PC");

        const char *msg = "PHOTO_PLACEHOLDER";
        send(sock, msg, strlen(msg), 0);

        uint8_t rx_buffer[1];
        int len = recv(sock, rx_buffer, sizeof(rx_buffer), 0);
        if (len > 0) {
            if (rx_buffer[0] == '1') {
                gpio_set_level(LED_GPIO, 1);
                ESP_LOGI(TAG, "Señal recibida: LED ON");
            } else {
                gpio_set_level(LED_GPIO, 0);
                ESP_LOGI(TAG, "Señal recibida: LED OFF");
            }
        }

        close(sock);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    wifi_init();

    xTaskCreate(tcp_task, "tcp_task", 4096, NULL, 5, NULL);
}