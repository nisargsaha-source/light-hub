#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "protocol.h"
#include "packet.h"
#include "bulb_cmd.h"
#include "bulb_mgr.h"

LOG_MODULE_REGISTER(protocol, LOG_LEVEL_INF);

/* Expected payload sizes per message type */
#define SET_COLOR_LEN 4
#define GET_STATE_LEN 1

/* Build a RESPONSE packet carrying one status byte */
static size_t respond_status(uint8_t *resp, uint8_t seq, uint8_t status)
{
    return pkt_build(resp, PKT_TYPE_RESPONSE, seq, &status, 1);
}

size_t proto_handle(const struct pkt_rx *rx, uint8_t *resp)
{
    /* Header fields, at the offsets from PROTOCOL.md */
    uint8_t version = rx->buf[1];
    uint8_t type    = rx->buf[2];
    uint8_t seq     = rx->buf[3];
    uint16_t len    = rx->payload_len;
    const uint8_t *payload = &rx->buf[PKT_HEADER_LEN];

    /* Rule 5: version */
    if (version != PKT_VERSION) {
        LOG_WRN("Bad version %u (seq %u)", version, seq);
        return respond_status(resp, seq, PKT_STATUS_BAD_VERSION);
    }

    switch (type) {

    case PKT_TYPE_SET_COLOR: {
        /* Rule 7: length for this type */
        if (len != SET_COLOR_LEN) {
            return respond_status(resp, seq, PKT_STATUS_BAD_LENGTH);
        }
        /* Rule 8: bulb ID */
        if (payload[0] >= BULB_COUNT) {
            return respond_status(resp, seq, PKT_STATUS_INVALID_BULB);
        }

        struct bulb_cmd cmd = {
            .bulb_id = payload[0],
            .r = payload[1],
            .g = payload[2],
            .b = payload[3],
        };

        /* Never block the receive path: if bulb_mgr is behind, say so */
        if (k_msgq_put(&cmd_q, &cmd, K_NO_WAIT) != 0) {
            LOG_WRN("Command queue full (seq %u)", seq);
            return respond_status(resp, seq, PKT_STATUS_BUSY);
        }
        return respond_status(resp, seq, PKT_STATUS_OK);
    }

    case PKT_TYPE_GET_STATE: {
        struct bulb_cmd state;

        if (len != GET_STATE_LEN) {
            return respond_status(resp, seq, PKT_STATUS_BAD_LENGTH);
        }
        if (bulb_mgr_get_state(payload[0], &state) != 0) {
            return respond_status(resp, seq, PKT_STATUS_INVALID_BULB);
        }

        uint8_t out[4] = { state.bulb_id, state.r, state.g, state.b };
        return pkt_build(resp, PKT_TYPE_STATE, seq, out, sizeof(out));
    }

    default:
        /* Rule 6: unknown type. This also covers hub-to-host types
         * like RESPONSE being sent *to* the hub, which makes no sense. */
        LOG_WRN("Unknown type 0x%02x (seq %u)", type, seq);
        return respond_status(resp, seq, PKT_STATUS_UNKNOWN_TYPE);
    }
}