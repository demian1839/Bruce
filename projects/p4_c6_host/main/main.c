/**
 * @file main.c
 * @brief ESP32-P4 Main System Unit Firmware (Host Processing Logic)
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "nvs_flash.h"
#include "protocol.h"

static const char *TAG = "ESP32_P4_HOST";

#define INTERCHIP_UART_NUM      UART_NUM_1
#define INTERCHIP_TX_PIN        GPIO_NUM_17
#define INTERCHIP_RX_PIN        GPIO_NUM_18
#define INTERCHIP_BAUD_RATE     1152000
#define UART_BUF_SIZE           1024

#define UART_FRAME_START        0xAA
#define UART_FRAME_STOP         0x55

static QueueHandle_t s_p4_event_queue = NULL;
static uint32_t s_calc_counter = 0;

typedef struct {
    uint8_t  src_mac[6];
    uint16_t touch_x;
    uint16_t touch_y;
    uint8_t  touch_state;
} p4_host_event_t;

typedef enum {
    PARSER_STATE_START,
    PARSER_STATE_MAC,
    PARSER_STATE_HEADER,
    PARSER_STATE_PAYLOAD,
    PARSER_STATE_STOP
} uart_parser_state_t;

/**
 * @brief Initialize Inter-Chip High-Speed UART Link with ESP32-C6 Co-Processor
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
 * @brief Send Framed UART Packet to ESP32-C6 Co-Processor
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
 * @brief Robust State-Machine UART Stream Parser Task
 */
static void p4_interchip_rx_task(void *pvParameters) {
    uint8_t byte;
    uart_parser_state_t state = PARSER_STATE_START;

    uint8_t mac_buf[6];
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
                mac_buf[mac_idx++] = byte;
                if (mac_idx >= 6) {
                    header_idx = 0;
                    state = PARSER_STATE_HEADER;
                }
                break;

            case PARSER_STATE_HEADER:
                pkt_ptr[header_idx++] = byte;
                // Header consists of: magic (1), opcode (1), payload_len (2), checksum (1) = 5 bytes
                if (header_idx >= 5) {
                    if (rx_pkt.magic != PROTOCOL_MAGIC || rx_pkt.payload_len > sizeof(rx_pkt.payload)) {
                        // Invalid header, reset state machine
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
                    if (validate_packet(&rx_pkt) && rx_pkt.opcode == OP_TOUCH_EVENT) {
                        p4_host_event_t evt;
                        memcpy(evt.src_mac, mac_buf, 6);
                        evt.touch_x = rx_pkt.payload.touch.points[0].x;
                        evt.touch_y = rx_pkt.payload.touch.points[0].y;
                        evt.touch_state = rx_pkt.payload.touch.points[0].state;

                        xQueueSend(s_p4_event_queue, &evt, portMAX_DELAY);
                    }
                }
                state = PARSER_STATE_START;
                break;
        }
    }
}

/**
 * @brief Host Compute Task: Processes touch events, updates system logic, sends UI updates to C6
 */
static void p4_compute_task(void *pvParameters) {
    p4_host_event_t event;

    while (1) {
        if (xQueueReceive(s_p4_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            s_calc_counter += 10;
            ESP_LOGI(TAG, "ESP32-P4 Processing Touch Event at (%u, %u)! New Calc Counter: %" PRIu32,
                     event.touch_x, event.touch_y, s_calc_counter);

            // Construct OP_UI_UPDATE packet
            espnow_msg_packet_t ui_pkt = {0};
            ui_pkt.magic = PROTOCOL_MAGIC;
            ui_pkt.opcode = OP_UI_UPDATE;
            ui_pkt.payload_len = sizeof(ui_update_payload_t);

            ui_pkt.payload.ui.sequence = 1;
            ui_pkt.payload.ui.uptime_ms = (uint32_t)(esp_timer_get_time() / 1000);
            ui_pkt.payload.ui.calc_counter = s_calc_counter;
            ui_pkt.payload.ui.active_widget_id = 0x01;
            ui_pkt.payload.ui.bg_red = (event.touch_x % 255);
            ui_pkt.payload.ui.bg_green = (event.touch_y % 255);
            ui_pkt.payload.ui.bg_blue = (s_calc_counter % 255);
            snprintf(ui_pkt.payload.ui.status_text, MAX_MSG_LEN, "P4 Processed (%u,%u)", event.touch_x, event.touch_y);

            ui_pkt.checksum = calculate_packet_checksum(&ui_pkt);

            send_framed_uart_packet(event.src_mac, &ui_pkt);
            ESP_LOGI(TAG, "P4 Sent UI Update Frame over Inter-Chip Link to " MACSTR, MAC2STR(event.src_mac));
        }
    }
}

/**
 * @brief Background High-Performance Logic Loop on ESP32-P4
 */
static void p4_background_logic_task(void *pvParameters) {
    while (1) {
        s_calc_counter++;
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "ESP32-P4 High-Performance Compute Task Running... Calc Counter: %" PRIu32, s_calc_counter);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting ESP32-P4 System Host Processing Unit...");

    s_p4_event_queue = xQueueCreate(10, sizeof(p4_host_event_t));
    init_interchip_uart();

    xTaskCreatePinnedToCore(p4_interchip_rx_task, "p4_ic_rx", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(p4_compute_task, "p4_compute", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(p4_background_logic_task, "p4_bg_logic", 4096, NULL, 1, NULL, 1);
}
