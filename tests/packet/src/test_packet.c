#include <zephyr/ztest.h>
#include <zephyr/sys/crc.h>
#include <string.h>
#include "packet.h"

/* Golden vector: SET_COLOR, seq 7, bulb 2 -> red.
 * CRC 0x39DF computed independently (Python binascii.crc_hqx), stored little-endian. */
static const uint8_t set_color_pkt[] = {
    0xAA, 0x01, 0x01, 0x07, 0x04, 0x00,   /* header */
    0x02, 0xFF, 0x00, 0x00,               /* payload: bulb 2, R=255, G=0, B=0 */
    0xDF, 0x39                            /* CRC */
};

/* Feed len bytes into rx; return the result for the last byte */
static enum pkt_rx_result feed_all(struct pkt_rx *rx, const uint8_t *data, size_t len)
{
    enum pkt_rx_result res = PKT_RX_INCOMPLETE;

    for (size_t i = 0; i < len; i++) {
        res = pkt_rx_feed(rx, data[i]);
    }
    return res;
}

ZTEST_SUITE(packet, NULL, NULL, NULL, NULL, NULL);

/* Right CRC variant: CRC-16/CCITT-FALSE check value */
ZTEST(packet, test_crc_check_value)
{
    const uint8_t data[] = "123456789";
    uint16_t crc = crc16_itu_t(0xFFFF, data, 9);   /* 9 = skip the '\0' */

    zassert_equal(crc, 0x29B1, "got 0x%04x", crc);
}

/* pkt_build produces exactly the golden vector */
ZTEST(packet, test_build_set_color)
{
    uint8_t out[PKT_MAX_LEN];
    const uint8_t payload[] = { 0x02, 0xFF, 0x00, 0x00 };

    size_t n = pkt_build(out, PKT_TYPE_SET_COLOR, 7, payload, sizeof(payload));

    zassert_equal(n, sizeof(set_color_pkt), "length %u", (unsigned)n);
    zassert_mem_equal(out, set_color_pkt, sizeof(set_color_pkt), "bytes differ");
}

/* A valid packet is accepted, and buf holds the right fields */
ZTEST(packet, test_rx_valid)
{
    struct pkt_rx rx;
    pkt_rx_reset(&rx);

    enum pkt_rx_result res = feed_all(&rx, set_color_pkt, sizeof(set_color_pkt));

    zassert_equal(res, PKT_RX_DONE, "result %d", res);
    zassert_equal(rx.payload_len, 4, "payload_len %u", rx.payload_len);
    zassert_equal(rx.buf[2], PKT_TYPE_SET_COLOR, "type");
    zassert_equal(rx.buf[3], 7, "seq");
    zassert_mem_equal(&rx.buf[PKT_HEADER_LEN], &set_color_pkt[PKT_HEADER_LEN], 4,
                      "payload differs");
}

/* One byte short: not finished yet */
ZTEST(packet, test_rx_incomplete)
{
    struct pkt_rx rx;
    pkt_rx_reset(&rx);

    enum pkt_rx_result res = feed_all(&rx, set_color_pkt, sizeof(set_color_pkt) - 1);

    zassert_equal(res, PKT_RX_INCOMPLETE, "result %d", res);
}

/* Corrupted CRC is rejected */
ZTEST(packet, test_rx_bad_crc)
{
    struct pkt_rx rx;
    uint8_t pkt[sizeof(set_color_pkt)];

    pkt_rx_reset(&rx);
    memcpy(pkt, set_color_pkt, sizeof(pkt));
    pkt[sizeof(pkt) - 1] ^= 0xFF;   /* flip the last CRC byte */

    enum pkt_rx_result res = feed_all(&rx, pkt, sizeof(pkt));

    zassert_equal(res, PKT_RX_BAD_CRC, "result %d", res);
}

/* Length 500 (0x01F4) is rejected as soon as the header is complete */
ZTEST(packet, test_rx_bad_length)
{
    struct pkt_rx rx;
    const uint8_t hdr[] = { 0xAA, 0x01, 0x01, 0x07, 0xF4, 0x01 };

    pkt_rx_reset(&rx);

    /* First 5 bytes: still collecting the header */
    zassert_equal(feed_all(&rx, hdr, 5), PKT_RX_INCOMPLETE, "rejected too early");

    /* 6th byte completes the header: must fail right here, without waiting */
    zassert_equal(pkt_rx_feed(&rx, hdr[5]), PKT_RX_BAD_LENGTH, "not rejected");
    zassert_equal(rx.state, PKT_WAIT_MAGIC, "receiver didn't reset");
}

/*check if junk bytes then packet stll returns PKT_RX_DONE*/
ZTEST(packet, test_rx_garbage_then_valid)
{
    struct pkt_rx rx;
    const uint8_t garbage[]= {0x01, 0xAD, 0xBF, 0x90, 0x5D, 0x01, 0xAD, 0xBF, 0x90, 0x5D, 0x01, 0xAD, 0xBF, 0x90, 0x5D};

    pkt_rx_reset(&rx);

    /*first 1just5 bytes are garb*/
    zassert_equal(feed_all(&rx, garbage, sizeof(garbage)), PKT_RX_INCOMPLETE, "did'nt understand  garbage ");

    /*see its still waiting for magic byte*/
    zassert_equal(rx.state, PKT_WAIT_MAGIC, "garbage started a packet");
    /*get proper*/
    enum pkt_rx_result res = feed_all(&rx, set_color_pkt, sizeof(set_color_pkt));

    zassert_equal(res, PKT_RX_DONE, "result %d", res);
    zassert_equal(rx.payload_len, 4, "payload_len %u", rx.payload_len);
    zassert_equal(rx.buf[2], PKT_TYPE_SET_COLOR, "type");
    zassert_equal(rx.buf[3], 7, "seq");
    zassert_mem_equal(&rx.buf[PKT_HEADER_LEN], &set_color_pkt[PKT_HEADER_LEN], 4,
                      "payload differs");
}

/* Two packets back to back: both accepted (checks the reset between packets) */
ZTEST(packet, test_rx_two_packets)
{
    struct pkt_rx rx;
    enum pkt_rx_result res;

    pkt_rx_reset(&rx);

    res = feed_all(&rx, set_color_pkt, sizeof(set_color_pkt));
    zassert_equal(res, PKT_RX_DONE, "first packet: result %d", res);

    /* No pkt_rx_reset here on purpose: the receiver must reset itself */
    res = feed_all(&rx, set_color_pkt, sizeof(set_color_pkt));
    zassert_equal(res, PKT_RX_DONE, "second packet: result %d", res);
}

/* Anything pkt_build makes, pkt_rx_feed accepts */
ZTEST(packet, test_roundtrip)
{
    struct pkt_rx rx;
    uint8_t pkt[PKT_MAX_LEN];
    const uint8_t payload[] = { 0x02 };   /* GET_STATE for bulb 2 */

    size_t n = pkt_build(pkt, PKT_TYPE_GET_STATE, 42, payload, sizeof(payload));
    zassert_true(n > 0, "pkt_build failed");

    pkt_rx_reset(&rx);
    enum pkt_rx_result res = feed_all(&rx, pkt, n);

    zassert_equal(res, PKT_RX_DONE, "result %d", res);
    zassert_equal(rx.payload_len, 1, "payload_len %u", rx.payload_len);
    zassert_equal(rx.buf[2], PKT_TYPE_GET_STATE, "type");
    zassert_equal(rx.buf[3], 42, "seq");
    zassert_equal(rx.buf[PKT_HEADER_LEN], 0x02, "bulb id");
}