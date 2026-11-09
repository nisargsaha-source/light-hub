#define DT_DRV_COMPAT lighthub_pca9685_led
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include "pca9685_led.h"
#include <zephyr/kernel.h>
#define PCA9685_REG_MODE1   0x00
#define PCA9685_MODE1_AI    0x20   /* auto-increment on, sleep off */

LOG_MODULE_REGISTER(pca9685_led, LOG_LEVEL_INF);


struct pca9685_config{
    struct i2c_dt_spec i2c;
};

static int pca9685_init(const struct device *dev){
    const struct pca9685_config *cfg = dev->config;
    //is bus ready
    if(!i2c_is_ready_dt(&cfg->i2c)){
        LOG_ERR("I2C bus not ready");
        return -ENODEV;
        
    }
    int ret = i2c_reg_write_byte_dt(&cfg->i2c, PCA9685_REG_MODE1, PCA9685_MODE1_AI);
    if(ret){
    LOG_ERR("MODE 1 WRITE FAILED, RETURNED NON 0 VALUE %d", ret);
    return ret;
    }
    k_busy_wait(500); //chip takes 500 microsec to start. Use k_busy_wait
                    // here because init functions run during boot, before normal thread scheduling, where sleeping isn’t appropriate.
    LOG_INF("PCA9685 at 0x%02x ready", cfg->i2c.addr);
    return 0;
}


/*I2C_DT_SPEC_INST_GET(inst) reads the bus and address from devicetree node number inst
 (the one found through your DT_DRV_COMPAT). For your overlay, that’s bus i2c0, address 0x40. 
 This is the line you tried to write earlier, and this is where it lives.
 DEVICE_DT_INST_DEFINE(...) creates the device itself. Arguments in order:

Argument	Your value	        Meaning
inst	    inst	            Which devicetree node
init_fn	   pca9685_init	        Your init function
pm	       NULL	                Power management (not used)
data	   NULL	                Runtime state struct (not needed yet)
config	   &pca9685_cfg_##inst	The config struct just above
level	   POST_KERNEL	         Initialize after the kernel is running
prio	    90	                 Run after the I2C controller (which initializes earlier)
api      	NULL	             Function table 

DT_INST_FOREACH_STATUS_OKAY(PCA9685_DEFINE) is what actually uses the template.
 It looks for every devicetree node with your compatible and status = "okay" (nodes are okay by default), 
 and expands PCA9685_DEFINE(0), PCA9685_DEFINE(1), and so on, one per node. With your overlay, there’s one 
 node, so you get exactly one config and one device.*/
/*#define PCA9685(inst)                                                                                 \
    static const struct pca9685_config pca9685_cf_##inst = {                                          \
        .i2c = I2C_DT_SPEC_INST_GET(inst),                                                            \
    };                                                                                                \
    DEVICE_DT_INST_DEFINE(inst, pca9685_init, NULL, NULL,                                             \
        &pca9685_cfg_##inst, POST_KERNEL,90,NULL);  
DT_INST_FOREACH_STATUS_OKAY(PCA_9685_DEFINE)*/

static const struct pca9685_config pca9685_cfg_0 = {
    .i2c = I2C_DT_SPEC_INST_GET(0),
};

DEVICE_DT_INST_DEFINE(0, pca9685_init, NULL, NULL,
                      &pca9685_cfg_0, POST_KERNEL, 90, NULL);