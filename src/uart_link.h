#pragma once

/*
 * Set up interrupt-driven UART RX on the console UART.
 * Returns 0 on success, -ENODEV if the UART isn't ready,
 * or the error from uart_irq_callback_user_data_set().
 */
int uart_rx_init(void);
void uart_send(const uint8_t *data, size_t len);
