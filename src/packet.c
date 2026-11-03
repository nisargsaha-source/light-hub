#include <zephyr/sys/crc.h>
#include <zephyr/sys/byteorder.h>
#include "packet.h"
#include <string.h>


void pkt_rx_reset(struct pkt_rx *rx){
    rx->state = PKT_WAIT_MAGIC;
    rx->idx = 0;
    rx->payload_len = 0;
}

enum pkt_rx_result pkt_rx_feed(struct pkt_rx *rx, uint8_t byte)
{
    switch (rx->state) {
    case PKT_WAIT_MAGIC:
        /* ignore until 0xAA; store it, move to HEADER */
        while(byte != PKT_MAGIC){
            return PKT_RX_INCOMPLETE;
        }
        rx->buf[0] = PKT_MAGIC;
        rx->idx = 1;
        rx->state = PKT_HEADER;
        return PKT_RX_INCOMPLETE;

    case PKT_HEADER:
        rx->buf[rx->idx++] = byte; //uses value then increments post btw
        if (rx->idx < PKT_HEADER_LEN) {
            return PKT_RX_INCOMPLETE;
        }

        /* Full header in: read and check the length BEFORE storing any payload */
        rx->payload_len = sys_get_le16(&rx->buf[4]);
        if (rx->payload_len > PKT_MAX_PAYLOAD) {
            pkt_rx_reset(rx);
            return PKT_RX_BAD_LENGTH;
        }

        rx->state = (rx->payload_len == 0) ? PKT_CRC : PKT_PAYLOAD;
        return PKT_RX_INCOMPLETE;

    case PKT_PAYLOAD:
        rx->buf[rx->idx++] = byte;
        if (rx->idx == PKT_HEADER_LEN + rx->payload_len) {
            rx->state = PKT_CRC;
        }
        return PKT_RX_INCOMPLETE;

    case PKT_CRC: {
        size_t crc_off = PKT_HEADER_LEN + rx->payload_len;

        rx->buf[rx->idx++] = byte;
        if (rx->idx < crc_off + PKT_CRC_LEN) {
            return PKT_RX_INCOMPLETE;
        }

        /* CRC covers version..end of payload */
        uint16_t received = sys_get_le16(&rx->buf[crc_off]);
        uint16_t computed = crc16_itu_t(0xFFFF, &rx->buf[0], crc_off);

        /* Ready for the next packet, but keep buf and payload_len for the caller */
        rx->state = PKT_WAIT_MAGIC;
        rx->idx = 0;

        return (received == computed) ? PKT_RX_DONE : PKT_RX_BAD_CRC;
    }
    }

    /* Unknown state (memory corruption?): recover instead of misbehaving */
    pkt_rx_reset(rx);
    return PKT_RX_INCOMPLETE;
}

/* Build a packet into out (must hold PKT_MAX_LEN bytes).
 * Returns the total packet length, or 0 if len > PKT_MAX_PAYLOAD. */
size_t pkt_build(uint8_t *out, uint8_t type, uint8_t seq, const uint8_t *payload, uint16_t len){
    if(len > PKT_MAX_PAYLOAD){
        return 0;
    }
    out[0] = PKT_MAGIC;
    out[1] = PKT_VERSION;
    out[2] = type;
    out[3] = seq;
    sys_put_le16(len, &out[4]); //writes the length as 2 bytes, low byte first
    if(len >0){
        memcpy(&out[6], payload,len);
    }
    uint16_t crc = crc16_itu_t(0XFFFF, &out[0], 6+len);
    sys_put_le16(crc, &out[6+len]);
    return (PKT_HEADER_LEN+len+PKT_CRC_LEN);
}