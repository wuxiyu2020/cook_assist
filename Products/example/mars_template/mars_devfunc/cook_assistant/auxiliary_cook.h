/*** 
 * @Description  : 
 * @Author       : zhouxc
 * @Date         : 2024-10-22 13:40:14
 * @LastEditors  : zhouxc
 * @LastEditTime : 2024-10-25 16:57:00
 * @FilePath     : /et70-ca3/Products/example/mars_template/mars_devfunc/cook_assistant/auxiliary_cook.h
 */
#ifndef _AUXILIARY_COOK_H
#define _AUXILIARY_COOK_H

#include "cook_wrapper.h"
#define ARRAY_DATA_SIZE 10
#define MAX_FIRE_TIME 4 * 20    //最长开火时间

typedef enum
{
    IDLE,
    GENTLE,
    RISE,
    DOWN,
    BOILED
} boil_tendency_t;

enum aux_exit_type
{
    AUX_NOT_RUN,
    AUX_SUCCESS_EXIT,
    AUX_ERROR_EXIT
};

typedef struct
{
    unsigned char ignition_switch;                  //点火开关
    unsigned char aux_switch;                       //辅助烹饪开关
    unsigned char aux_type;                         //辅助烹饪模式
    unsigned int aux_set_time;                      //辅助烹饪煮模式设置时间
    unsigned int aux_set_temp;                      //辅助烹饪炸模式设置温度
    unsigned int aux_remain_time;                   //辅助烹饪煮模式剩余时间
    unsigned char aux_boil_counttime_flag;          //倒计时标志位 0：不进行倒计时 1：进行倒计时

    unsigned int aux_total_tick;                    //进入辅助烹饪的总时间
    unsigned int current_average;                   //当前的平均温度

    unsigned char enter_boil_time;                  //进入水开的次数，初始为0
    boil_tendency_t boil_current_tendency;          //煮模式当前状态趋势
    unsigned int boil_current_status_tick;          //煮模式当前趋势计时

    boil_tendency_t boil_next_tendency;             //煮模式待切换的下一个状态
    unsigned int boil_next_status_tick;             //煮模式待切换趋势计时

    unsigned short now_temp;                        //当前温度    
    unsigned short temp_array[ARRAY_DATA_SIZE];     //温度数组
    unsigned char temp_size;                        //温度数组中的数据量

    unsigned short average_temp_array[ARRAY_DATA_SIZE]; //存储平均温度的数组
    unsigned char average_temp_size;                    //平均温度数组中的温度数量

    // unsigned char boil_mode_rise_step;              //煮模式加热阶段标志位
    // unsigned int boil_mode_tick;                    //煮模式加热阶段时间计时

    // unsigned char boil_mode_gentle_step;            //煮模式平缓阶段标志位
    // unsigned int boil_mode_gentle_tick;             //煮模式平缓阶段时间

    // unsigned char boil_mode_boil_step;              //煮模式水开标志位


} aux_handle_t;

// typedef enum AUXCOOKMODE
// {
//     MODE_CHAO = 1,          //炒模式
//     MODE_ZHU = 2,           //煮模式
//     MODE_JIAN = 3,          //煎模式
//     MODE_ZHA = 4            //炸模式
// } aux_cook_mode;


aux_handle_t *get_aux_handle(enum INPUT_DIR input_dir);
void register_beep_cb(int(*cb)(int beep_type));
void register_multivalve_cb(int(*cb)(enum INPUT_DIR input_dir, int gear));
void register_auxclose_fire_cb(int (*cb)(enum INPUT_DIR input_dir));
void register_auxcount_down(int (*cb)(unsigned char remaintime));
void register_aux_exit_cb(int (*aux_exit_cb)(enum aux_exit_type));
 

#endif