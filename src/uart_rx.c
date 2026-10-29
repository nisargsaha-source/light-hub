#include <errno.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include "bulb_cmd.h"
#include "bulb_mgr.h"
#include "parser.h"
#include "uart_rx.h"

LOG_MODULE_REGISTER(uart_rx, LOG_LEVEL_INF);

#define LINE_MAX 64

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

/* RX thread: turns the byte stream from the ISR into complete lines */
static void rx_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    char line[LINE_MAX];
    size_t len = 0;
    bool overflow = false;
    uint8_t byte;

    while (1) {
        /* Sleep until the ISR queues a byte */
        k_msgq_get(&rx_q, &byte, K_FOREVER);

        /* Enter key: end of line */
        if (byte == '\r' || byte == '\n') {
            printk("\n");

            if (overflow) {
                LOG_WRN("Line too long, discarded");
            } else if (len > 0) {
                line[len] = '\0'; /* terminate the C string */
                LOG_DBG("Got line: %s", line);

                struct bulb_cmd cmd;

                if (parse_bulb_cmd(line, &cmd) == 0) {
                    k_msgq_put(&cmd_q, &cmd, K_FOREVER);
                } else {
                    LOG_WRN("Invalid command. Format: <bulb> <r> <g> <b>, each 0-255");
                }
            }

            /* Empty lines (len == 0) are ignored, e.g. the \n after a \r */
            len = 0;
            overflow = false;
            continue;
        }

        /* Echo the character so the user can see what they type */
        printk("%c", byte);

        /* Leave room for the '\0' terminator */
        if (len < LINE_MAX - 1) {
            line[len++] = byte;
        } else {
            overflow = true; /* keep discarding until Enter */
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
