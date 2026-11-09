#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/*DT_NODELABEL(leds) finds the devicetree node with the label leds: from your overlay.
DEVICE_DT_GET(...) returns the device Zephyr created for that node, the one your DEVICE_DT_INST_DEFINE made.*/
const struct device *leds = DEVICE_DT_GET(DT_NODELABEL(home_leds));

int main(void){
if(device_is_ready(leds)){
    LOG_INF("DEVICE READY");
    return 0;
}else {
    LOG_WRN("DEVICE NOT READY");
    return 0;
}
}