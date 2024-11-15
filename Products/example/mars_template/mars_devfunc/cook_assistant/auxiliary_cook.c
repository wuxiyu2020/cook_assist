/*
 * @Description  : 
 * @Author       : zhouxc
 * @Date         : 2024-10-21 10:37:25
 * @LastEditors  : zhouxc
 * @LastEditTime : 2024-11-15 15:51:33
 * @FilePath     : /et70-ca3/Products/example/mars_template/mars_devfunc/cook_assistant/auxiliary_cook.c
 */

#include <stdio.h>
#include <stdlib.h>
#ifndef MODULE_TEST
    #include "cloud.h"
    #include "fsyd.h"
#endif
#include "auxiliary_cook.h"
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)
#define AUX_DATA_HZ 4
#define START 2
#define STEP 3

//煮模式加热水沸保底时间10min
#define BOIL_MODE_MAX_HEAT_TIME 10 * 4 * 60

aux_handle_t g_aux_state[2];
char *boil_status_info[]={"idle","gentle","rise","down","boiled"};
char *aux_mode[] = {"chao","zhu","jian","zha"};

int(*beep_control_cb)(int beep_type); 
void register_beep_cb(int(*cb)(int beep_type))
{
    beep_control_cb = cb;
}

int(*multi_valve_cb)(enum INPUT_DIR input_dir, int gear);
void register_multivalve_cb(int(*cb)(enum INPUT_DIR input_dir, int gear))
{
    multi_valve_cb = cb;
}

#define VALVE_CHANGE_DELAY 4 * 5;
/**
 * @brief: 设置八段阀档位函数
 * @param {int} gear 目标档位
 * @param {aux_handle_t} *aux_handle
 * @return {*} 0：成功 -1：失败
 */
int change_multivalve_gear(unsigned char gear, enum INPUT_DIR input_dir)
{
    LOG_default("enter:%s\r\n",__func__);

    aux_handle_t *aux_handle = get_aux_handle(input_dir);
    if(gear > 7) 
    {
        LOG_default("小于最小档位，设置为最小档位");
        gear = 0x07;
    }
    else if(gear < 0)
    {
       LOG_default("超出最大档位，设置为最大档位");
        gear = 0x00;
    }

    //如果设置档位和当前档位一致，直接返回
    if(aux_handle->aux_multivalve_gear == gear)
    {
        LOG_default("aim gear is now gear,not response\r\n");
        return -1;
    }

    //判断尚未到达目标档位，不进行档位调整
    if(aux_handle->aux_multivalve_gear != aux_handle->aux_aim_multivalve_gear)
    {   
        LOG_default("multivalve is moving,this change not reponse\r\n");
        return -1;
    }

    //煮模式最后两分钟,只接受1档设置
    if(aux_handle->aux_type == MODE_ZHU && aux_handle->aux_remain_time <= 2 * 60 * INPUT_DATA_HZ && gear != 0x01)
    {
        return -1;
    }

    if(multi_valve_cb != NULL)
    {
        //记录一次目标档位
        aux_handle->aux_aim_multivalve_gear = gear;
        multi_valve_cb(INPUT_RIGHT,gear);
        LOG_RED("设置目标档位为:%d",gear);
        return 0;
    }
    return -1;
    
}

int (*aux_close_fire_cb)(enum INPUT_DIR input_dir);
void register_auxclose_fire_cb(int (*cb)(enum INPUT_DIR input_dir))
{
    aux_close_fire_cb = cb;
}

static int (*aux_remaintime_cb)(unsigned char remaintime);
void register_auxcount_down(int (*cb)(unsigned char remaintime))
{
    aux_remaintime_cb = cb;
}

int(*aux_exit_cb)(enum aux_exit_type);
void register_aux_exit_cb(int (*cb)(enum aux_exit_type))
{
    aux_exit_cb = cb;
}

/**
 * @brief: 获取辅助烹饪参数结构体的函数
 * @param {enum INPUT_DIR} input_dir
 * @return {*}
 */
aux_handle_t *get_aux_handle(enum INPUT_DIR input_dir)
{
    aux_handle_t *aux_handle = NULL;
    if(input_dir == INPUT_LEFT)
    {
        aux_handle = &g_aux_state[INPUT_LEFT];
    }
    else if(input_dir == INPUT_RIGHT)
    {
        aux_handle = &g_aux_state[INPUT_RIGHT];
    }
    return aux_handle; 
}

/**
 * @brief: 设置辅助烹饪模式的函数入口
 * 这里不做参数检测，在输入前默认参数是正确的，例如某些模式应该有时间参数，某些不应该有时间参数
 * @param {int} aux_switch
 * @param {int} cook_type
 * @param {int} set_time
 * @param {int} set_temp            
 * @return {*}
 */
void auxiliary_cooking_switch(int aux_switch, int cook_type, int set_time, int set_temp)
{
    aux_handle_t *aux_handle = get_aux_handle(INPUT_RIGHT);
    aux_handle->aux_switch = aux_switch;
    aux_handle->aux_type = cook_type;
    aux_handle->aux_set_time = set_time;
    aux_handle->aux_set_temp = set_temp;
    aux_handle->aux_remain_time = set_time * 60 * AUX_DATA_HZ;
    LOG_default("aux seting:==============================\r\n\
    aux_switch:%d,aux_mode:%s aux_set_time:%d,aux_set_temp:%d\r\n",\
    aux_handle->aux_switch, aux_mode[aux_handle->aux_type - 1], aux_handle->aux_set_time, aux_handle->aux_set_temp);

    if(aux_handle->aux_type == MODE_ZHU)
    {
        if(aux_handle->aux_set_time > 0 && aux_handle->aux_set_time < 8)
        {
            aux_handle->aux_boil_type = BOIL_0_8;
        }
        else if(aux_handle->aux_set_time >= 8 && aux_handle->aux_set_time < 20)
        {
            aux_handle->aux_boil_type = BOIL_8_20;
        }
        else if(aux_handle->aux_set_time >= 20)
        {
            aux_handle->aux_boil_type = BOIL_20_MAX;
        }
    }
    return;
}


void cook_aux_reinit(enum INPUT_DIR input_dir)
{
    aux_handle_t *aux_handle = get_aux_handle(input_dir);
    //初始化辅助烹饪设置参数
    aux_handle->aux_switch = 0;
    aux_handle->aux_type = 0;
    aux_handle->aux_set_time = 0;
    aux_handle->aux_set_temp = 0;

    //初始化八段阀档位变量
    aux_handle->aux_multivalve_gear = 0;
    aux_handle->aux_aim_multivalve_gear = 0;

    //煮模式变量重置
    aux_handle->aux_total_tick = 0;
    aux_handle->boil_current_tendency = 0;
    aux_handle->boil_current_status_tick = 0;
    aux_handle->boil_next_tendency = 0;
    aux_handle->boil_next_status_tick = 0;
    aux_handle->enter_boil_time = 0;
    aux_handle->aux_boil_counttime_flag = 0;
    aux_handle->aux_boil_type = 0;

    //炸模式变量重置
    aux_handle->fry_step = 0;
    aux_handle->aux_aim_multivalve_gear = 0;
    aux_handle->fry_last_gear_average_temp = 0;
    aux_handle->rise_quick_tick = 0;
    aux_handle->rise_slow_tick = 0;
    aux_handle->first_reach_set_temp_flag = 0;
    aux_handle->fry_last_change_gear_tick = 0;
}

/**
 * @brief: 初始化辅助烹饪相关参数
 * @param {enum INPUT_DIR} input_dir
 * @return {*}
 */
void cook_aux_init(enum INPUT_DIR input_dir)
{
    cook_aux_reinit(input_dir);
    return;
}

/**
 * @brief: 保存辅助烹饪温度数组的函数
 * @param {aux_handle_t} *aux_handle
 * @param {unsigned short} temp
 * @return {*}
 */
void aux_temp_save(aux_handle_t *aux_handle,unsigned short temp)
{    
    //首次未存满的时候，按顺序存入
    if(aux_handle->temp_size < ARRAY_DATA_SIZE)
    {
        aux_handle->temp_array[aux_handle->temp_size] = temp;
        aux_handle->temp_size++;
    }
    //存满之后，全部向前移动一位，存入最后一位
    else if(aux_handle->temp_size == ARRAY_DATA_SIZE)
    {
        for(int i = 0; i < ARRAY_DATA_SIZE - 1; i++)
        {
            aux_handle->temp_array[i] = aux_handle->temp_array[i+1];
        }
        aux_handle->temp_array[ARRAY_DATA_SIZE - 1] = temp;
    }
    else if(aux_handle->temp_size > ARRAY_DATA_SIZE)
    {
        LOG_default("aux deal temp data error!!!\r\n");
    }

    //打印单个温度数组
    LOG_default("aux temp array is:");
    unsigned int average_temp = 0;
    for(int i = 0; i < aux_handle->temp_size; i++)
    {
        LOG_default("%d ",aux_handle->temp_array[i]);
        average_temp += aux_handle->temp_array[i];
    }
    LOG_default("\r\n");
    average_temp /= aux_handle->temp_size;
    LOG_default("present average temp is:%d\r\n",average_temp);
    aux_handle->current_average_temp = average_temp;

    //每10个数据计算法一个平均值，单个数据时间间隔0.25s，即2.5s取一个平均值
    if(aux_handle->aux_total_tick % ARRAY_DATA_SIZE == 0)
    {
        if(aux_handle->average_temp_size < ARRAY_DATA_SIZE)
        {
            aux_handle->average_temp_array[aux_handle->average_temp_size] = average_temp;
            aux_handle->average_temp_size++;
        }
        else if(aux_handle->average_temp_size == ARRAY_DATA_SIZE)
        {
            for(int i = 0; i < ARRAY_DATA_SIZE - 1; i++)
            {
                aux_handle->average_temp_array[i] = aux_handle->average_temp_array[i+1];
            }
            aux_handle->average_temp_array[ARRAY_DATA_SIZE - 1] = average_temp;
        }
        else if(aux_handle->average_temp_size > ARRAY_DATA_SIZE)
        {
            LOG_default("aux deal average temp data error!!!\r\n");
        }
    }

    //打印平均温度数组
    LOG_default("=============================================\r\naux average temp array is:");
    for(int i = 0; i < aux_handle->average_temp_size; i++)
    {
        LOG_default("%d ",aux_handle->average_temp_array[i]);
    }
    LOG_default("\r\n=============================================\r\n");

}


/**
 * @brief:判断水开的函数 
 * @param {aux_handle_t} *aux_handle
 * @return {*} 0:水未开，1：水已开
 */
int judge_water_boil(aux_handle_t *aux_handle)
{
    LOG_default("enter boil judge\r\n");
    //煮模式中的保底时间到，尚未发生过水开场景，保底认为水开
    if(aux_handle->aux_total_tick >= BOIL_MODE_MAX_HEAT_TIME && aux_handle->aux_type == MODE_ZHU &&\
     aux_handle->enter_boil_time < 1)
    {
        LOG_default("煮模式保底时间到认为已经是水开\r\n");
        return 1;
    }

    if(aux_handle->current_average_temp > 850 && aux_handle->current_average_temp < 1100 && aux_handle->average_temp_size == ARRAY_DATA_SIZE)
    {
        unsigned char i = 0;
        unsigned char rise_count1 = 0;
        unsigned char rise_count2 = 0;
        unsigned int average_temp_average = 0;          //平均温度数组的平均温度
        for(i = 0; i < aux_handle->average_temp_size - 1; i++)
        {
            if(aux_handle->average_temp_array[i] < aux_handle->average_temp_array[i+1])
            {
                rise_count1++;
            }
            average_temp_average += aux_handle->average_temp_array[i];
        }
        //最后一个温度不要遗漏
        average_temp_average += aux_handle->average_temp_array[i];
        average_temp_average /= (i+1);

        rise_count2 += (aux_handle->average_temp_array[0] < aux_handle->average_temp_array[3]);
        rise_count2 += (aux_handle->average_temp_array[3] < aux_handle->average_temp_array[6]);
        rise_count2 += (aux_handle->average_temp_array[6] < aux_handle->average_temp_array[9]);

        if(rise_count1 >= 7 || rise_count2 >= 3)
        {
            LOG_default("认为仍然处于上升状态\r\n");
            return 0;
        }

        unsigned short temp_stable_threshold = 20;          //温度的波动温度范围
        unsigned char stable_count = 0;                     //稳定计数
        // 统计稳定温度的次数
        for (i = 0; i < aux_handle->average_temp_size; i++) 
        {
            if (abs(aux_handle->average_temp_array[i] - average_temp_average) <= temp_stable_threshold) {
                stable_count++;
            }
        }
        if(stable_count >= 9)
        {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief: 煮模式中的状态切换函数
 * @param {aux_handle_t} *aux_handle
 * @return {*}
 */
void boil_status_change(aux_handle_t *aux_handle)
{
    //状态切换判断
    //初始为空闲，则直接接收传入的新的状态
    if(aux_handle->boil_current_tendency == IDLE && aux_handle->boil_next_tendency != IDLE)
    {
        aux_handle->boil_current_tendency = aux_handle->boil_next_tendency;
        aux_handle->boil_current_status_tick = 0;
        aux_handle->boil_next_status_tick = 0;
    }
    else if(aux_handle->boil_current_tendency != IDLE && aux_handle->boil_next_tendency == IDLE)
    {
        if(aux_handle->boil_next_status_tick > 4 * 5)
        {
            LOG_default("[change_status],to idle\r\n");
            aux_handle->boil_current_tendency = aux_handle->boil_next_tendency;
            aux_handle->boil_current_status_tick = 0;
            aux_handle->boil_next_status_tick = 0;
        }
    }
    else if(aux_handle->boil_current_tendency == RISE && aux_handle->boil_next_tendency != RISE)
    {
        if(aux_handle->boil_next_status_tick > 4 * 6)
        {
            LOG_default("[change_status],rise to %s\r\n",boil_status_info[aux_handle->boil_next_tendency]);
            aux_handle->boil_current_tendency = aux_handle->boil_next_tendency;
            aux_handle->boil_current_status_tick = 0;
            aux_handle->boil_next_status_tick = 0;
        }
    }
    else if(aux_handle->boil_current_tendency == DOWN && aux_handle->boil_next_tendency != DOWN)
    {
        if(aux_handle->boil_next_status_tick > 4 * 2)
        {
            //第二次水开之前切换到了down都会停止计时，再次沸后计时
            if(aux_handle->enter_boil_time <= 1)
            {
                aux_handle->aux_boil_counttime_flag = 0;    //停止计时
            }
            aux_handle->boil_current_tendency = aux_handle->boil_next_tendency;
            aux_handle->boil_current_status_tick = 0;
            aux_handle->boil_next_status_tick = 0;
        }
    }
    else if(aux_handle->boil_current_tendency == GENTLE && aux_handle->boil_next_tendency != GENTLE)
    {
        if(aux_handle->boil_next_status_tick > 4 * 8)
        {
            aux_handle->boil_current_tendency = aux_handle->boil_next_tendency;
            aux_handle->boil_current_status_tick = 0;
            aux_handle->boil_next_status_tick = 0;
        }
    }
    else if(aux_handle->boil_current_tendency == BOILED && aux_handle->boil_next_tendency != BOILED)
    {
        if(aux_handle->boil_next_tendency == GENTLE)
        {
            LOG_default("要切换的是gentle，仍然保持boil状态");
        }
        //status change: BOIL->DOWN
        else if(aux_handle->boil_next_tendency == DOWN)
        {
            // //已经发生过水开的情况了
            // if(aux_handle->aux_boil_type == BOIL_20_MAX && aux_handle->aux_total_tick >= 10 * 60 * INPUT_DATA_HZ)
            // {
            //     LOG_default("总时间大于20min，处于10Min后，八段阀切换为大火3档\r\n");
            //     change_multivalve_gear(0x03, INPUT_RIGHT);
            // }
            // else
            //{
                LOG_default("水沸状态切换到下降状态，八段阀切换为大火1档\r\n");
                change_multivalve_gear(0x01, INPUT_RIGHT); 
            //}
              
           

            aux_handle->boil_current_tendency = aux_handle->boil_next_tendency;
            aux_handle->boil_current_status_tick = 0;
            aux_handle->boil_next_status_tick = 0;
        }
        else if(aux_handle->boil_next_tendency == RISE)
        {   
            if( aux_handle->boil_next_status_tick > 5 * 10)
            {
                aux_handle->boil_current_tendency = aux_handle->boil_next_tendency;
                aux_handle->boil_current_status_tick = 0;
                aux_handle->boil_next_status_tick = 0;
            }
        }
    }
}


/**
 * @brief: 煮场景
 * @param {aux_handle_t} *aux_handle
 * @return {*}
 */
void mode_boil_func(aux_handle_t *aux_handle)
{
    LOG_default("enter func:%s\r\n",__func__);
    //用于判断此次状态和上次状态之间是否发生了变化
    static int before_next_tendency = IDLE;


    //do...while中状态判断逻辑
    do
    {
        //上升趋势========================================================
        if(aux_handle->temp_array[0] - aux_handle->temp_array[ARRAY_DATA_SIZE - 1] <= 0)
        {
            LOG_default("begin rise tendency judge\r\n");
            unsigned char rise_count1 = 0;
            unsigned char rise_count2 = 0;
            //两两相邻数据之间的9次比较
            for(int i = 0; i < ARRAY_DATA_SIZE - 2; i++)
            {
                if(aux_handle->temp_array[i] <= aux_handle->temp_array[i+1])
                {
                    rise_count1++;
                }
            }
            //两两间隔4个数据之间的5次比较
            for(int i = ARRAY_DATA_SIZE - 1; i > 4; i--)
            {
                if(aux_handle->temp_array[i] > aux_handle->temp_array[i-5])
                {
                    rise_count2++;
                }
            }
            unsigned char rise_count3 = 0;
            if(aux_handle->average_temp_size >= ARRAY_DATA_SIZE)
            {
                //10个最新的平均温度之间两两进行9次比较
                for(int i = ARRAY_DATA_SIZE -1; i > 0; i--)
                {
                    if(aux_handle->average_temp_array[i] - aux_handle->average_temp_array[i-1] >= 1)
                    {
                        rise_count3++;
                    }
                }
            } 
            LOG_default("rise_count1:%d,rise_count2:%d,rise_count3:%d\r\n",rise_count1,rise_count2,rise_count3);
            //rise_count3 = (aux_handle->average_temp_array[aux_handle->average_temp_size - 1] - aux_handle->average_temp_array[aux_handle->average_temp_size - 2] >= 10);
            //rise_count3 += 
            //仅仅靠10个温度难以判断是rise还是gentle
            //if((rise_count1 >= 7 && rise_count2 >= 4) || 
            if(rise_count2 >= 4 || rise_count3 >= 7)
            {
                aux_handle->boil_next_tendency = RISE;
                LOG_default("[%s]present state is rising\r\n",__func__);
                break;
            }
        }
        else
        {
            LOG_default("begin down tendency judge\r\n");
            //下降趋势====================================================
            unsigned char down_count1 = 0;
            for(int i = 0; i < ARRAY_DATA_SIZE - 1; i++)
            {
                if(aux_handle->temp_array[i] - aux_handle->temp_array[i+1] > 50)
                {
                    //这里确定是属于down情况，因此直接break，break需要先跳出for再次跳出while
                    aux_handle->boil_next_tendency = DOWN;
                    break;
                }
            }
            //再次跳出while循环
            if(aux_handle->boil_next_tendency == DOWN)
            {
                //aux_handle->boil_next_status_tick = 4 * 5;     //快速的变化直接赋值下降的时间为5s，以及时切换到下降状态
                LOG_default("[%s]present state is down\r\n",__func__);
                break;
            }

            // short diff0 = aux_handle->temp_array[0] - aux_handle->temp_array[ARRAY_DATA_SIZE - 1];
            // short diff1 = aux_handle->temp_array[START] - aux_handle->temp_array[START + STEP];
            // short diff2 = aux_handle->temp_array[START + STEP] - aux_handle->temp_array[START + STEP * 2];
            // unsigned char slow_down = diff0 > 10 && diff0 < 100;
            // slow_down += diff1 >= 0;
            // slow_down += diff2 >= 0;
            // // slow_down += diff3 >= 0;
            // //满足以上三个条件，第三组数据大于最后一组数据1-10；第三组数据大于第五组数据；第五组数据大于第七组数据
            // if (slow_down >= 3)
            // {
            //     //第三组开始
            //     int i = 0;
            //     for (i = START + 1; i < ARRAY_DATA_SIZE - 1; ++i)
            //     {
            //         if (aux_handle->temp_array[START] < aux_handle->temp_array[i])      
            //             break;
            //         if (aux_handle->temp_array[ARRAY_DATA_SIZE - 1] > aux_handle->temp_array[i])    
            //             break;
            //     }
            //     if (i >= ARRAY_DATA_SIZE - 1)
            //     {
            //         aux_handle->boil_next_tendency = DOWN;
            //         //aux_handle->boil_next_status_tick = 0;     //快速的变化直接赋值下降的时间为5s，以及时切换到下降状态
            //         LOG_default("[%s]present state is down\r\n",__func__);
            //         break;
            //     }
            // }

        }

        //平缓趋势======================================================================
        unsigned char gentle_count1 = 0;
        //int gentle_count2 = 0;
        for(int i = 0; i < ARRAY_DATA_SIZE; i++)
        {
            if(abs(aux_handle->temp_array[i] - aux_handle->current_average_temp) < 13)
            {
                gentle_count1++;
            }
        }
        unsigned int before_average = 0;
        unsigned int after_average = 0;
        for(int i = 0;i < ARRAY_DATA_SIZE/2; i++)
        {
            before_average += aux_handle->temp_array[i];
            after_average += aux_handle->temp_array[i+5];
        }
        before_average /= 5;
        after_average /= 5;

        if( gentle_count1 == 9 || abs(before_average - after_average) < 10)
        {
            aux_handle->boil_next_tendency = GENTLE;
            LOG_default("[%s]present state is gentle\r\n",__func__);
            break;
        }

        aux_handle->boil_next_tendency = IDLE;
        break;
    }while(1);

    //待切换状态变化，则计时清零，开始对新状态进行计时
    if(aux_handle->boil_next_tendency != before_next_tendency)
    {
        LOG_default("next status change");
        before_next_tendency = aux_handle->boil_next_tendency;
        aux_handle->boil_next_status_tick = 0;
    }
    //状态未发生变化，继续对next_status_tick计时
    else
    {
        aux_handle->boil_next_status_tick++;
    }

    boil_status_change(aux_handle);    

    //Gentle中判断水开
    if(aux_handle->boil_current_tendency == GENTLE)
    {
        int ret = judge_water_boil(aux_handle);

        if(ret)
        {
            aux_handle->boil_current_tendency = BOILED;
            LOG_default("切换到水开状态\r\n");
            
            //进入水开次数累加
            aux_handle->enter_boil_time++;

            //前两次进入水沸状态都重新倒计时，这里对当前倒计时进行清零。对应的是冷水放食材直接煮开到结束，或者冷水煮开后再放食材煮沸后到结束
            if(aux_handle->enter_boil_time <= 2)
            {
                aux_handle->aux_remain_time = aux_handle->aux_set_time * 60 * AUX_DATA_HZ;
            }

            aux_handle->aux_boil_counttime_flag = 1;        //开启倒计时

            if(aux_handle->enter_boil_time == 1)
            {
                //首次水开进行蜂鸣
                if(beep_control_cb != NULL)
                {
                    beep_control_cb(0x02);
                }

                //首次水开全部调为3档
                change_multivalve_gear(0x03, INPUT_RIGHT);
            }
            //非首次水开
            else
            {
                //检测到水开，根据不同的设定时间来调整火力档位
                if(aux_handle->aux_boil_type == BOIL_0_8)
                {
                    change_multivalve_gear(0x03, INPUT_RIGHT);
                }
                else if(aux_handle->aux_boil_type == BOIL_8_20)
                {
                    change_multivalve_gear(0x03, INPUT_RIGHT);
                    //同时开启1min计时，1min之后调为7档
                }
                else if(aux_handle->aux_boil_type == BOIL_20_MAX)
                {
                    //前10min水沸后调为3档，10min后水沸后调为6档
                    if(aux_handle->aux_total_tick < 10 * 60 * INPUT_DATA_HZ)
                    {
                        change_multivalve_gear(0x03, INPUT_RIGHT);
                    }
                    else if(aux_handle->aux_total_tick >= 10 * 60 * INPUT_DATA_HZ)
                    {
                        change_multivalve_gear(0x06, INPUT_RIGHT);
                    }   
                    
                }
            }

            
            
            aux_handle->boil_current_status_tick = 0;
            aux_handle->boil_next_status_tick = 0;
        }
    }

    //水开和温度down之外的特殊处理
    //如果煮模式设置时间大于20min,且当前处于10min后，且处于沸腾状态（如果是非沸腾状态需要设置为1档加热至沸腾），档位非6档，切换档位到6档位
    if(aux_handle->aux_boil_type == BOIL_20_MAX && aux_handle->boil_current_tendency == BOILED && \
    aux_handle->aux_total_tick >= 10 * 60 * INPUT_DATA_HZ && aux_handle->aux_multivalve_gear != 0x06)
    {
        change_multivalve_gear(0x06, INPUT_RIGHT);
    }

    //aux暂时设置，enter_boil_time>1之后直接执行aux_handle->aux_remain_time--;即可，不需要再次区分，上方进入水沸的时候会判断是否需要清空
    if(aux_handle->aux_boil_counttime_flag == 1)
    {
        aux_handle->aux_remain_time--;

        if(aux_handle->enter_boil_time == 1)
        {
            LOG_default("首次沸腾，开始倒计时\r\n");
            
        }
        else if(aux_handle->enter_boil_time == 2)
        {
            LOG_default("第二次沸腾，开始倒计时\r\n");
        }
        else if(aux_handle->aux_remain_time > 2)
        {
            //后续进入沸腾仍然按照之前的倒计时进行
            LOG_default("后续沸腾，继续计时\r\n");
        }
    }
    
    //剩余倒计时开始倒计时了，且整分钟的发生变化，进行发送剩余分钟倒计时给电控
    if(aux_handle->aux_remain_time != aux_handle->aux_set_time * 60 * INPUT_DATA_HZ && aux_handle->aux_remain_time % (60 * AUX_DATA_HZ) == 0)
    {
        if(aux_remaintime_cb != NULL)
        {
            //回调函数通知电控显示倒计时时间发生了变化，倒计时时间的单位是min
            aux_remaintime_cb(aux_handle->aux_remain_time / (60 * AUX_DATA_HZ));
        }
    }
    
    //煮模式计时结束，关火，通知电控模式成功退出
    if(aux_handle->aux_remain_time == 0)
    {
        LOG_default("boil count down time is on,close fire[boil success end!!!]\r\n");
        if(aux_close_fire_cb != NULL)
        {
            aux_close_fire_cb(INPUT_RIGHT);             //右灶关火
        }

        aux_handle->aux_switch = 0;                 //辅助烹饪煮模式结束
        if(aux_exit_cb != NULL)
        {
            aux_exit_cb(AUX_SUCCESS_EXIT);              //成功退出
        }

        change_multivalve_gear(0x00, INPUT_RIGHT);
        cook_aux_reinit(INPUT_RIGHT);
    }
    else if(aux_handle->aux_remain_time <= 2 * 60 * INPUT_DATA_HZ && aux_handle->aux_multivalve_gear != 0x01)
    {
        change_multivalve_gear(0x01, INPUT_RIGHT);
    }

    aux_handle->boil_current_status_tick++;

    LOG_default("boil_remain_time:%d,enter_boil_time:%d\r\n",aux_handle->aux_remain_time,aux_handle->enter_boil_time);
    LOG_default("[%s]current status:[%s],total_tick:%d,current_status_tick:%d,next_status:%s,next_status_tick:%d\r\n",\
    __func__,boil_status_info[aux_handle->boil_current_tendency ],aux_handle->aux_total_tick,aux_handle->boil_current_status_tick,\
    boil_status_info[aux_handle->boil_next_tendency],aux_handle->boil_next_status_tick);
}



/**
 * @brief: 炸场景
 * @param {aux_handle_t} *aux_handle
 * @return {*}
 */
void mode_fry_func(aux_handle_t *aux_handle)
{
    //记录刚进入模式时的温度
    static unsigned int enter_mode_temp = 0;
    static bool gentle_flag = false;
    LOG_default("enter func:%s\r\n",__func__);
    aux_handle->fry_last_change_gear_tick++;

    if(aux_handle->aux_total_tick == 1)
    {
        LOG_default("enter fry mode temp:%d\r\n", aux_handle->current_average_temp);
        enter_mode_temp = aux_handle->current_average_temp;
        aux_handle->fry_step = 0;               //重置炸场景步骤
    }
    
    //进入炸模式后，如果当前步骤还未确定，且处于进入模式的4s-30s内，则判断是否处于热锅状态
    if(aux_handle->fry_step == 0)
    {
        unsigned char gentle_count = 0;
        unsigned char rise_count = 0;
        //首先判断10组温度没有发生大的跳变，处于较为稳定的状态
        for(int i = 0; i < ARRAY_DATA_SIZE - 1; i++)
        {
            //相邻的温度，新的温度大于旧的温度，新的温度大于旧的温度的幅度小于等于2度
            if(aux_handle->temp_array[i + 1] >= aux_handle->temp_array[i] && \
            aux_handle->temp_array[i + 1] - aux_handle->temp_array[i] <= 20)
            {
                gentle_count++;
            }
        }

        //温度处于较为平稳的阶段，才能判断是否处于稳定上升的阶段
        if(gentle_count >= 9)
        {
            //最新的温度比最早的温度高5度或以上
            rise_count += aux_handle->temp_array[0] + 50 <= aux_handle->temp_array[ARRAY_DATA_SIZE - 1];
            //间隔四个温度之间的温度上升幅度大于等于2度
            rise_count += aux_handle->temp_array[0] + 20 <= aux_handle->temp_array[5];
            rise_count += aux_handle->temp_array[1] + 20 <= aux_handle->temp_array[6];
            rise_count += aux_handle->temp_array[2] + 20 <= aux_handle->temp_array[7];
            rise_count += aux_handle->temp_array[3] + 20 <= aux_handle->temp_array[8];
            rise_count += aux_handle->temp_array[4] + 20 <= aux_handle->temp_array[9];   
        }

        if(aux_handle->aux_total_tick <= 4 * 5)
        {
            LOG_default("初始阶段，5s内不做处理\r\n");
        }
        else if(aux_handle->aux_total_tick > 4 * 5 && aux_handle->aux_total_tick <= 4 * 30)
        {
            //判断是否处于热锅阶段
            if(rise_count >= 6)
            {
                aux_handle->rise_quick_tick++;
            }
            else if(rise_count < 2)
            {
                aux_handle->rise_slow_tick++;
                
            }
            else        //如果都不满足暂时不判断
            {
                //如果30s即将过去仍然判断不到是热油还是热锅，直接默认为热油阶段，准备开始控温逻辑
                if(aux_handle->aux_total_tick == 4 * 30)
                {
                    LOG_GRE("30s没有检测到处于何种阶段，直接设定为热油阶段");
                    aux_handle->fry_step = 1;
                }
            }
        }
        
        //连续3s都判断为此状态，因为10组温度数据代表的是2.5s内的温度数据，3s仍为此状态比较稳妥的判断当前的步骤
        if(aux_handle->rise_quick_tick >= 3 * AUX_DATA_HZ)
        {
            LOG_GRE("检测到处于热锅阶段");
            aux_handle->fry_step = 1;
        }
        else if(aux_handle->rise_slow_tick >= 3 * AUX_DATA_HZ)
        {
            LOG_GRE("检测到处于热油阶段");
            aux_handle->fry_step = 2;
        }
    }

    int gentle_count = 0;
    //判断是否处于温度较稳定的平缓状态
    for(int i = 0; i < ARRAY_DATA_SIZE - 1; i++)
    {
        if(abs(aux_handle->temp_array[i] - aux_handle->temp_array[i+1]) <= 20)
        {
            gentle_count++;
        }
    }

    if(gentle_count >= 9)
    {
        gentle_flag = true;
    }
    else
    {
        gentle_flag = false;
    }

    //处于热锅阶段，进行热锅阶段温控逻辑
    if(aux_handle->fry_step == 1 && gentle_flag == true)
    {
        if(aux_handle->current_average_temp >= 200 * 10 && aux_handle->current_average_temp < 220 * 10) 
        {
            change_multivalve_gear(0x04, INPUT_RIGHT);
            aux_handle->fry_last_change_gear_tick = 0;
            if(beep_control_cb != NULL)
            {
                beep_control_cb(0x02);          //提醒用户加油   
            }
        }
        else if(aux_handle->current_average_temp > 220 * 10)
        {
            change_multivalve_gear(0x07, INPUT_RIGHT); 
            aux_handle->fry_last_change_gear_tick = 0;
        }
    }

    //已经检测到热锅之后或者已经超过了30s,开始判断是否放入油或食材;即默认热锅最多30s
    if(aux_handle->fry_step == 1  || aux_handle->aux_total_tick > 30 * 4)
    {
        //0.25s相邻的两个温度发生大于5度的波动
        if(abs(aux_handle->temp_array[ARRAY_DATA_SIZE - 1] - aux_handle->temp_array[ARRAY_DATA_SIZE - 2]) > 50)
        {
            aux_handle->fry_step = 2;              //进入到了控温步骤
            LOG_default("判断可能是放入了食用油或食材\r\n");
        }
    }

    //进入控温逻辑，当温度处于跳动状态的时候无法进行控温操作
    if(aux_handle->fry_step == 2 && gentle_flag == true)
    {
        if(aux_handle->current_average_temp <= aux_handle->aux_set_temp * 10 - 10 * 15)
        {
            if(aux_handle->aux_multivalve_gear != 0 && aux_handle->fry_last_change_gear_tick >= 10 * 4)
            {
                aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;      //记录调档之前的平均温度

                change_multivalve_gear(0x00, INPUT_RIGHT);                                      //设置为最大档加热
                aux_handle->fry_last_change_gear_tick = 0;
            }
        }
        /*暂时注释掉此逻辑，中间过渡档位过多会出现问题。如果调固定档之后出现了温度下降，则需要返回再次判断一次。
        例如：假设一大锅东西必须使用1档才能维持在目标温度，在距离目标温度30度的时候调为了2档，接着温度下降，下降到距离目标温度50度的时候，
        再次调整为0档。随后距离目标温度30度的时候再次调整为2档，始终无法跳到1档去维持这个温度。
        因此，在初始档位靠近目标温度之后，只调整一次档位，档位取值为较为中间的档位3档，随后进入动态控温逻辑*/
        // else if(aux_handle->current_average_temp <= aux_handle->aux_set_temp * 10 - 30 * 10)
        // {
        //     if(aux_handle->aux_multivalve_gear != 0x02 && aux_handle->fry_last_change_gear_tick >= 10 * 4)
        //     {
        //         aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;
                
        //         change_multivalve_gear(0x02, INPUT_RIGHT);
        //         aux_handle->fry_last_change_gear_tick = 0;
        //     }
        // }
        //距离目标温度-15度的时候开始降档，提前减速，后续方便控温
        else if(aux_handle->current_average_temp > aux_handle->aux_set_temp * 10 - 10 * 15)
        {
            if(aux_handle->aux_multivalve_gear != 0x03 && aux_handle->fry_last_change_gear_tick >= 10 * 4)
            {
                aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;

                change_multivalve_gear(0x03, INPUT_RIGHT);
                aux_handle->fry_last_change_gear_tick = 0;
                aux_handle->fry_step = 3;
            }
        }
    }

    if(aux_handle->fry_step == 3 && gentle_flag == true)
    {
        //距离目标温度-5度的时候开始降档，提前减速，后续方便控温
        if(aux_handle->current_average_temp >= aux_handle->aux_set_temp * 10 - 10 * 5)
        {
            //上次调火4s后
            unsigned char gear;
            //判断调档后温度下降了
            if(aux_handle->current_average_temp <=  aux_handle->fry_last_gear_average_temp)
            {
                if(aux_handle->fry_last_change_gear_tick >= 15 * 4)
                {
                    gear = aux_handle->aux_multivalve_gear - 1;
                    aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;
                    change_multivalve_gear(gear, INPUT_RIGHT);
                    aux_handle->fry_last_change_gear_tick = 0;
                }
                else if(aux_handle->fry_last_change_gear_tick >= 10 * 4)
                {
                    gear = aux_handle->aux_multivalve_gear - 2;
                    aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;
                    change_multivalve_gear(gear, INPUT_RIGHT);
                    aux_handle->fry_last_change_gear_tick = 0;
                }
            }
            

            
            if(aux_handle->aux_multivalve_gear != 0x06 && aux_handle->fry_last_change_gear_tick >= 10 * 4)
            {
                aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;

                change_multivalve_gear(0x06, INPUT_RIGHT);
                aux_handle->fry_last_change_gear_tick = 0;
                aux_handle->fry_step = 3;
            }
        }
    }

    if(aux_handle->fry_step >= 2 && aux_handle->fry_step <= 3 && aux_handle->first_reach_set_temp_flag == 0)
    {
        if(aux_handle->current_average_temp - aux_handle->aux_set_temp * 10 >= 0)
        {
            LOG_GRE("首次达到设定的目标温度");
            aux_handle->first_reach_set_temp_flag = 1;
        }
    }

    if(aux_handle->fry_step == 3 && gentle_flag == true)
    {
        if(aux_handle->fry_last_change_gear_tick >= 10 * 4 && aux_handle->current_average_temp >= aux_handle->aux_set_temp * 10 - 5 * 10)
        {
            unsigned char gear = 0;
            //温度高于目标温度5摄氏度以上，降低火力操作（档位越大火力越小）
            if(aux_handle->current_average_temp >= aux_handle->aux_set_temp * 10 + 10 * 5)
            {
                gear = aux_handle->aux_multivalve_gear + 3;
            }
            else if(aux_handle->current_average_temp >= aux_handle->aux_set_temp * 10)
            {
                gear = aux_handle->aux_multivalve_gear + 2;
            }
            else if(aux_handle->current_average_temp >= aux_handle->aux_set_temp * 10 - 5 * 10)
            {
                gear = aux_handle->aux_multivalve_gear + 1;
            }
            aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;
            change_multivalve_gear(gear, INPUT_RIGHT);
            aux_handle->fry_last_change_gear_tick = 0;

        }
        else if(aux_handle->fry_last_change_gear_tick >= 10 * 4 && aux_handle->current_average_temp < aux_handle->aux_set_temp * 10 - 5 * 10)
        {
            //温度高于目标温度10摄氏度以上，增大火力操作（档位越小火力越大）
            unsigned char gear = aux_handle->aux_multivalve_gear - 1;
            aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;
            change_multivalve_gear(gear, INPUT_RIGHT);
            aux_handle->fry_last_change_gear_tick = 0;
        }
    }

    LOG_default("current step:%d,current gear:%d\r\n",aux_handle->fry_step, aux_handle->aux_multivalve_gear);
    return;
}

/**
 * @brief: 
 * @param {aux_handle_t} *aux_handle
 * @return {*}
 */
void mode_chao_func(aux_handle_t *aux_handle)
{
    LOG_default("enter func:%s\r\n",__func__);
    return;
}

/**
 * @brief: 
 * @param {aux_handle_t} *aux_handle
 * @return {*}
 */
void mode_jian_func(aux_handle_t *aux_handle)
{
    LOG_default("enter func:%s\r\n",__func__);
    return;
}


/**
 * @brief:辅助烹饪参数重置函数 
 * @param {aux_handle_t} *aux_handle
 * @return {*}
 */
void aux_cook_reset(aux_handle_t *aux_handle)
{
    return;
}

/**
 * @brief: 辅助烹饪温度输入函数
 * @param {enum INPUT_DIR} input_dir
 * @param {unsigned short} temp
 * @param {unsigned short} environment_temp
 * @return {*}
 */
void aux_assistant_input(enum INPUT_DIR input_dir, unsigned short temp, unsigned short environment_temp)
{
    aux_handle_t *aux_handle = get_aux_handle(input_dir);

    //aux_handle->aux_total_tick++;
    aux_temp_save(aux_handle, temp);
    LOG_default("present target temp:%d,environment temp:%d\r\n",temp,environment_temp);

    if(aux_handle->aux_switch == 0 || aux_handle->ignition_switch == 0)
    {   
        LOG_default("aux closed\r\n");
        return;
    }

    aux_handle->aux_total_tick++;

    // if(aux_handle->aux_total_tick >= MAX_FIRE_TIME)
    // {
    //     LOG_default("兜底关火时间%d到，关闭火焰\r\n",MAX_FIRE_TIME/4);
    //     aux_close_fire_cb(INPUT_RIGHT);         //右灶关火
    //     aux_handle->aux_switch = 0;             //退出辅助烹饪模式
    // }

    if(aux_handle->temp_size < 10)
    {
        LOG_default("temp data is not enough,func return\r\n");
        return;
    }
    
    switch(aux_handle->aux_type)
    {
        case MODE_CHAO:
            mode_chao_func(aux_handle);
            break;
        case MODE_ZHU:
            mode_boil_func(aux_handle);
            break;
        case MODE_JIAN:
            mode_jian_func(aux_handle);
            break;
        case MODE_ZHA:
            mode_fry_func(aux_handle);
            break;
        default:
        {
            LOG_default("Error cook Mode\r\n");
            return;
        }
    }    
}


