/*** 
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-08-16 10:00:21
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-10-29 16:17:21
 * @FilePath     : /alios-things/Products/example/mars_template/mars_driver/mars_i2c.h
 */

#include <stdint.h>

/*** 
 * @brief 低成本烹饪助手使用
 * @return {*}
 */
void mars_i2c_init(void);

void mars_i2c_master_send(uint16_t dev_addr, uint8_t *buf, uint16_t len, uint16_t timeout);

void mars_i2c_reg_write(uint16_t dev_addr, uint8_t reg_addr, uint8_t *buf, uint16_t len, uint32_t timeout);

int32_t mars_i2c_reg_read(uint16_t dev_addr, uint8_t reg_addr, uint8_t *buf, uint16_t len, uint32_t timeout);