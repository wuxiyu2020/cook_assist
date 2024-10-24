/*
 * @Description  : 
 * @Author       : zhouxc
 * @Date         : 2024-10-23 18:22:59
 * @LastEditors  : zhouxc
 * @LastEditTime : 2024-10-23 18:52:51
 * @FilePath     : /et70-ca3/Products/example/mars_template/mars_devfunc/cook_assistant/aux_api.c
 */
#include <stdio.h>
#include <string.h>
#include "cloud.h"
#include "auxiliary_cook.h"


void set_aux_ignition_switch(unsigned char ignition_switch, enum INPUT_DIR input_dir)
{
    aux_handle_t *aux_handle = get_aux_handle(input_dir);
    
    //状态未发生变化，直接返回
    if(aux_handle->ignition_switch == ignition_switch)
    {
        return;
    }

    aux_handle->ignition_switch = ignition_switch;
    printf("aux ignition switch change,now is:%d\r\n",aux_handle->ignition_switch);
    if(aux_handle->ignition_switch)
    {

    }
    else        //关火
    {
        cook_aux_reinit(input_dir);
    }
    
}
