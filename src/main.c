#include <stdio.h>
#include <zephyr/kernel.h>

struct bulb_cmd{
    uint8_t bulb_id;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

/*Queue holding up to 8 bulb_cmd structs, buffer alighned to 4 bytes*/
K_MSGQ_DEFINE(cmd_q, sizeof(struct bulb_cmd), 8, 4);

//Producer makes fake command every sec and queues it
void producer(void *p1, void *p2, void *p3){
    /* A few colors to cycle through: red, green, blue, white */
    const uint8_t colors[4][3] = {
        {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}
    };
    uint8_t i = 0;

    while (1){
        struct bulb_cmd cmd = {
            .bulb_id = i % 3,          /* bulbs 0, 1, 2 */
            .r = colors[i % 4][0],
            .g = colors[i % 4][1],
            .b = colors[i % 4][2],
        };
        //copy cmd into queue; wait forever if full
        int ret = k_msgq_put(&cmd_q, &cmd, K_FOREVER);
        if(ret!=0){
            printk("put failed:%d\n", ret);
        }
        printk("queue used: %u\n", k_msgq_num_used_get(&cmd_q));
        i++;
        k_msleep(1000);
    }
}

void consumer(void *p1, void *p2, void *p3){
    struct bulb_cmd cmd;
    while(1){
        //sleep until message arrive then copy into cmd
        k_msgq_get(&cmd_q,&cmd, K_FOREVER);

        printk("Bulb %u -> R=%u G=%u B=%u\n",
               cmd.bulb_id, cmd.r, cmd.g, cmd.b);
               
    }
    
}

/*create both threads at compile time, 1KB stack each
 producer priority 5, consumer 7 (lower no = higher priority)*/
K_THREAD_DEFINE(prod_id, 1024, producer, NULL, NULL, NULL, 5,0,0);
K_THREAD_DEFINE(cons_id, 1024, consumer, NULL, NULL, NULL, 7, 0, 0);


int main(void)
{
    printk("Light hub starting\n");
    return 0;  /* main ends, but the two threads keep running */
}