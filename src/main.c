#include <stdio.h>
#include <zephyr/kernel.h>

int main(void)
{
	printk("Hello World! %s\n", CONFIG_BOARD_TARGET);
    int count =0;
    while(1){
        count++;
        printk("Hello World! %d\n", count);
        k_msleep(1);
    }

	return 0;
}
