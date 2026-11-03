#include <errno.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include "packet.h"
#include "protocol.h"

#include "bulb_cmd.h"
#include "bulb_mgr.h"

#include "uart_link.h"

#define PKT_RX_TIMEOUT_MS 100

LOG_MODULE_REGISTER(uart_rx, LOG_LEVEL_INF);


/* DEVICE_DT_GET(node_id): pointer to a device's driver instance from its devicetree node.
 * DT_CHOSEN(zephyr_console) means "the node devicetree has chosen as the console". */
static const struct device *const uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

/* Byte queue: 1-byte messages, 32 slots, alignment 1 */
K_MSGQ_DEFINE(rx_q, 1, 32, 1);

/* Bytes dropped because rx_q was full. Written in the ISR, read by threads. */
static volatile uint32_t rx_dropped;

/* ISR callback: move every received byte into rx_q */
static void uart_receive(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    uint8_t byte;

    /* Call first to refresh the interrupt status */
    if (!uart_irq_update(dev)) {
        return;
    }

    if (uart_irq_rx_ready(dev)) {
        while (uart_fifo_read(dev, &byte, 1)) {
            if (k_msgq_put(&rx_q, &byte, K_NO_WAIT) != 0) {
                rx_dropped++; /* queue full: count it, don't print in the ISR */
            }
        }
    }
}

/* RX thread: turns the byte stream from the ISR into packets,
 * hands complete packets to the protocol handler, sends responses */
static void rx_thread(void *p1, void *p2, void *p3)
{
    struct pkt_rx rx;
    uint8_t resp[PKT_MAX_LEN];//response packet
    uint8_t byte;

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    pkt_rx_reset(&rx);

    while (1) {
        /* Idle: wait forever. Mid-packet: give up after 100 ms */
        k_timeout_t timeout = (rx.state == PKT_WAIT_MAGIC)
                              ? K_FOREVER
                              : K_MSEC(PKT_RX_TIMEOUT_MS);

        if (k_msgq_get(&rx_q, &byte, timeout) != 0) {
            LOG_WRN("Packet timed out, resyncing");
            pkt_rx_reset(&rx);
            continue;
        }

        switch (pkt_rx_feed(&rx, byte)) { //byte gotten from rx_q, which is then fed to rx
        case PKT_RX_DONE: {
            size_t n = proto_handle(&rx, resp);//response packet got build
            if (n > 0) {
                uart_send(resp, n);
            }
            break;
        }
        case PKT_RX_BAD_CRC:
            LOG_WRN("Bad CRC, packet dropped");
            break;
        case PKT_RX_BAD_LENGTH:
            LOG_WRN("Length over %d, packet dropped", PKT_MAX_PAYLOAD);
            break;
        case PKT_RX_INCOMPLETE:
        default:
            break;
        }
    }
}


/* 1KB stack, priority 5: runs ahead of bulb_mgr_thread (priority 7) */
K_THREAD_DEFINE(rx_id, 1024, rx_thread, NULL, NULL, NULL, 5, 0, 0);

int uart_rx_init(void)
{
    int err;

    if (!device_is_ready(uart)) {
        return -ENODEV;
    }

    err = uart_irq_callback_user_data_set(uart, uart_receive, NULL);
    if (err) {
        return err;
    }

    uart_irq_rx_enable(uart);
    return 0;
}

void uart_send(const uint8_t *data, size_t len){
    for(size_t i=0; i<len; i++){
        uart_poll_out(uart, data[i]);
    }

}