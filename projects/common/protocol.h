/**
 * @file protocol.h
 * @brief ESP-NOW Shared Data Protocol Definition for ESP32-P4/C6 Host and Elecrow Display
 */

#ifndef SHARED_PROTOCOL_H
#define SHARED_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTOCOL_MAGIC 0xA5
#define MAX_TOUCH_POINTS 1
#define MAX_MSG_LEN 32

typedef enum {
    OP_TOUCH_EVENT = 0x01,
    OP_UI_UPDATE   = 0x02,
    OP_HEARTBEAT   = 0x03,
    OP_PAIR_REQ    = 0x04,
    OP_PAIR_ACK    = 0x05,
} packet_opcode_t;

typedef enum {
    TOUCH_EVENT_RELEASED = 0,
    TOUCH_EVENT_PRESSED  = 1,
    TOUCH_EVENT_MOVED    = 2,
} touch_state_t;

typedef struct __attribute__((packed)) {
    uint16_t x;
    uint16_t y;
    uint8_t  state; // touch_state_t
    uint8_t  point_id;
} touch_point_t;

typedef struct __attribute__((packed)) {
    uint32_t timestamp_ms;
    uint8_t  touch_count;
    touch_point_t points[MAX_TOUCH_POINTS];
} touch_packet_payload_t;

typedef struct __attribute__((packed)) {
    uint32_t sequence;
    uint32_t uptime_ms;
    uint32_t calc_counter;
    uint16_t active_widget_id;
    uint8_t  status_flags;
    uint8_t  bg_red;
    uint8_t  bg_green;
    uint8_t  bg_blue;
    char     status_text[MAX_MSG_LEN];
} ui_update_payload_t;

typedef struct __attribute__((packed)) {
    uint32_t uptime_ms;
    uint8_t  rssi_level;
} heartbeat_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t magic;         // PROTOCOL_MAGIC (0xA5)
    uint8_t opcode;        // packet_opcode_t
    uint16_t payload_len;  // Length of payload union data
    uint8_t checksum;      // XOR checksum over header (magic, opcode, payload_len) + payload bytes
    union {
        touch_packet_payload_t touch;
        ui_update_payload_t    ui;
        heartbeat_payload_t    heartbeat;
        uint8_t raw[128];
    } payload;
} espnow_msg_packet_t;

/**
 * @brief Helper to get exact packed packet size for minimal over-the-air transmission overhead
 */
static inline size_t get_packet_total_size(const espnow_msg_packet_t *pkt) {
    return sizeof(pkt->magic) + sizeof(pkt->opcode) + sizeof(pkt->payload_len) + sizeof(pkt->checksum) + pkt->payload_len;
}

/**
 * @brief Calculate XOR checksum over header and payload
 */
static inline uint8_t calculate_packet_checksum(const espnow_msg_packet_t *pkt) {
    const uint8_t *data = (const uint8_t *)pkt;
    uint8_t checksum = 0;
    // XOR magic, opcode, payload_len
    checksum ^= pkt->magic;
    checksum ^= pkt->opcode;
    checksum ^= (uint8_t)(pkt->payload_len & 0xFF);
    checksum ^= (uint8_t)((pkt->payload_len >> 8) & 0xFF);

    // XOR payload bytes
    const uint8_t *payload_bytes = (const uint8_t *)&pkt->payload;
    for (size_t i = 0; i < pkt->payload_len; i++) {
        checksum ^= payload_bytes[i];
    }
    return checksum;
}

/**
 * @brief Validate packet magic and checksum
 */
static inline bool validate_packet(const espnow_msg_packet_t *pkt) {
    if (pkt->magic != PROTOCOL_MAGIC) {
        return false;
    }
    return (calculate_packet_checksum(pkt) == pkt->checksum);
}

#ifdef __cplusplus
}
#endif

#endif // SHARED_PROTOCOL_H
