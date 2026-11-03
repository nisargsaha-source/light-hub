#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bulb_cmd.h"
#include "bulb_mgr.h"

static struct bulb_cmd bulbs[8];

LOG_MODULE_REGISTER(bulb_mgr, LOG_LEVEL_INF);

/* Queue holding up to 8 bulb_cmd structs, buffer aligned to 4 bytes */
K_MSGQ_DEFINE(cmd_q, sizeof(struct bulb_cmd), 8, 4);
/*define mutex*/
K_MUTEX_DEFINE(bulbs_lock);

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
        k_mutex_lock(&bulbs_lock, K_FOREVER);
        bulbs[cmd.bulb_id]= cmd;   
        k_mutex_unlock(&bulbs_lock);    
        LOG_INF("Bulb %u -> R=%u G=%u B=%u", cmd.bulb_id, cmd.r, cmd.g, cmd.b); 
    }
}
/* Copy bulb id's current state into *out. Returns 0, or -EINVAL if id > 7. */
int bulb_mgr_get_state(uint8_t id, struct bulb_cmd *out){
    if(id >= BULB_COUNT) return -EINVAL;
    k_mutex_lock(&bulbs_lock, K_FOREVER);
    *out = bulbs[id];
    k_mutex_unlock(&bulbs_lock);
    return 0;
}

/* 1KB stack, priority 7 (lower number = higher priority) */
K_THREAD_DEFINE(bulb_mgr_id, 1024, bulb_mgr_thread, NULL, NULL, NULL, 7, 0, 0);
