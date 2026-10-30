#include <stdint.h>
#include <stddef.h>

#define PKT_MAGIC        0xAA
#define PKT_VERSION      0x01
#define PKT_HEADER_LEN   6
#define PKT_MAX_PAYLOAD  32
#define PKT_CRC_LEN      2
#define PKT_MAX_LEN      (PKT_HEADER_LEN + PKT_MAX_PAYLOAD + PKT_CRC_LEN)

enum pkt_rx_state {
    PKT_WAIT_MAGIC,
    PKT_HEADER,
    PKT_PAYLOAD,
    PKT_CRC,
};

/* Receiver state: everything it must remember between bytes */
struct pkt_rx {
    enum pkt_rx_state state;
    uint8_t buf[PKT_MAX_LEN];   /* the raw packet as it arrives */
    size_t idx;                 /* how many bytes are in buf so far */
    uint16_t payload_len;       /* read from the header */
};

/* Results of feeding one byte */
enum pkt_rx_result {
    PKT_RX_INCOMPLETE,   /* need more bytes */
    PKT_RX_DONE,         /* a full packet with valid CRC is in buf */
    PKT_RX_BAD_CRC,      /* full packet arrived, CRC mismatch */
    PKT_RX_BAD_LENGTH,   /* header claimed length > 32 */
};

void pkt_rx_reset(struct pkt_rx *rx);
enum pkt_rx_result pkt_rx_feed(struct pkt_rx *rx, uint8_t byte);