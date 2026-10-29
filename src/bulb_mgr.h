#pragma once

#include <zephyr/kernel.h>

/* Queue of struct bulb_cmd. Producers put commands here, bulb_mgr_thread consumes them. */
extern struct k_msgq cmd_q;
