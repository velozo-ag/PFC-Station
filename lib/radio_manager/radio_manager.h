#ifndef RADIO_MANAGER_H
#define RADIO_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "sdp_protocol.h"

#define RADIO_MAX_DISCOVERED_NODES 16

typedef struct {
    uint8_t mac[6];
    char node_name[16];
    uint8_t node_type;
    bool is_paired;
    uint8_t services_count;
    int8_t rssi;
    uint32_t last_seen_ms;
} discovered_node_t;

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

size_t radio_manager_get_discovered_count(void);
size_t radio_manager_get_discovered_nodes(discovered_node_t *out_nodes, size_t max_count);
void radio_manager_clear_discovered_nodes(void);
void radio_manager_print_nodes_table(void);

#endif
