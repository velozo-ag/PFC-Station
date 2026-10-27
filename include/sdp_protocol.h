#ifndef SDP_PROTOCOL_H
#define SDP_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define SDP_MAGIC 0x50
#define SDP_WIFI_CHANNEL 1
#define SDP_BROADCAST_ID 0xFF
#define SDP_STATION_ID 0x01
#define SDP_UNASSIGNED_ID 0x00

#define SDP_MAX_PAYLOAD_LEN 240
#define SDP_MAX_NAME_LEN 16
#define SDP_MAX_LABEL_LEN 16
#define SDP_MAX_UNIT_LEN 6

#pragma pack(push, 1)

typedef enum {
    SDP_MSG_RADAR_PING    = 0x01,
    SDP_MSG_PROBE_RESP    = 0x02,
    SDP_MSG_PAIR_REQ      = 0x03,
    SDP_MSG_PAIR_ACK      = 0x04,
    SDP_MSG_MANIFEST_REQ  = 0x05,
    SDP_MSG_MANIFEST_RESP = 0x06,
    SDP_MSG_CMD           = 0x07,
    SDP_MSG_CMD_ACK       = 0x08,
    SDP_MSG_STATE_REQ     = 0x09,
    SDP_MSG_STATE_RESP    = 0x0A,
    SDP_MSG_SYS_DIAG_REQ  = 0x0B,
    SDP_MSG_SYS_DIAG_RESP = 0x0C,
    SDP_MSG_SYS_REBOOT    = 0x0D,
    SDP_MSG_UNPAIR        = 0x0E
} sdp_msg_type_t;

typedef struct {
    uint8_t magic;
    uint8_t msg_type;
    uint8_t sender_id;
    uint8_t target_id;
    uint8_t seq_num;
    uint8_t payload_len;
} sdp_header_t;

typedef struct {
    uint8_t protocol_version;
    uint8_t reserved;
} sdp_radar_ping_payload_t;

typedef struct {
    uint8_t hw_uid[6];
    char node_name[SDP_MAX_NAME_LEN];
    uint8_t node_type;
    uint8_t is_paired;
    uint8_t services_count;
} sdp_probe_resp_payload_t;

typedef struct {
    uint8_t hw_uid[6];
    uint8_t assigned_node_id;
    uint8_t session_key[16];
} sdp_pair_req_payload_t;

typedef struct {
    uint8_t hw_uid[6];
    uint8_t status;
} sdp_pair_ack_payload_t;

typedef enum {
    WIDGET_TYPE_SWITCH        = 0x01,
    WIDGET_TYPE_LIST_SELECTOR = 0x02,
    WIDGET_TYPE_SLIDER        = 0x03,
    WIDGET_TYPE_NUM_DISPLAY   = 0x04
} sdp_widget_type_t;

typedef struct {
    uint8_t service_index;
} sdp_manifest_req_payload_t;

typedef struct {
    uint8_t service_index;
    uint8_t widget_type;
    char label[SDP_MAX_LABEL_LEN];
    int32_t min_value;
    int32_t max_value;
    int32_t step_value;
    int32_t current_value;
    char unit[SDP_MAX_UNIT_LEN];
} sdp_manifest_resp_payload_t;

typedef struct {
    uint8_t service_index;
    int32_t value;
} sdp_cmd_payload_t;

typedef struct {
    uint8_t service_index;
    uint8_t status;
} sdp_cmd_ack_payload_t;

typedef struct {
    uint32_t free_heap;
    uint32_t uptime_sec;
    int8_t rssi;
    uint8_t battery_pct;
} sdp_sys_diag_payload_t;

typedef struct {
    sdp_header_t header;
    uint8_t payload[SDP_MAX_PAYLOAD_LEN];
} sdp_packet_t;

#pragma pack(pop)

#endif
