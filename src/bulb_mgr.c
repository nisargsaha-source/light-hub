#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bulb_cmd.h"
#include "bulb_mgr.h"

LOG_MODULE_REGISTER(bulb_mgr, LOG_LEVEL_INF);

/* Queue holding up to 8 bulb_cmd structs, buffer aligned to 4 bytes */
K_MSGQ_DEFINE(cmd_q, sizeof(struct bulb_cmd), 8, 4);

/* Consumer: applies bulb commands (for now, just prints them) */
static void bulb_mgr_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct bulb_cmd cmd;

    while (1) {
        /* Sleep until a message arrives, then copy it into cmd */
        k_msgq_get(&cmd_q, &cmd, K_FOREVER);

        LOG_INF("Bulb %u -> R=%u G=%u B=%u",
                cmd.bulb_id, cmd.r, cmd.g, cmd.b);
    }
}

/* 1KB stack, priority 7 (lower number = higher priority) */
K_THREAD_DEFINE(bulb_mgr_id, 1024, bulb_mgr_thread, NULL, NULL, NULL, 7, 0, 0);
