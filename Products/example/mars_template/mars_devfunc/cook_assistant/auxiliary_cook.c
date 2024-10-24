/*
 * @Description  : 
 * @Author       : zhouxc
 * @Date         : 2024-10-21 10:37:25
 * @LastEditors  : zhouxc
 * @LastEditTime : 2024-10-24 14:49:52
 * @FilePath     : /et70-ca3/Products/example/mars_template/mars_devfunc/cook_assistant/auxiliary_cook.c
 */

#include <stdio.h>
#include <stdlib.h>
#include "cloud.h"
#include "fsyd.h"
#include "auxiliary_cook.h"
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)

aux_handle_t g_aux_state[2];
char *boil_status[]={"idle","gentle","rise","down","boiled"};
char *aux_mode[] = {"chao","zhu","jian","zha"};

static int(*beep_cb)(int beep_type); 
void register_beep_cb(int(*cb)(int beep_type))
{
    beep_cb = cb;
}

static int(*multi_valve)(enum INPUT_DIR input_dir, int gear);
void register_multivalve_cb(int(*cb)(enum INPUT_DIR input_dir, int gear))
{
    multi_valve = cb;
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
    static before_next_tendency = IDLE;
    do
    {
        //上升趋势========================================================
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



        //下降趋势====================================================
        for(int i = 0; i < ARRAY_DATA_SIZE - 1; i++)
        {
            if(aux_handle->temp_array[i+1] - aux_handle->temp_array[i] > 80)
            {
                aux_handle->boil_next_tendency = GENTLE;
                aux_handle->boil_next_status_tick = 4 * 5;     //快速的变化直接赋值下降的时间为5s，以及时切换到下降状态
                printf("[%s]present state is gentle\r\n",__func__);
                break;
            }
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
            aux_handle->boil_current_tendency = aux_handle->boil_next_tendency;
            aux_handle->boil_current_status_tick = 0;
            aux_handle->boil_next_status_tick = 0;
        }
    }
    else if(aux_handle->boil_current_tendency == RISE && aux_handle->boil_next_tendency != RISE)
    {
        if(aux_handle->boil_next_status_tick > 4 * 6)
        {
            aux_handle->boil_current_tendency = aux_handle->boil_next_tendency;
            aux_handle->boil_current_status_tick = 0;
            aux_handle->boil_next_status_tick = 0;
        }
    }
    else if(aux_handle->boil_next_tendency == DOWN && aux_handle->boil_next_tendency != DOWN)
    {
        if(aux_handle->boil_next_status_tick > 4 * 2)
        {
            aux_handle->boil_current_tendency = aux_handle->boil_next_tendency;
            aux_handle->boil_current_status_tick = 0;
            aux_handle->boil_next_status_tick = 0;
        }
    }
    else if(aux_handle->boil_current_tendency == GENTLE && aux_handle->boil_next_tendency != GENTLE)
    {
        if(aux_handle->boil_next_status_tick > 4 * 10)
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
        if(aux_handle->current_average > 850 && aux_handle->current_average < 1100 && aux_handle->boil_current_status_tick > 4 * 15)
        {
            aux_handle->boil_current_tendency = BOILED;
            printf("切换到水开状态\r\n");
            aux_handle->boil_current_status_tick = 0;
            aux_handle->boil_next_status_tick = 0;
        }
    }

    aux_handle->boil_current_status_tick++;

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


