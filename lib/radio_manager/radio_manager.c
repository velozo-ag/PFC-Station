#include "radio_manager.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdp_protocol.h"

static QueueHandle_t s_radio_rx_queue = NULL;
static discovered_node_t s_discovered_nodes[RADIO_MAX_DISCOVERED_NODES];
static size_t s_discovered_count = 0;
static SemaphoreHandle_t s_discovered_mutex = NULL;

static void radio_recv_cb(const esp_now_recv_info_t* esp_now_info,
                          const uint8_t* data, int data_len) {
    if (esp_now_info == NULL || data == NULL || data_len <= 0) {
        return;
    }

    if (data_len > (int)(SDP_MAX_PAYLOAD_LEN + sizeof(sdp_header_t))) {
        return;
    }

    radio_rx_packet_t packet;
    memcpy(packet.mac, esp_now_info->src_addr, 6);
    packet.rssi =
        esp_now_info->rx_ctrl != NULL ? esp_now_info->rx_ctrl->rssi : 0;
    packet.len = (uint8_t)data_len;
    memcpy(packet.payload, data, data_len);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (s_radio_rx_queue != NULL) {
        xQueueSendFromISR(s_radio_rx_queue, &packet, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

static void radio_send_cb(const esp_now_send_info_t* tx_info,
                          esp_now_send_status_t status) {}

esp_err_t radio_manager_init(void) {
    if (s_discovered_mutex == NULL) {
        s_discovered_mutex = xSemaphoreCreateMutex();
        if (s_discovered_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    s_radio_rx_queue = xQueueCreate(10, sizeof(radio_rx_packet_t));
    if (s_radio_rx_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_netif_init();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_wifi_set_channel(SDP_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_now_init();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_now_register_recv_cb(radio_recv_cb);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_now_register_send_cb(radio_send_cb);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_now_peer_info_t broadcast_peer = {0};
    memset(broadcast_peer.peer_addr, 0xFF, 6);
    broadcast_peer.channel = SDP_WIFI_CHANNEL;
    broadcast_peer.ifidx = WIFI_IF_STA;
    broadcast_peer.encrypt = false;

    ret = esp_now_add_peer(&broadcast_peer);
    if (ret != ESP_OK && ret != ESP_ERR_ESPNOW_EXIST) {
        return ret;
    }

    return ESP_OK;
}

esp_err_t radio_manager_send_broadcast(const uint8_t* data, size_t len) {
    static const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF,
                                             0xFF, 0xFF, 0xFF};
    return esp_now_send(broadcast_mac, data, len);
}

esp_err_t radio_manager_send_unicast(const uint8_t* mac, const uint8_t* data,
                                     size_t len) {
    if (mac == NULL || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peer = {0};
        memcpy(peer.peer_addr, mac, 6);
        peer.channel = SDP_WIFI_CHANNEL;
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = false;
        esp_err_t err = esp_now_add_peer(&peer);
        if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
            return err;
        }
    }

    return esp_now_send(mac, data, len);
}

esp_err_t radio_manager_send_radar_ping(void) {
    sdp_packet_t packet;
    packet.header.magic = SDP_MAGIC;
    packet.header.msg_type = SDP_MSG_RADAR_PING;
    packet.header.sender_id = SDP_STATION_ID;
    packet.header.target_id = SDP_BROADCAST_ID;
    packet.header.seq_num = 0;
    packet.header.payload_len = sizeof(sdp_radar_ping_payload_t);

    sdp_radar_ping_payload_t ping;
    ping.protocol_version = 1;
    ping.reserved = 0;
    memcpy(packet.payload, &ping, sizeof(ping));

    size_t total_len = sizeof(sdp_header_t) + sizeof(ping);
    return radio_manager_send_broadcast((const uint8_t*)&packet, total_len);
}

// imprime en serial una tabla con todos los nodos que almacena en ram
void radio_manager_print_nodes_table(void) {
    if (s_discovered_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_discovered_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        printf("\n=================== NODOS EN RAM (%zu) ===================\n",
               s_discovered_count);
        if (s_discovered_count == 0) {
            printf("  (Ningun nodo descubierto aun)\n");
        } else {
            for (size_t i = 0; i < s_discovered_count; i++) {
                printf(
                    "#%02zu | MAC: %02X:%02X:%02X:%02X:%02X:%02X | Name: %-14s "
                    "| Tipo: %u | Serv: %u | RSSI: %3d dBm\n",
                    i + 1, s_discovered_nodes[i].mac[0],
                    s_discovered_nodes[i].mac[1], s_discovered_nodes[i].mac[2],
                    s_discovered_nodes[i].mac[3], s_discovered_nodes[i].mac[4],
                    s_discovered_nodes[i].mac[5],
                    s_discovered_nodes[i].node_name,
                    s_discovered_nodes[i].node_type,
                    s_discovered_nodes[i].services_count,
                    s_discovered_nodes[i].rssi);
            }
        }
        printf(
            "==============================================================="
            "\n\n");
        xSemaphoreGive(s_discovered_mutex);
    }
}

size_t radio_manager_get_discovered_count(void) {
    size_t count = 0;
    if (s_discovered_mutex != NULL &&
        xSemaphoreTake(s_discovered_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        count = s_discovered_count;
        xSemaphoreGive(s_discovered_mutex);
    }
    return count;
}

/*
copia los nodos descubiertos, los copia en una variable y los
retorna, liberando rapidamente el mutex
*/
size_t radio_manager_get_discovered_nodes(discovered_node_t* out_nodes,
                                          size_t max_count) {
    if (out_nodes == NULL || max_count == 0) {
        return 0;
    }

    size_t copied = 0;
    if (s_discovered_mutex != NULL &&
        xSemaphoreTake(s_discovered_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        copied =
            s_discovered_count < max_count ? s_discovered_count : max_count;
        memcpy(out_nodes, s_discovered_nodes,
               copied * sizeof(discovered_node_t));
        xSemaphoreGive(s_discovered_mutex);
    }
    return copied;
}

void radio_manager_clear_discovered_nodes(void) {
    if (s_discovered_mutex != NULL &&
        xSemaphoreTake(s_discovered_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_discovered_count = 0;
        memset(s_discovered_nodes, 0, sizeof(s_discovered_nodes));
        xSemaphoreGive(s_discovered_mutex);
        printf("[SDP] RAM Nodes clear.\n");
    }
}

void task_radio_manager(void* pvParameters) {
    radio_rx_packet_t rx_pkt;
    int64_t last_radar_ping_us = 0;

    while (1) {
        // Ping Automatico cada 3s para pruebas
        int64_t now_us = esp_timer_get_time();
        if ((now_us - last_radar_ping_us) >= 3000000LL) {
            radio_manager_send_radar_ping();
            printf("[SDP Radar] Ping broadcast transmitido...\n");
            last_radar_ping_us = now_us;
        }
        // -----------------------------------------------

        if (xQueueReceive(s_radio_rx_queue, &rx_pkt, pdMS_TO_TICKS(100))) {
            // valida que al menos tenga tamano del header, y si tiene el magic
            // correcto
            if (rx_pkt.len < sizeof(sdp_header_t)) {
                continue;
            }
            sdp_header_t* hdr = (sdp_header_t*)rx_pkt.payload;
            if (hdr->magic != SDP_MAGIC) {
                continue;
            }

            printf(
                "[Radio Core0] RX type=0x%02X from "
                "%02X:%02X:%02X:%02X:%02X:%02X RSSI=%d\n",
                hdr->msg_type, rx_pkt.mac[0], rx_pkt.mac[1], rx_pkt.mac[2],
                rx_pkt.mac[3], rx_pkt.mac[4], rx_pkt.mac[5], rx_pkt.rssi);

            // para los PROBE msg's
            if (hdr->msg_type == SDP_MSG_PROBE_RESP) {
                if (hdr->payload_len >= sizeof(sdp_probe_resp_payload_t)) {

                    sdp_probe_resp_payload_t* probe =
                        (sdp_probe_resp_payload_t*)(rx_pkt.payload +
                                                    sizeof(sdp_header_t));
                    uint32_t now_ms =
                        (uint32_t)(esp_timer_get_time() / 1000ULL);

                    if (s_discovered_mutex != NULL &&
                        xSemaphoreTake(s_discovered_mutex,
                                       pdMS_TO_TICKS(100)) == pdTRUE) {
                        int found_idx = -1;
                        for (size_t i = 0; i < s_discovered_count; i++) {
                            if (memcmp(s_discovered_nodes[i].mac, rx_pkt.mac,
                                       6) == 0) {
                                found_idx = (int)i;
                                break;
                            }
                        }

                        if (found_idx >= 0) {
                            s_discovered_nodes[found_idx].rssi = rx_pkt.rssi;
                            s_discovered_nodes[found_idx].last_seen_ms = now_ms;
                            s_discovered_nodes[found_idx].is_paired =
                                probe->is_paired;
                            s_discovered_nodes[found_idx].services_count =
                                probe->services_count;
                            strncpy(
                                s_discovered_nodes[found_idx].node_name,
                                probe->node_name,
                                sizeof(
                                    s_discovered_nodes[found_idx].node_name) -
                                    1);
                            s_discovered_nodes[found_idx]
                                .node_name[sizeof(s_discovered_nodes[found_idx]
                                                      .node_name) -
                                           1] = '\0';
                            printf("[SDP] Nodo actualizado en RAM: %s\n",
                                   s_discovered_nodes[found_idx].node_name);
                        } else if (s_discovered_count <
                                   RADIO_MAX_DISCOVERED_NODES) {
                            memcpy(s_discovered_nodes[s_discovered_count].mac,
                                   rx_pkt.mac, 6);
                            strncpy(
                                s_discovered_nodes[s_discovered_count]
                                    .node_name,
                                probe->node_name,
                                sizeof(s_discovered_nodes[s_discovered_count]
                                           .node_name) -
                                    1);
                            s_discovered_nodes[s_discovered_count].node_name
                                [sizeof(s_discovered_nodes[s_discovered_count]
                                            .node_name) -
                                 1] = '\0';
                            s_discovered_nodes[s_discovered_count].node_type =
                                probe->node_type;
                            s_discovered_nodes[s_discovered_count].is_paired =
                                probe->is_paired;
                            s_discovered_nodes[s_discovered_count]
                                .services_count = probe->services_count;
                            s_discovered_nodes[s_discovered_count].rssi =
                                rx_pkt.rssi;
                            s_discovered_nodes[s_discovered_count]
                                .last_seen_ms = now_ms;
                            printf(
                                "[SDP] Nuevo nodo agregado a RAM: %s (#%zu)\n",
                                s_discovered_nodes[s_discovered_count]
                                    .node_name,
                                s_discovered_count + 1);
                            s_discovered_count++;
                        }
                        xSemaphoreGive(s_discovered_mutex);
                    }

                    radio_manager_print_nodes_table();
                }
            }
        }
    }
}
