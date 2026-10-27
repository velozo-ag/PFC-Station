#ifndef RADIO_MANAGER_H
#define RADIO_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "sdp_protocol.h"

typedef struct {
    uint8_t mac[6];
    int8_t rssi;
    uint8_t len;
    uint8_t payload[SDP_MAX_PAYLOAD_LEN + sizeof(sdp_header_t)];
} radio_rx_packet_t;

esp_err_t radio_manager_init(void);
esp_err_t radio_manager_send_broadcast(const uint8_t *data, size_t len);
esp_err_t radio_manager_send_unicast(const uint8_t *mac, const uint8_t *data, size_t len);
esp_err_t radio_manager_send_radar_ping(void);
void task_radio_manager(void *pvParameters);

#endif
