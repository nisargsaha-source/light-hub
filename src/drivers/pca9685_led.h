#pragma once
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>


#define PCA9685_REG_MODE1   0x00
#define PCA9685_MODE1_AI    0x20 