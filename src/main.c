#include <zephyr/logging/log.h>

#include "uart_link.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
    int err;

    LOG_INF("Light hub starting");

    err = uart_rx_init();
    if (err) {
        LOG_ERR("UART init failed: %d", err);
        return 0;
    }

    LOG_INF("Ready. Type: <bulb> <r> <g> <b>");

    return 0; /* main ends, but the threads keep running */
}
