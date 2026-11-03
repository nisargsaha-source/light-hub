#pragma once

#include <zephyr/kernel.h>

/* Queue of struct bulb_cmd. Producers put commands here, bulb_mgr_thread consumes them. */
extern struct k_msgq cmd_q;
int bulb_mgr_get_state(uint8_t id, struct bulb_cmd *out);