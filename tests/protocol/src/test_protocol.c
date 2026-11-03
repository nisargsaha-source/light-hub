#include <zephyr/ztest.h>
#include <errno.h>
#include "packet.h"
#include "protocol.h"
#include "bulb_cmd.h"
#include "bulb_mgr.h"

/* ---- Fakes: replace bulb_mgr.c for testing ---- */

/* Same definition as bulb_mgr.c, but no thread consumes it,
 * so tests can look at exactly what proto_handle queued, cant have timing issues (flaky tests) */
K_MSGQ_DEFINE(cmd_q, sizeof(struct bulb_cmd), 8, 4);

/* Fake bulb table: tests fill it in directly */
static struct bulb_cmd fake_bulbs[BULB_COUNT];

int bulb_mgr_get_state(uint8_t id, struct bulb_cmd *out)
{
    if (id >= BULB_COUNT) {
        return -EINVAL;
    }
    *out = fake_bulbs[id];
    out->bulb_id = id;
    return 0;
}

/* ---- Helpers ---- */

/* Put a request straight into a receiver struct, as if pkt_rx_feed had
 * just returned DONE. Skipping the receiver keeps these tests focused
 * on proto_handle; the receiver already has its own tests. */
static void make_request(struct pkt_rx *rx, uint8_t type, uint8_t seq,
                         const uint8_t *payload, uint16_t len)
{
    pkt_rx_reset(rx);
    pkt_build(rx->buf, type, seq, payload, len);
    rx->payload_len = len;
}

/* Check resp is a RESPONSE packet with the given seq and status */
static void check_status(const uint8_t *resp, size_t n, uint8_t seq, uint8_t status)
{
    zassert_equal(n, PKT_HEADER_LEN + 1 + PKT_CRC_LEN, "response length %u", (unsigned)n);
    zassert_equal(resp[2], PKT_TYPE_RESPONSE, "type 0x%02x", resp[2]);
    zassert_equal(resp[3], seq, "seq %u", resp[3]);
    zassert_equal(resp[PKT_HEADER_LEN], status, "status %u", resp[PKT_HEADER_LEN]);
}

/* Runs before every test: empty queue, all bulbs off */
static void reset_fakes(void *fixture)
{
    ARG_UNUSED(fixture);
    k_msgq_purge(&cmd_q);
    memset(fake_bulbs, 0, sizeof(fake_bulbs));
}

ZTEST_SUITE(protocol, NULL, NULL, reset_fakes, NULL, NULL);

/* Valid SET_COLOR: OK response, and the command lands on cmd_q */
ZTEST(protocol, test_set_color_ok)
{
    struct pkt_rx rx;
    struct bulb_cmd cmd;
    uint8_t resp[PKT_MAX_LEN];
    const uint8_t payload[] = { 2, 255, 0, 0 };

    make_request(&rx, PKT_TYPE_SET_COLOR, 7, payload, sizeof(payload));
    size_t n = proto_handle(&rx, resp);

    check_status(resp, n, 7, PKT_STATUS_OK);

    zassert_equal(k_msgq_get(&cmd_q, &cmd, K_NO_WAIT), 0, "nothing queued");
    zassert_equal(cmd.bulb_id, 2, "bulb_id");
    zassert_equal(cmd.r, 255, "r");
    zassert_equal(cmd.g, 0, "g");
    zassert_equal(cmd.b, 0, "b");
}

/* Bulb 9 doesn't exist: INVALID_BULB, and nothing queued */
ZTEST(protocol, test_set_color_invalid_bulb)
{
    struct pkt_rx rx;
    uint8_t resp[PKT_MAX_LEN];
    const uint8_t payload[] = { 9, 0, 255, 0 };

    make_request(&rx, PKT_TYPE_SET_COLOR, 3, payload, sizeof(payload));
    size_t n = proto_handle(&rx, resp);

    check_status(resp, n, 3, PKT_STATUS_INVALID_BULB);
    zassert_equal(k_msgq_num_used_get(&cmd_q), 0, "rejected command was queued");
}

/* Wrong protocol version: BAD_VERSION, nothing queued */
ZTEST(protocol, test_bad_version)
{
    struct pkt_rx rx;
    uint8_t resp[PKT_MAX_LEN];
    const uint8_t payload[] = { 2, 0, 0, 255 };

    make_request(&rx, PKT_TYPE_SET_COLOR, 6, payload, sizeof(payload));
    rx.buf[1] = 0x02;   /* change version after building; proto_handle doesn't recheck the CRC */

    size_t n = proto_handle(&rx, resp);

    check_status(resp, n, 6, PKT_STATUS_BAD_VERSION);
    zassert_equal(k_msgq_num_used_get(&cmd_q), 0, "bad-version command was queued");
}

/* Type the hub doesn't know: UNKNOWN_TYPE */
ZTEST(protocol, test_unknown_type)
{
    struct pkt_rx rx;
    uint8_t resp[PKT_MAX_LEN];

    make_request(&rx, 0x33, 5, NULL, 0);
    size_t n = proto_handle(&rx, resp);

    check_status(resp, n, 5, PKT_STATUS_UNKNOWN_TYPE);
}

/* SET_COLOR with 3 payload bytes instead of 4: BAD_LENGTH, nothing queued */
ZTEST(protocol, test_set_color_bad_length)
{
    struct pkt_rx rx;
    uint8_t resp[PKT_MAX_LEN];
    const uint8_t payload[] = { 2, 255, 0 };

    make_request(&rx, PKT_TYPE_SET_COLOR, 4, payload, sizeof(payload));
    size_t n = proto_handle(&rx, resp);

    check_status(resp, n, 4, PKT_STATUS_BAD_LENGTH);
    zassert_equal(k_msgq_num_used_get(&cmd_q), 0, "bad-length command was queued");
}

/* GET_STATE returns a STATE packet with the bulb's current color */
ZTEST(protocol, test_get_state)
{
    struct pkt_rx rx;
    uint8_t resp[PKT_MAX_LEN];
    const uint8_t payload[] = { 4 };
    const uint8_t expected[] = { 4, 10, 20, 30 };

    /* Pretend bulb 4 is currently R=10 G=20 B=30 */
    fake_bulbs[4] = (struct bulb_cmd){ .bulb_id = 4, .r = 10, .g = 20, .b = 30 };

    make_request(&rx, PKT_TYPE_GET_STATE, 12, payload, sizeof(payload));
    size_t n = proto_handle(&rx, resp);

    /* STATE packet: 6 header + 4 payload + 2 CRC */
    zassert_equal(n, PKT_HEADER_LEN + 4 + PKT_CRC_LEN, "response length %u", (unsigned)n);
    zassert_equal(resp[2], PKT_TYPE_STATE, "type 0x%02x", resp[2]);
    zassert_equal(resp[3], 12, "seq %u", resp[3]);
    zassert_mem_equal(&resp[PKT_HEADER_LEN], expected, sizeof(expected), "state differs");

    /* GET_STATE only reads; it must not queue anything */
    zassert_equal(k_msgq_num_used_get(&cmd_q), 0, "GET_STATE queued a command");
}

/* GET_STATE for a bulb that doesn't exist: INVALID_BULB */
ZTEST(protocol, test_get_state_invalid_bulb)
{
    struct pkt_rx rx;
    uint8_t resp[PKT_MAX_LEN];
    const uint8_t payload[] = { BULB_COUNT };   /* 8: one past the last valid ID */

    make_request(&rx, PKT_TYPE_GET_STATE, 13, payload, sizeof(payload));
    size_t n = proto_handle(&rx, resp);

    check_status(resp, n, 13, PKT_STATUS_INVALID_BULB);
}

/* Queue full: BUSY instead of blocking the receive path */
ZTEST(protocol, test_busy)
{
    struct pkt_rx rx;
    uint8_t resp[PKT_MAX_LEN];
    struct bulb_cmd filler = { .bulb_id = 0, .r = 1, .g = 1, .b = 1 };
    const uint8_t payload[] = { 1, 0, 255, 0 };

    /* Fill all 8 slots, as if bulb_mgr had fallen behind */
    for (int i = 0; i < 8; i++) {
        zassert_equal(k_msgq_put(&cmd_q, &filler, K_NO_WAIT), 0, "fill failed at %d", i);
    }

    make_request(&rx, PKT_TYPE_SET_COLOR, 20, payload, sizeof(payload));
    size_t n = proto_handle(&rx, resp);

    check_status(resp, n, 20, PKT_STATUS_BUSY);
    zassert_equal(k_msgq_num_used_get(&cmd_q), 8, "queue changed");
}

/* A hub-to-host type sent TO the hub is rejected */
ZTEST(protocol, test_response_to_hub_type)
{
    struct pkt_rx rx;
    uint8_t resp[PKT_MAX_LEN];
    const uint8_t payload[] = { PKT_STATUS_OK };

    make_request(&rx, PKT_TYPE_RESPONSE, 21, payload, sizeof(payload));
    size_t n = proto_handle(&rx, resp);

    check_status(resp, n, 21, PKT_STATUS_UNKNOWN_TYPE);
}