/*
 * @Description  : 
 * @Author       : zhouxc
 * @Date         : 2024-10-23 18:22:59
 * @LastEditors  : zhouxc
 * @LastEditTime : 2024-11-28 10:42:53
 * @FilePath     : /et70-ca3/Products/example/mars_template/mars_devfunc/cook_assistant/auxiliary_cook/aux_api.c
 */
#include <stdio.h>
#include <string.h>
#include "../cloud.h"
#include "auxiliary_cook.h"

extern int(*multi_valve_cb)(enum INPUT_DIR input_dir, int gear);
extern int(*aux_exit_cb)(enum aux_exit_type);
extern int(*beep_control_cb)(int beep_type);

void set_aux_ignition_switch(unsigned char ignition_switch, enum INPUT_DIR input_dir)
{
    aux_handle_t *aux_handle = get_aux_handle(input_dir);
    
    //状态未发生变化，直接返回
    if(aux_handle->ignition_switch == ignition_switch)
    {
        return;
    }

    aux_handle->ignition_switch = ignition_switch; 
    if(aux_handle->ignition_switch)    //开火
    {

    }
    else //关火
    {
        LOGI("aux", "右灶关闭 ===> 请求退出辅助烹饪");
        multi_valve_cb(INPUT_RIGHT, 0);
        aux_exit_cb(AUX_SUCCESS_EXIT);
        cook_aux_reinit(input_dir);


        // //煮模式会自动关火并退出，如果是手动关火属于异常退出
        // if(aux_handle->aux_type == MODE_ZHU)
        // {
                        
        //     if(aux_exit_cb != NULL)
        //     {
        //         LOGI("aux", "右灶关闭,退出辅助烹饪!!!");
        //         multi_valve_cb(INPUT_RIGHT, 0);
        //         aux_exit_cb(AUX_ERROR_EXIT);
        //     }
        // }
        // else if(aux_handle->aux_type == MODE_CHAO || aux_handle->aux_type == MODE_JIAN || aux_handle->aux_type == MODE_ZHA)
        // {
        //     //无论什么模式，关火都会退出辅助烹饪的逻辑
        //     if(multi_valve_cb != NULL)
        //     {
        //         //恢复最大档位
        //         LOGI("aux", "右灶关闭,退出辅助烹饪!!!");
        //         multi_valve_cb(INPUT_RIGHT, 0);
        //         aux_exit_cb(AUX_SUCCESS_EXIT);
        //     }
        // }
        
        // //需要上方先退出模式再重置模式
        // cook_aux_reinit(input_dir);
    }
    
}

void exit_aux_func(enum INPUT_DIR input_dir)
{
    aux_handle_t *aux_handle = get_aux_handle(input_dir);
    if(aux_handle->aux_switch != 0)
    {
        LOGI("aux", "手自一体未处于最大档 ===> 请求退出辅助烹饪");
        aux_exit_cb(AUX_SUCCESS_EXIT);
        multi_valve_cb(INPUT_RIGHT, 0);        
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
