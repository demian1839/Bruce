/**
 * @file main.c
 * @brief Onboard ESP32-C6 ESP-NOW Wireless Co-Processor Firmware
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "protocol.h"

static const char *TAG = "ESP32_C6_COPROC";

#define INTERCHIP_UART_NUM      UART_NUM_1
#define INTERCHIP_TX_PIN        GPIO_NUM_16
#define INTERCHIP_RX_PIN        GPIO_NUM_17
#define INTERCHIP_BAUD_RATE     1152000
#define UART_BUF_SIZE           1024

#define UART_FRAME_START        0xAA
#define UART_FRAME_STOP         0x55

static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static QueueHandle_t s_rx_queue = NULL;

typedef struct {
    uint8_t src_mac[ESP_NOW_ETH_ALEN];
    espnow_msg_packet_t packet;
} rx_item_t;

typedef enum {
    PARSER_STATE_START,
    PARSER_STATE_MAC,
    PARSER_STATE_HEADER,
    PARSER_STATE_PAYLOAD,
    PARSER_STATE_STOP
} uart_parser_state_t;

/**
 * @brief Initialize Inter-Chip High-Speed UART Link with ESP32-P4 Host MCU
 */
static void init_interchip_uart(void) {
    uart_config_t uart_config = {
        .baud_rate = INTERCHIP_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(INTERCHIP_UART_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(INTERCHIP_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(INTERCHIP_UART_NUM, INTERCHIP_TX_PIN, INTERCHIP_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "Inter-chip UART link initialized at %d baud", INTERCHIP_BAUD_RATE);
}

/**
 * @brief Send Framed UART Packet to ESP32-P4 Host
 */
static void send_framed_uart_packet(const uint8_t *target_mac, const espnow_msg_packet_t *pkt) {
    size_t pkt_size = get_packet_total_size(pkt);
    uint8_t tx_buf[1 + 6 + sizeof(espnow_msg_packet_t) + 1];

    tx_buf[0] = UART_FRAME_START;
    memcpy(&tx_buf[1], target_mac, 6);
    memcpy(&tx_buf[7], pkt, pkt_size);
    tx_buf[7 + pkt_size] = UART_FRAME_STOP;

    size_t total_frame_len = 1 + 6 + pkt_size + 1;
    uart_write_bytes(INTERCHIP_UART_NUM, (const char *)tx_buf, total_frame_len);
}

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
 * @brief Initialize Wi-Fi and ESP-NOW on ESP32-C6
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

    ESP_ERROR_CHECK(add_peer(s_broadcast_mac));

    uint8_t mac[ESP_NOW_ETH_ALEN];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "ESP32-C6 Co-Processor ESP-NOW Ready. Station MAC: " MACSTR, MAC2STR(mac));
}

/**
 * @brief Task forwarding received ESP-NOW touch packets to ESP32-P4 Host over Framed UART Link
 */
static void c6_rx_bridge_task(void *pvParameters) {
    rx_item_t item;

    while (1) {
        if (xQueueReceive(s_rx_queue, &item, portMAX_DELAY) == pdTRUE) {
            if (!validate_packet(&item.packet)) {
                ESP_LOGW(TAG, "Packet validation failed! Dropping packet.");
                continue;
            }

            // Register peer
            add_peer(item.src_mac);

            if (item.packet.opcode == OP_TOUCH_EVENT) {
                ESP_LOGI(TAG, "Touch Event from Display " MACSTR " -> Forwarding to ESP32-P4 Host over Inter-Chip Link", MAC2STR(item.src_mac));

                // Send framed packet over UART
                send_framed_uart_packet(item.src_mac, &item.packet);
            }
        }
    }
}

/**
 * @brief Task reading UI Update packets sent from ESP32-P4 over Framed UART Link and dispatching via ESP-NOW
 */
static void c6_p4_response_task(void *pvParameters) {
    uint8_t byte;
    uart_parser_state_t state = PARSER_STATE_START;

    uint8_t target_mac[6];
    size_t mac_idx = 0;

    espnow_msg_packet_t rx_pkt;
    uint8_t *pkt_ptr = (uint8_t *)&rx_pkt;
    size_t header_idx = 0;
    size_t payload_idx = 0;

    while (1) {
        int len = uart_read_bytes(INTERCHIP_UART_NUM, &byte, 1, pdMS_TO_TICKS(10));
        if (len <= 0) {
            continue;
        }

        switch (state) {
            case PARSER_STATE_START:
                if (byte == UART_FRAME_START) {
                    mac_idx = 0;
                    state = PARSER_STATE_MAC;
                }
                break;

            case PARSER_STATE_MAC:
                target_mac[mac_idx++] = byte;
                if (mac_idx >= 6) {
                    header_idx = 0;
                    state = PARSER_STATE_HEADER;
                }
                break;

            case PARSER_STATE_HEADER:
                pkt_ptr[header_idx++] = byte;
                if (header_idx >= 5) {
                    if (rx_pkt.magic != PROTOCOL_MAGIC || rx_pkt.payload_len > sizeof(rx_pkt.payload)) {
                        state = PARSER_STATE_START;
                    } else {
                        payload_idx = 0;
                        state = (rx_pkt.payload_len > 0) ? PARSER_STATE_PAYLOAD : PARSER_STATE_STOP;
                    }
                }
                break;

            case PARSER_STATE_PAYLOAD:
                ((uint8_t *)&rx_pkt.payload)[payload_idx++] = byte;
                if (payload_idx >= rx_pkt.payload_len) {
                    state = PARSER_STATE_STOP;
                }
                break;

            case PARSER_STATE_STOP:
                if (byte == UART_FRAME_STOP) {
                    if (validate_packet(&rx_pkt)) {
                        add_peer(target_mac);
                        size_t send_len = get_packet_total_size(&rx_pkt);
                        esp_now_send(target_mac, (uint8_t *)&rx_pkt, send_len);
                        ESP_LOGI(TAG, "Forwarded UI Update from P4 over ESP-NOW to " MACSTR " (Len: %zu bytes)",
                                 MAC2STR(target_mac), send_len);
                    }
                }
                state = PARSER_STATE_START;
                break;
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting ESP32-C6 Wireless Co-Processor Firmware...");

    s_rx_queue = xQueueCreate(10, sizeof(rx_item_t));
    init_espnow();
    init_interchip_uart();

    xTaskCreate(c6_rx_bridge_task, "c6_bridge", 4096, NULL, 5, NULL);
    xTaskCreate(c6_p4_response_task, "c6_p4_resp", 4096, NULL, 5, NULL);
}
