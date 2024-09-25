/*
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-08-16 10:00:09
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-09-16 17:45:05
 * @FilePath     : /alios-things/Products/example/mars_template/mars_driver/mars_i2c.c
 */

#include <stdio.h>
#include <string.h>
#include <hal/soc/i2c.h>

#define I2C_BUF_SIZE   32
char app_data_buf[I2C_BUF_SIZE] = {0};

i2c_dev_t app_i2c = {
            .config={
                .address_width = I2C_HAL_ADDRESS_WIDTH_7BIT,
                .freq = 78000,
                .mode = I2C_MODE_MASTER,
            },
            .port = 0,
};

// void app_i2c_gpio_init(uint8_t sda, uint8_t scl)
// {
//     uint8_t gpiopins[2];

//     gpiopins[0] = sda;
//     gpiopins[1] = scl;
    
//     GLB_GPIO_Func_Init(GPIO_FUN_I2C, gpiopins, sizeof(gpiopins) / sizeof(gpiopins[0]));
//     return;
// }

void mars_i2c_init(void)
{
//暂时需要修改i2c_gpio_init()函数中的GPIO配置
// Living_SDK/platform/mcu/tg7100c/hal_drv/tg7100c_hal/bl_i2c.c

// void i2c_gpio_init(int i2cx)
// {
//     uint8_t gpiopins[2];
//     if (i2cx == I2C0) {
//         gpiopins[0] = GLB_GPIO_PIN_2;
//         gpiopins[1] = GLB_GPIO_PIN_1;
//     } else {
//     }
    
//     GLB_GPIO_Func_Init(GPIO_FUN_I2C, gpiopins, sizeof(gpiopins) / sizeof(gpiopins[0]));
//     return;
// }
    
    int ret     = -1;
    ret = hal_i2c_init(&app_i2c);
    if (ret != 0) {
        printf("i2c0 init error !\n");
        hal_i2c_finalize(&app_i2c);
    }
    // uint8_t i2c_sda = 1;
    // uint8_t i2c_scl = 2;
    // app_i2c_gpio_init(i2c_sda, i2c_scl);
    // i2c_set_freq(i2c->config.freq, 0);
}

void mars_i2c_master_send(uint16_t dev_addr, uint8_t *buf, uint16_t len, uint16_t timeout)
{
    int ret = -1;

    ret = hal_i2c_master_send(&app_i2c, dev_addr, buf, len, timeout);
}

void mars_i2c_master_recv(uint16_t dev_addr, uint8_t *buf, uint16_t len, uint16_t timeout)
{
    int ret = -1;

    ret = hal_i2c_master_recv(&app_i2c, dev_addr, buf, len, timeout);
}

void mars_i2c_reg_write(uint16_t dev_addr, uint8_t reg_addr, uint8_t *buf, uint16_t len, uint32_t timeout)
{
    int ret = -1;

    int data_len = 0;
    app_data_buf[data_len++] = reg_addr;
    memcpy(app_data_buf+1, buf, len);
    data_len += len;
        
    ret = hal_i2c_master_send(&app_i2c, dev_addr, app_data_buf, data_len, timeout);
}

int32_t mars_i2c_reg_read(uint16_t dev_addr, uint8_t reg_addr, uint8_t *buf, uint16_t len, uint32_t timeout)
{
    int ret = -1;

    int data_len = 0;
    app_data_buf[data_len++] = reg_addr;
    ret = hal_i2c_master_send(&app_i2c, dev_addr, app_data_buf, data_len, timeout);

    memset(app_data_buf, 0, sizeof(app_data_buf));
    ret = hal_i2c_master_recv(&app_i2c, dev_addr, buf, len, timeout);

    return ret;
}
