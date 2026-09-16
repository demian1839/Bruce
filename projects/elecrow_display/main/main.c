/**
 * @file main.c
 * @brief Elecrow Display Terminal Unit (ESP-NOW UI & Touch Input Client)
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "protocol.h"

static const char *TAG = "ELECROW_DISPLAY";

// Broadcast address or target ESP32-C6 Co-Processor / ESP32-P4 MAC address
static uint8_t s_p4_host_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static QueueHandle_t s_rx_queue = NULL;

typedef struct {
    uint8_t src_mac[ESP_NOW_ETH_ALEN];
    espnow_msg_packet_t packet;
} rx_item_t;

/**
 * @brief ESP-NOW send callback
 */
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    ESP_LOGD(TAG, "ESP-NOW Tx to " MACSTR " - Status: %s",
             MAC2STR(mac_addr), (status == ESP_NOW_SEND_SUCCESS) ? "SUCCESS" : "FAIL");
}

/**
 * @brief ESP-NOW receive callback
 */
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len) {
    if (!recv_info || !data || data_len < sizeof(uint8_t) * 4) {
        ESP_LOGW(TAG, "Invalid packet size received: %d", data_len);
        return;
    }

    rx_item_t item;
    memcpy(item.src_mac, recv_info->src_addr, ESP_NOW_ETH_ALEN);
    memcpy(&item.packet, data, (data_len < sizeof(espnow_msg_packet_t)) ? data_len : sizeof(espnow_msg_packet_t));

    if (s_rx_queue) {
        xQueueSend(s_rx_queue, &item, portMAX_DELAY);
    }
}

/**
 * @brief Add ESP-NOW Peer
 */
static esp_err_t add_peer(const uint8_t *mac) {
    esp_now_peer_info_t peer_info = {0};
    memcpy(peer_info.peer_addr, mac, ESP_NOW_ETH_ALEN);
    peer_info.channel = 1;
    peer_info.encrypt = false;

    if (esp_now_is_peer_exist(mac)) {
        return ESP_OK;
    }
    return esp_now_add_peer(&peer_info);
}

/**
 * @brief Initialize Wi-Fi and ESP-NOW
 */
static void init_espnow(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    ESP_ERROR_CHECK(add_peer(s_p4_host_mac));

    uint8_t mac[ESP_NOW_ETH_ALEN];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "Elecrow Display ESP-NOW Initialized. Station MAC: " MACSTR, MAC2STR(mac));
}

/**
 * @brief Send Touch Event packet to ESP32-P4 / C6
 */
static void send_touch_event(uint16_t x, uint16_t y, touch_state_t state) {
    espnow_msg_packet_t packet = {0};
    packet.magic = PROTOCOL_MAGIC;
    packet.opcode = OP_TOUCH_EVENT;
    packet.payload_len = sizeof(touch_packet_payload_t);

    packet.payload.touch.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
    packet.payload.touch.touch_count = 1;
    packet.payload.touch.points[0].x = x;
    packet.payload.touch.points[0].y = y;
    packet.payload.touch.points[0].state = (uint8_t)state;
    packet.payload.touch.points[0].point_id = 0;

    packet.checksum = calculate_packet_checksum(&packet);

    size_t send_len = get_packet_total_size(&packet);

    int64_t t_start = esp_timer_get_time();
    esp_err_t ret = esp_now_send(s_p4_host_mac, (uint8_t *)&packet, send_len);
    int64_t t_end = esp_timer_get_time();

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Touch Sent to P4/C6: (%u, %u), state: %d [Len: %zu bytes, Tx time: %lld us]",
                 x, y, state, send_len, (t_end - t_start));
    } else {
        ESP_LOGE(TAG, "Failed to send Touch Event: %s", esp_err_to_name(ret));
    }
}

/**
 * @brief Simulated / Hardware Touch Sampling Task
 */
static void touch_sensor_task(void *pvParameters) {
    uint16_t sim_x = 100;
    uint16_t sim_y = 150;

    while (1) {
        // Read touch hardware interface or simulate touch input
        vTaskDelay(pdMS_TO_TICKS(500));

        sim_x = (sim_x + 15) % 800;
        sim_y = (sim_y + 25) % 480;

        // Dispatch lightweight byte packet to P4/C6
        send_touch_event(sim_x, sim_y, TOUCH_EVENT_PRESSED);
    }
}

/**
 * @brief Receive Task for UI updates sent from ESP32-P4 Host
 */
static void ui_receiver_task(void *pvParameters) {
    rx_item_t item;

    while (1) {
        if (xQueueReceive(s_rx_queue, &item, portMAX_DELAY) == pdTRUE) {
            if (!validate_packet(&item.packet)) {
                ESP_LOGW(TAG, "Packet validation failed! Dropping packet.");
                continue;
            }

            switch (item.packet.opcode) {
                case OP_UI_UPDATE: {
                    ui_update_payload_t *ui = &item.packet.payload.ui;
                    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
                    uint32_t latency = (now_ms >= ui->uptime_ms) ? (now_ms - ui->uptime_ms) : 0;

                    ESP_LOGI(TAG, "Rx UI Update from P4/C6! Seq: %" PRIu32 ", Uptime: %" PRIu32 " ms, "
                             "Calc Counter: %" PRIu32 ", Text: '%s', RGB: (%u,%u,%u) [Estimate Latency: %" PRIu32 " ms]",
                             ui->sequence,
                             ui->uptime_ms,
                             ui->calc_counter,
                             ui->status_text,
                             ui->bg_red, ui->bg_green, ui->bg_blue,
                             latency);

                    // Update display panel frame/widgets here
                    break;
                }
                default:
                    ESP_LOGW(TAG, "Unhandled Opcode: 0x%02X", item.packet.opcode);
                    break;
            }
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting Elecrow Display Terminal (ESP-NOW)...");

    s_rx_queue = xQueueCreate(10, sizeof(rx_item_t));
    init_espnow();

    xTaskCreatePinnedToCore(touch_sensor_task, "touch_task", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(ui_receiver_task, "ui_recv_task", 4096, NULL, 5, NULL, 1);
}
