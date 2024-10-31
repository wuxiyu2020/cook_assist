/*
 * @Description  : 
 * @Author       : zhouxc
 * @Date         : 2024-10-23 18:22:59
 * @LastEditors  : zhouxc
 * @LastEditTime : 2024-10-31 11:43:25
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
    
    //开火
    if(aux_handle->ignition_switch)
    {

    }
    else        //关火
    {
        //煮模式会自动关火并退出，如果是手动关火属于异常退出
        if(aux_handle->aux_type == MODE_ZHU)
        {
            extern int(*aux_exit_cb)(enum aux_exit_type);
            
            if(aux_exit_cb != NULL)
            {
                printf("close fire,exit aux mode\r\n");
                aux_exit_cb(AUX_ERROR_EXIT);
            }
        }

        //无论什么模式，关火都会退出辅助烹饪的逻辑
        extern int(*multi_valve_cb)(enum INPUT_DIR input_dir, int gear);
        if(multi_valve_cb != NULL)
        {
            //恢复最大档位
            printf("close fire,set mutivalve:0\r\n");
            multi_valve_cb(INPUT_RIGHT,0);
        }
        
        //需要上方先退出模式再重置模式
        cook_aux_reinit(input_dir);
    }
    
}


/**
 * @brief: 串口交互端代码通知辅助烹饪当前八段阀的档位
 * @param {unsigned char} gear
 * @param {enum INPUT_DIR} input_dir
 * @return {*}
 */
void set_multivalve_gear(unsigned char gear, enum INPUT_DIR input_dir)
{
    if(gear < 0 || gear > 7)
    {
        printf("ERROR!MultiValvenot support this gear\r\n");
        return;
    }

    aux_handle_t *aux_handle = get_aux_handle(input_dir);
    aux_handle->aux_multivalve_gear = gear;

}
