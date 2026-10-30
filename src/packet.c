#include <zephyr/sys/crc.h>
#include <zephyr/sys/byteorder.h>
#include <packet.h> 


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
        while(byte != 0xAA){
        }
        rx->buf[0] = 0xAA;
        rx->idx = 1;
        rx->state = PKT_HEADER;
        break;

    case PKT_HEADER:
        /* store byte; once idx == PKT_HEADER_LEN, read length,
           check it, then go to PAYLOAD or CRC */
        
        break;

    case PKT_PAYLOAD:
        /* store byte; once all payload bytes are in, go to CRC */
        break;

    case PKT_CRC:
        /* store byte; once both CRC bytes are in, compute and compare,
           reset, and return DONE or BAD_CRC */
        break;
    }

    return PKT_RX_INCOMPLETE;
}