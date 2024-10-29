/*
 * @Description  : 
 * @Author       : zhouxc
 * @Date         : 2024-10-21 10:37:25
 * @LastEditors  : zhouxc
 * @LastEditTime : 2024-10-28 19:55:44
 * @FilePath     : /et70-ca3/Products/example/mars_template/mars_devfunc/cook_assistant/auxiliary_cook.c
 */

#include <stdio.h>
#include <stdlib.h>
#include "cloud.h"
#include "fsyd.h"
#include "auxiliary_cook.h"
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)
#define AUX_DATA_HZ 4
#define START 2
#define STEP 3

aux_handle_t g_aux_state[2];
char *boil_status[]={"idle","gentle","rise","down","boiled"};
char *aux_mode[] = {"chao","zhu","jian","zha"};

static int(*beep_control_cb)(int beep_type); 
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
void change_multivalve_task(int gear)
{
    if(gear > 7 || gear < 0)
    {
        printf("multivalve not support this gear\r\n");
        return;
    }
    if(multi_valve_cb != NULL)
    {
        multi_valve_cb(INPUT_RIGHT,gear);
    }
    
}

static int (*aux_close_fire_cb)(enum INPUT_DIR input_dir);
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
    printf("aux seting:==============================\r\n\
    aux_switch:%d,aux_mode:%s aux_set_time:%d,aux_set_temp:%d\r\n",\
    aux_handle->aux_switch, aux_mode[aux_handle->aux_type - 1], aux_handle->aux_set_time, aux_handle->aux_set_temp);
    return;
}


void cook_aux_reinit(enum INPUT_DIR input_dir)
{
    aux_handle_t *aux_handle = get_aux_handle(input_dir);
    aux_handle->aux_switch = 0;
    aux_handle->aux_type = 0;
    aux_handle->aux_set_time = 0;
    aux_handle->aux_set_temp = 0;

    aux_handle->aux_total_tick = 0;
    aux_handle->boil_current_tendency = 0;
    aux_handle->boil_current_status_tick = 0;
    aux_handle->boil_next_tendency = 0;
    aux_handle->boil_next_status_tick = 0;
    aux_handle->enter_boil_time = 0;
    aux_handle->aux_boil_counttime_flag = 0;
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
        for(int i = 0; i <= ARRAY_DATA_SIZE - 2; i++)
        {
            aux_handle->temp_array[i] = aux_handle->temp_array[i+1];
        }
        aux_handle->temp_array[ARRAY_DATA_SIZE - 1] = temp;
    }
    else if(aux_handle->temp_size > ARRAY_DATA_SIZE)
    {
        printf("aux deal temp data error!!!\r\n");
    }

    printf("aux temp array is:");
    unsigned int average_temp = 0;
    for(int i = 0; i < aux_handle->temp_size; i++)
    {
        printf("%d ",aux_handle->temp_array[i]);
        average_temp += aux_handle->temp_array[i];
    }
    printf("\r\n");
    average_temp /= ARRAY_DATA_SIZE;
    printf("present average temp is:%d\r\n",average_temp);
    aux_handle->current_average = average_temp;

    //每10个数据计算法一个平均值，单个数据时间间隔0.25s，即2.5s取一个平均值
    if(aux_handle->aux_total_tick % 10 == 0)
    {
        if(aux_handle->average_temp_size < ARRAY_DATA_SIZE)
        {
            aux_handle->average_temp_array[aux_handle->average_temp_size] = average_temp;
            aux_handle->average_temp_size++;
        }
        else if(aux_handle->average_temp_size == ARRAY_DATA_SIZE)
        {
            for(int i = 0; i <= ARRAY_DATA_SIZE - 2; i++)
            {
                aux_handle->average_temp_array[i] = aux_handle->average_temp_array[i+1];
            }
            aux_handle->average_temp_array[ARRAY_DATA_SIZE - 1] = average_temp;
        }
        else if(aux_handle->average_temp_size > ARRAY_DATA_SIZE)
        {
            printf("aux deal average temp data error!!!\r\n");
        }
    }

    printf("=============================================\r\naux average temp array is:");
    for(int i = 0; i < aux_handle->average_temp_size; i++)
    {
        printf("%d ",aux_handle->average_temp_array[i]);
    }
    printf("\r\n=============================================\r\n");

}



/**
 * @brief: 煮场景
 * @param {aux_handle_t} *aux_handle
 * @return {*}
 */
void mode_boil_func(aux_handle_t *aux_handle)
{
    printf("enter func:%s\r\n",__func__);
    //用于判断此次状态和上次状态之间是否发生了变化
    static int before_next_tendency = IDLE;
    do
    {
        //上升趋势========================================================
        if(aux_handle->temp_array[0] - aux_handle->temp_array[ARRAY_DATA_SIZE - 1] < 0)
        {
            printf("begin rise tendency judge\r\n");
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
            if(aux_handle->average_temp_size >= 6)
            {
                //6个最新的平均温度之间两两进行5次比较
                for(int i = aux_handle->average_temp_size -1; i >= aux_handle->average_temp_size - 5; i--)
                {
                    if(aux_handle->average_temp_array[i] - aux_handle->average_temp_array[i-1] >= 3)
                    {
                        rise_count3++;
                    }
                }
            } 
            printf("rise_count1:%d,rise_count2:%d,rise_count3:%d\r\n",rise_count1,rise_count2,rise_count3);
            //rise_count3 = (aux_handle->average_temp_array[aux_handle->average_temp_size - 1] - aux_handle->average_temp_array[aux_handle->average_temp_size - 2] >= 10);
            //rise_count3 += 
            //仅仅靠10个温度难以判断是rise还是gentle
            //if((rise_count1 >= 7 && rise_count2 >= 4) || 
            if(rise_count3 >= 4)
            {
                aux_handle->boil_next_tendency = RISE;
                printf("[%s]present state is rising\r\n",__func__);
                break;
            }
        }
        else
        {
            printf("begin down tendency judge\r\n");
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
                printf("[%s]present state is down\r\n",__func__);
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
            //         printf("[%s]present state is down\r\n",__func__);
            //         break;
            //     }
            // }

        }

        //平缓趋势======================================================================
        unsigned char gentle_count1 = 0;
        //int gentle_count2 = 0;
        for(int i = 0; i < ARRAY_DATA_SIZE; i++)
        {
            if(abs(aux_handle->temp_array[i] - aux_handle->current_average) < 13)
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
            printf("[%s]present state is gentle\r\n",__func__);
            break;
        }

        

        

        aux_handle->boil_next_tendency = IDLE;
        break;
    }while(1);

    //待切换状态变化，则计时清零，开始对新状态进行计时
    if(aux_handle->boil_next_tendency != before_next_tendency)
    {
        printf("next status change");
        before_next_tendency = aux_handle->boil_next_tendency;
        aux_handle->boil_next_status_tick = 0;
    }
    //状态未发生变化，继续对next_status_tick计时
    else
    {
        aux_handle->boil_next_status_tick++;
    }

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
            printf("[change_status],to idle\r\n");
            aux_handle->boil_current_tendency = aux_handle->boil_next_tendency;
            aux_handle->boil_current_status_tick = 0;
            aux_handle->boil_next_status_tick = 0;
        }
    }
    else if(aux_handle->boil_current_tendency == RISE && aux_handle->boil_next_tendency != RISE)
    {
        if(aux_handle->boil_next_status_tick > 4 * 6)
        {
            printf("[change_status],rise to %s\r\n",boil_status[aux_handle->boil_next_tendency]);
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
            printf("要切换的是gentle，仍然保持boil状态");
        }
        else if(aux_handle->boil_next_tendency == DOWN)
        {
            //已经发生过水开的情况了
            if(aux_handle->enter_boil_time > 0)
            {
                printf("八段阀再次调整火焰为大火\r\n");
                if(multi_valve_cb != NULL)
                {
                    multi_valve_cb(INPUT_RIGHT, 0x00);
                }   
            }

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

    if(aux_handle->boil_current_tendency == GENTLE)
    {
        if(aux_handle->current_average > 850 && aux_handle->current_average < 1100 && aux_handle->boil_current_status_tick > 4 * 12)
        {
            aux_handle->boil_current_tendency = BOILED;
            printf("切换到水开状态\r\n");
            
            //进入水开次数累加
            aux_handle->enter_boil_time++;

            //前两次进入水沸状态都重新倒计时，这里对当前倒计时进行清零。对应的是冷水放食材直接煮开到结束，或者冷水煮开后再放食材煮沸后到结束
            if(aux_handle->enter_boil_time <= 2)
            {
                aux_handle->aux_remain_time = aux_handle->aux_set_time * 60 * AUX_DATA_HZ;
            }

            aux_handle->aux_boil_counttime_flag = 1;        //开启倒计时

            //检测到水开，调整火力档位，设置蜂鸣器蜂鸣
            if(multi_valve_cb != NULL)
            {
                multi_valve_cb(INPUT_RIGHT,3);
            }
            if(beep_control_cb != NULL)
            {
                beep_control_cb(0x03);
            }
            

            aux_handle->boil_current_status_tick = 0;
            aux_handle->boil_next_status_tick = 0;
        }
    }


    //aux暂时设置，enter_boil_time>1之后直接执行aux_handle->aux_remain_time--;即可，不需要再次区分，上方进入水沸的时候会判断是否需要清空
    if(aux_handle->aux_boil_counttime_flag == 1)
    {
        aux_handle->aux_remain_time--;

        if(aux_handle->enter_boil_time == 1)
        {
            printf("首次沸腾，开始倒计时\r\n");
            
        }
        else if(aux_handle->enter_boil_time == 2)
        {
            printf("第二次沸腾，开始倒计时\r\n");
        }
        else if(aux_handle->aux_remain_time > 2)
        {
            //后续进入沸腾仍然按照之前的倒计时进行
            printf("后续沸腾，继续计时\r\n");
        }
    }
    
    if(aux_handle->aux_remain_time % (60 * AUX_DATA_HZ) == 0)
    {
        if(aux_remaintime_cb != NULL)
        {
            //回调函数通知电控显示倒计时时间发生了变化，倒计时时间的单位是min
            aux_remaintime_cb(aux_handle->aux_remain_time / (60 * AUX_DATA_HZ));
        }
    }
    
    if(aux_handle->aux_remain_time == 0)
    {
        printf("boil count down time is on,close fire[boil success end!!!]\r\n");
        if(aux_close_fire_cb != NULL)
        {
            aux_close_fire_cb(INPUT_RIGHT);             //右灶关火
        }
        
        
        aux_handle->aux_switch = 0;                 //辅助烹饪煮模式结束
        
        if(aux_exit_cb != NULL)
        {
            aux_exit_cb(AUX_SUCCESS_EXIT);              //成功退出
        }

        if(multi_valve_cb != NULL)
        {
            multi_valve_cb(INPUT_RIGHT, 0);
        }
        cook_aux_reinit(INPUT_RIGHT);
    }

    

    aux_handle->boil_current_status_tick++;

    printf("boil_remian_time:%d,enter_boil_time:%d\r\n",aux_handle->aux_remain_time,aux_handle->enter_boil_time);
    printf("[%s]current status:[%s],total_tick:%d,current_status_tick:%d,next_status:%s,next_status_tick:%d\r\n",\
    __func__,boil_status[aux_handle->boil_current_tendency ],aux_handle->aux_total_tick,aux_handle->boil_current_status_tick,\
    boil_status[aux_handle->boil_next_tendency],aux_handle->boil_next_status_tick);
}



/**
 * @brief: 炸场景
 * @param {aux_handle_t} *aux_handle
 * @return {*}
 */
void mode_fry_func(aux_handle_t *aux_handle)
{
     printf("enter func:%s\r\n",__func__);
     return;
}

/**
 * @brief: 
 * @param {aux_handle_t} *aux_handle
 * @return {*}
 */
void mode_chao_func(aux_handle_t *aux_handle)
{
    printf("enter func:%s\r\n",__func__);
    return;
}

/**
 * @brief: 
 * @param {aux_handle_t} *aux_handle
 * @return {*}
 */
void mode_jian_func(aux_handle_t *aux_handle)
{
    printf("enter func:%s\r\n",__func__);
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
    aux_temp_save(aux_handle, temp);
    printf("present target temp:%d,environment temp:%d\r\n",temp,environment_temp);

    if(aux_handle->aux_switch == 0 || aux_handle->ignition_switch == 0)
    {   
        printf("aux closed\r\n");
        return;
    }

    aux_handle->aux_total_tick++;

    // if(aux_handle->aux_total_tick >= MAX_FIRE_TIME)
    // {
    //     printf("兜底关火时间%d到，关闭火焰\r\n",MAX_FIRE_TIME/4);
    //     aux_close_fire_cb(INPUT_RIGHT);         //右灶关火
    //     aux_handle->aux_switch = 0;             //退出辅助烹饪模式
    // }

    if(aux_handle->temp_size < 10)
    {
        printf("temp data is not enough,func return\r\n");
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
            printf("Error cook Mode\r\n");
            return;
        }
    }    
}


