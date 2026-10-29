#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

struct bulb_cmd{
    uint8_t bulb_id;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

/*Queue holding up to 8 bulb_cmd structs, buffer alighned to 4 bytes*/
K_MSGQ_DEFINE(cmd_q, sizeof(struct bulb_cmd), 8, 4);
/* Byte queue: 1-byte messages, 32 slots, alignment 1 */
K_MSGQ_DEFINE(rx_q, 1, 32, 1);

/* Bytes dropped because rx_q was full. Written in the ISR, read by threads. */
static volatile uint32_t rx_dropped;

static int parse_bulb_cmd(const char *line, struct bulb_cmd *out);

void consumer(void *p1, void *p2, void *p3){
    struct bulb_cmd cmd;
    while(1){
        //sleep until message arrive then copy into cmd
        k_msgq_get(&cmd_q,&cmd, K_FOREVER);

        printk("Bulb %u -> R=%u G=%u B=%u\n",
               cmd.bulb_id, cmd.r, cmd.g, cmd.b);
               
    }
    
}

#define LINE_MAX 64

/* RX thread: turns the byte stream from the ISR into complete lines */
void rx_thread(void *p1, void *p2, void *p3)
{
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
                printk("Line too long, discarded\n");
            } else if (len > 0) {
                line[len] = '\0';           /* terminate the C string */
                printk("Got line: %s\n", line);
                /* Stretch goal: parse line into a bulb_cmd here */
                struct bulb_cmd cmd;
                if (parse_bulb_cmd(line, &cmd) == 0) {
                     k_msgq_put(&cmd_q, &cmd, K_FOREVER);
                } else {
                    printk("Invalid command. Format: <bulb> <r> <g> <b>, each 0-255\n");
                }
        }

        /* Empty lines (len == 0) are ignored, e.g. the \n after a \r */

        len = 0;
        overflow = false;
        continue;
        }


        /* Leave room for the '\0' terminator */
        if (len < LINE_MAX - 1) {
            line[len++] = byte;
        } else {
            overflow = true;   /* keep discarding until Enter */
        }
    }
}


void uart_receive(const struct device *dev, void *user_data){
    uint8_t byte;
    /* Call first to refresh the interrupt status */
    if (!uart_irq_update(dev)) {
        return;
    }

    if(uart_irq_rx_ready(dev)){
        while( uart_fifo_read(dev, &byte, 1)){
        if (k_msgq_put(&rx_q, &byte, K_NO_WAIT) != 0) {
            rx_dropped++;   /* queue full: count it, don't print in the ISR */
        }
        }       
    }

}

static int parse_bulb_cmd(const char *line, struct bulb_cmd *out){
    uint32_t vals[4];
    const char *p = line;
    char *end;

    for (int i=0; i<4; i++){
        vals[i]= strtoul(p,&end,10);
        if(end == p){
            return -EINVAL; /*no number found*/
        }
        if (vals[i] > 255){
            return -EINVAL; /*wont fit in uint8_t*/
        }
        p=end; /*continue after this number*/
    }

    /*only allow trailing spaces*/
    while (*p == ' '){
        p++;
    }
    if(*p != '\0'){
        return -EINVAL;
    }

    out->bulb_id = (uint8_t)vals[0];
    out->r = (uint8_t)vals[1];
    out->g = (uint8_t)vals[2];
    out->b = (uint8_t)vals[3];
    return 0;

}

/*create both threads at compile time, 1KB stack each
 producer priority 5, consumer 7 (lower no = higher priority)*/
K_THREAD_DEFINE(cons_id, 1024, consumer, NULL, NULL, NULL, 7, 0, 0);
K_THREAD_DEFINE(rx_id, 1024, rx_thread, NULL, NULL, NULL, 5,0,0);


int main(void)
{
    printk("Light hub starting\n");
    //DEVICE_DT_GET(node_id): gets a pointer to a device’s driver instance from its devicetree node. Returns const struct device *
    static const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));//DT_CHOSEN(zephyr_console) means “the node devicetree has chosen as the console.
    if( !device_is_ready(uart)){
        printk("oops uart no work");
        return 0;
    }
    uart_irq_callback_user_data_set(uart, uart_receive, NULL);
    uart_irq_rx_enable(uart);



    return 0;  /* main ends, but the two threads keep running */
}