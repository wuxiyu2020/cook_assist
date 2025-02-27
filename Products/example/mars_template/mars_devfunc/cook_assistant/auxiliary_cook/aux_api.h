/*** 
 * @Description  : 
 * @Author       : zhouxc
 * @Date         : 2024-10-23 18:23:08
 * @LastEditors  : zhouxc
 * @LastEditTime : 2024-10-30 14:29:51
 * @FilePath     : /et70-ca3/Products/example/mars_template/mars_devfunc/cook_assistant/aux_api.h
 */
#ifndef AUX_API_H_
#define AUX_API_H_

#include "auxiliary_cook.h"

void set_aux_ignition_switch(unsigned char ignition_switch, enum INPUT_DIR input_dir);
void set_multivalve_gear(unsigned char gear, enum INPUT_DIR input_dir);
aux_handle_t *get_aux_handle(enum INPUT_DIR input_dir);

#endif
