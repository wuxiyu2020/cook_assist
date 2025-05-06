/*
 * @Description  :
 * @Author       : zhouxc
 * @Date         : 2024-10-21 10:37:25
 * @LastEditors  : zhouxc
 * @LastEditTime : 2024-11-28 10:43:06
 * @FilePath     : /et70-ca3/Products/example/mars_template/mars_devfunc/cook_assistant/auxiliary_cook/auxiliary_cook.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "auxiliary_cook.h"
#include "iot_import.h"
#ifndef MODULE_TEST
    #include "../cloud.h"
    #include "../fsyd.h"
#endif
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)
#define AUX_DATA_HZ 4
#define START 2
#define STEP 3

intptr_t fd = -1;
NetworkAddr udp_dest_addr = {0x00};
#define UDP_VOICE_ADDR    "192.168.50.54"
#define UDP_VOICE_PORT    (6711)
#define UDP_VOICE_MSG_QUEUE_SIZE (8)
aos_queue_t udp_voice_msg_queue;
char udp_voice_msg_queue_buff[UDP_VOICE_MSG_QUEUE_SIZE * 4];
#define AUX_COOK_VOICE_KV   "aux_cook_voice"
int voice_switch = 0;

ring_buffer_t fire_gear_queue;

typedef struct {
    uint64_t time;
    uint16_t temp;
}temp_value_t;

temp_value_t save_current_temp(uint16_t cur_temp)
{
    temp_value_t value = 
    {
        .time = aos_now_ms(),
        .temp = cur_temp
    };

    return value;
}

bool judge_cook_trend_turn_over(uint16_t array[], uint16_t length);
bool judge_cook_trend_rise(uint16_t array[], uint16_t length, int threshold);
bool judge_cook_trend_down(uint16_t array[], uint16_t length, int threshold);
bool check_any_large_diff(uint16_t array[], uint16_t length);

double calculateSlope(temp_value_t value1, temp_value_t value2)
{
    return ((double)(value2.temp - value1.temp)) / ((value2.time - value1.time) / 1000);
}

// 比较函数，用于qsort排序
int compare(const void *a, const void *b)
{
    return (*(uint16_t*)a - *(uint16_t*)b);
}

// 计算中位数
uint16_t findMedian(uint16_t data[], size_t size)
{
    unsigned short arr[ARRAY_DATA_SIZE] = {0x00};

    // 对数组进行排序
    memcpy(arr, data, sizeof(arr));
    qsort(arr, size, sizeof(uint16_t), compare);

    // 计算中位数
    if (size % 2 == 0) // 偶数个元素，中位数是中间两个数的平均值
    {
        return (arr[size / 2 - 1] + arr[size / 2]) / 2;
    }
    else // 奇数个元素，中位数是中间的那个数
    {
        return arr[size / 2];
    }
}

//计算最大值
uint16_t findMax(uint16_t data[], size_t size)
{
    uint16_t max_value = data[0];  // 假设第一个元素是最大值
    for (size_t i = 1; i < size; ++i) {
        if (data[i] > max_value) {
            max_value = data[i];
        }
    }

    return max_value;
}

//计算最小值
uint16_t findMin(uint16_t data[], size_t size)
{
    uint16_t min_value = data[0];  // 假设第一个元素是最小值
    for (size_t i = 1; i < size; ++i) {
        if (data[i] < min_value) {
            min_value = data[i];
        }
    }

    return min_value;
}

//计算平均值
uint16_t findAver(uint16_t data[], size_t size)
{
    uint32_t sum = 0;  // 使用 uint32_t 防止溢出
    for (size_t i = 0; i < size; ++i) {
        sum += data[i];
    }

    // 计算平均值并舍弃小数部分
    uint16_t average = (uint16_t)(sum / size);
    return average;
}

void printf_cur_temp_array(aux_handle_t* aux_handle, uint16_t temp)//最新温度(125) = 1810 | xxx xxx xxx xxx xxx xxx xxx xxx xxx xxx (最小=2430 最大=2440 平均=2432 中位=2430)
{
    uint8_t szArray[128] = {0x00};
    if(aux_handle->temp_size < ARRAY_DATA_SIZE) 
    {        
        for (int i=0; i<aux_handle->temp_size; i++)
        {
            sprintf(szArray + strlen(szArray), "%d ", aux_handle->temp_array[i]);
        }
        LOGI("aux","数据统计: 最新温度(%d) = %d | %s  ", aux_handle->aux_total_tick, temp, szArray);
    }
    else
    {
        for (int i=0; i<ARRAY_DATA_SIZE; i++)
        {
            sprintf(szArray + strlen(szArray), "%d ", aux_handle->temp_array[i]);
        }
        LOGI("aux","数据统计: 最新温度(%d) = %d | %s  (最小值=%d 最大值=%d 平均值=%d 中位值=%d)",
        aux_handle->aux_total_tick, 
        temp, 
        szArray,
        findMin(aux_handle->temp_array, ARRAY_DATA_SIZE),
        findMax(aux_handle->temp_array, ARRAY_DATA_SIZE),
        findAver(aux_handle->temp_array, ARRAY_DATA_SIZE),
        findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE));
    }
}

void udp_voice_init(void)
{
    fd = (intptr_t)HAL_UDP_create_without_connect(NULL, 5711);
    if ((intptr_t) - 1 == fd)
    {
        LOGI("aux","error: HAL_UDP_create_without_connect fail");
        //return;
    }

    memset(&udp_dest_addr, 0, sizeof(udp_dest_addr));
    memcpy(udp_dest_addr.addr, UDP_VOICE_ADDR, strlen(UDP_VOICE_ADDR));
    udp_dest_addr.port = UDP_VOICE_PORT;

    aos_queue_new(&udp_voice_msg_queue, udp_voice_msg_queue_buff, UDP_VOICE_MSG_QUEUE_SIZE * 4, 4);

    ring_buffer_init(&fire_gear_queue, 8, 1);
}

int udp_voice_deinit()
{
    int ret = close((int)fd);
    fd = -1;
    return ret;
}

int udp_voice_write_sync(const unsigned char* p_data, unsigned int datalen,unsigned int timeout_ms)
{
    if ((intptr_t) - 1 == fd)
    {
        return -1;
    }

    if (voice_switch == 0)
    {
        return -1;
    }

    LOGI("aux","播报语音消息: %s  (%s:%d)", p_data, udp_dest_addr.addr, udp_dest_addr.port);
    HAL_UDP_sendto(fd, &udp_dest_addr, p_data, strlen(p_data), 50);
}

int udp_voice_write(const unsigned char* p_data, unsigned int datalen,unsigned int timeout_ms)
{
    char* buff = (char*)aos_malloc(datalen+1);
    if (buff != NULL)
    {
        memset(buff, 0x00, datalen+1);
        memcpy(buff, p_data, datalen);
        uint32_t addr = (uint32_t)buff;
        aos_queue_send(&udp_voice_msg_queue, &addr, 4);
        LOGI("aux","语音消息·进入队列 (0x%08X %s)", buff, p_data);
    }
}

int udp_voice_send()
{
    static uint64_t time = 0;
    if ((aos_now_ms() - time) < 2000)
    {
        return -1;
    }

    if ((intptr_t) - 1 == fd)
    {
        return -1;
    }

    uint32_t addr = 0;
    unsigned int size_recv = 0;
    if (aos_queue_recv(&udp_voice_msg_queue, AOS_NO_WAIT, &addr, &size_recv) != 0) //AOS_WAIT_FOREVER 传入0获取不到会立即报失败
    {
        return -1;
    }

    char* buff = (char*)addr;
    if (buff != NULL)
    {
        LOGI("aux","语音消息·移出队列 (0x%08X %s:%d)", buff, udp_dest_addr.addr, udp_dest_addr.port);
        HAL_UDP_sendto(fd, &udp_dest_addr, buff, strlen(buff), 50);
        time = aos_now_ms();
        aos_free(buff);
        return 0;
    }

    return -1;

}

void switch_fsm_state(func_ptr_fsm_t* fsm, state_func_t func)
{
    (*fsm).state_time = 0;
    (*fsm).state_func = func;
}

void set_fsm_time(func_ptr_fsm_t* fsm)
{
    (*fsm).state_time = aos_now_ms();
}

bool is_fsm_timeout(func_ptr_fsm_t* fsm, int time)
{
    return ((aos_now_ms() - ((*fsm).state_time)) > time);
}

void run_fsm(func_ptr_fsm_t* fsm, void* user)
{
    if (((*fsm).state_func) != NULL)
    {
        (*((*fsm).state_func))(fsm, user);
    }
}

//煮模式加热水沸保底时间10min
#define BOIL_MODE_MAX_HEAT_TIME 10 * 4 * 60

aux_handle_t g_aux_state[2];
char *boil_status_info[]={"空闲", "平缓", "上升", "下降", "沸腾"};
char *aux_mode[] = {"退出辅助烹饪", "炒模式","煮模式","煎模式","炸模式"};

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
int change_multivalve_gear_old(unsigned char gear, enum INPUT_DIR input_dir)
{
    // aux_handle_t *aux_handle = get_aux_handle(input_dir);
    // char voice_buff[64] = {0x00};

    // //如果设置档位和当前档位一致，直接返回
    // if(aux_handle->aux_multivalve_gear == gear)
    // {
    //     LOGI("aux","error: 目标档位(%d)与当前档位(%d)一致，不下发切换指令", gear, aux_handle->aux_multivalve_gear);
    //     return -1;
    // }

    // //煮模式最后两分钟,只接受1档设置
    // if(aux_handle->aux_type == MODE_ZHU && aux_handle->aux_remain_time <= 2 * 60 * INPUT_DATA_HZ && gear != 0x01)
    // {
    //     return -1;
    // }

    // unsigned char fire_gear = gear;
    // ring_buffer_push(&fire_gear_queue, &fire_gear);
    // snprintf(voice_buff, sizeof(voice_buff), "火力%d档", gear);
    // udp_voice_write_sync(voice_buff, strlen(voice_buff), 50);
    // return 0;

    // //判断尚未到达目标档位，不进行档位调整
    // if(aux_handle->aux_multivalve_gear != aux_handle->aux_aim_multivalve_gear)
    // {
    //     LOGI("aux","error: 上一个切换(目标%d档)正在进行中，不下发切换指令", aux_handle->aux_aim_multivalve_gear);
    //     return -1;
    // }

    // //煮模式最后两分钟,只接受1档设置
    // if(aux_handle->aux_type == MODE_ZHU && aux_handle->aux_remain_time <= 2 * 60 * INPUT_DATA_HZ && gear != 0x01)
    // {
    //     return -1;
    // }

    // if(multi_valve_cb != NULL)
    // {
    //     aux_handle->aux_aim_multivalve_gear = gear;
    //     multi_valve_cb(INPUT_RIGHT, gear);
    //     snprintf(voice_buff, sizeof(voice_buff), "切换火力%d档", gear);
    //     udp_voice_write_sync(voice_buff, strlen(voice_buff), 50);
    //     return 0;
    // }

    // return -1;
}

void send_multivalve_gear(int dir)
{
    // if (ring_buffer_valid_size(&fire_gear_queue) > 0)
    // {
    //     aux_handle_t *aux_handle = get_aux_handle(dir);
    //     if(aux_handle->aux_multivalve_gear != aux_handle->aux_aim_multivalve_gear)//判断尚未到达目标档位，不进行档位调整
    //     {
    //         LOGI("aux","error: 上一个切换正在进行中......(%d)", aux_handle->aux_aim_multivalve_gear);
    //         return -1;
    //     }

    //     unsigned char gear = 0;
    //     ring_buffer_read(&fire_gear_queue, &gear, 1);
    //     if(multi_valve_cb != NULL)
    //     {
    //         aux_handle->aux_aim_multivalve_gear = gear;
    //         multi_valve_cb(INPUT_RIGHT, gear);
    //         return 0;
    //     }
    // }
}

int change_multivalve_gear(unsigned char gear, enum INPUT_DIR input_dir)
{
    aux_handle_t *aux_handle = get_aux_handle(input_dir);
    char voice_buff[64] = {0x00};

    if (gear > 7)
    {
        gear = 7;
    }

    //如果设置档位和当前档位一致，直接返回
    if(aux_handle->aux_multivalve_gear == gear)
    {
        //LOGI("aux","目标档位(%d)与当前档位(%d)一致，不下发切换指令", gear, aux_handle->aux_multivalve_gear);
        return -1;
    }

    if(aux_handle->aux_aim_multivalve_gear == gear)
    {
        //LOGI("aux","目标档位(%d)与上一次目标档位(%d)一致，不下发切换指令", gear, aux_handle->aux_multivalve_gear);
        return -1;
    }


    //煮模式最后两分钟,只接受1档设置
    if(aux_handle->aux_type == MODE_ZHU && aux_handle->aux_remain_time <= 2 * 60 * INPUT_DATA_HZ && gear != 0x01)
    {
        ///return -1;
    }
    

    //判断尚未到达目标档位，不进行档位调整
    // if(aux_handle->aux_multivalve_gear != aux_handle->aux_aim_multivalve_gear)
    // {
    //     LOGE("aux", "error: 上一个火力切换(目标%d档)正在进行中，不下发切换指令", aux_handle->aux_aim_multivalve_gear);
    //     snprintf(voice_buff, sizeof(voice_buff), "八段阀忙碌中,切换%d档失败", gear);
    //     udp_voice_write_sync(voice_buff, strlen(voice_buff), 50);
    //     return -1;
    // }

    if(multi_valve_cb != NULL)
    {
        aux_handle->aux_aim_multivalve_gear = gear;
        multi_valve_cb(INPUT_RIGHT, gear);
        snprintf(voice_buff, sizeof(voice_buff), "切换火力%d档", gear);
        //udp_voice_write_sync(voice_buff, strlen(voice_buff), 50);

        LOGW("aux", "change_multivalve_gear: 切换火力到 %d 档", gear);
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
    aux_handle->aux_switch      = aux_switch;
    aux_handle->aux_type        = cook_type;
    aux_handle->aux_set_time    = set_time;
    aux_handle->aux_set_temp    = set_temp;
    aux_handle->aux_remain_time = set_time * 60 * AUX_DATA_HZ;

    LOGI("aux","aux seting:============================== \
    aux_switch:%d, aux_mode:%s aux_set_time:%d, aux_set_temp:%d ",\
    aux_handle->aux_switch, aux_mode[aux_handle->aux_type], aux_handle->aux_set_time, aux_handle->aux_set_temp);

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

extern void aux_cook_reset(aux_handle_t *aux_handle);
void cook_aux_reinit(enum INPUT_DIR input_dir)
{
    aux_handle_t *aux_handle = get_aux_handle(input_dir);

    aux_handle->pan_fire_status = 0;
    //初始化辅助烹饪设置参数
    aux_handle->aux_switch = 0;
    aux_handle->aux_type = 0;
    aux_handle->aux_set_time = 0;
    aux_handle->aux_set_temp = 0;

    //初始化八段阀档位变量
    aux_handle->aux_multivalve_gear = 0;
    aux_handle->aux_aim_multivalve_gear = 0;

    aux_handle->max_up_value = 0;

    //煮模式变量重置
    aux_handle->boil_current_tendency = 0;
    aux_handle->boil_current_status_tick = 0;
    aux_handle->boil_next_tendency = 0;
    aux_handle->boil_next_status_tick = 0;
    aux_handle->enter_boil_time = 0;
    aux_handle->aux_boil_counttime_flag = 0;
    aux_handle->aux_boil_type = 0;
    aux_handle->tick_first_boil = 0;
    aux_handle->put_food_cnt    = 0;


    //炸模式变量重置
    aux_handle->fry_step = 0;
    aux_handle->aux_aim_multivalve_gear = 0;
    aux_handle->fry_last_gear_average_temp = 0;
    aux_handle->rise_quick_tick = 0;
    aux_handle->rise_slow_tick = 0;
    aux_handle->first_reach_set_temp_flag = 0;
    aux_handle->fry_last_change_gear_tick = 0;
    aux_handle->first_hot_pot_flag = 0;
    aux_handle->first_put_food_flag = 0;

    aux_handle->aux_total_tick = 0;
    memset(aux_handle->temp_array, 0x00, sizeof(aux_handle->temp_array));
    aux_handle->temp_size = 0;
    memset(aux_handle->average_temp_array, 0x00, sizeof(aux_handle->average_temp_array));
    aux_handle->average_temp_size = 0;

    aux_cook_reset(aux_handle);
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
void aux_temp_save(aux_handle_t *aux_handle, unsigned short temp)
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
        LOGI("aux","aux deal temp data error!!!");
    }

    //打印单个温度数组
    unsigned int average_temp = 0;
    for(int i = 0; i < aux_handle->temp_size; i++)
    {
        average_temp += aux_handle->temp_array[i];
    }
    average_temp /= aux_handle->temp_size;
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
            LOGI("aux","aux deal average temp data error!!! ");
        }
    }
}


/**
 * @brief:判断水开的函数
 * @param {aux_handle_t} *aux_handle
 * @return {*} 0:水未开，1：水已开
 */
int judge_water_boil(aux_handle_t *aux_handle)
{
    LOGI("aux","judge_water_boil: enter boil judge");
    //煮模式中的保底时间到，尚未发生过水开场景，保底认为水开
    if(aux_handle->aux_total_tick >= BOIL_MODE_MAX_HEAT_TIME && aux_handle->aux_type == MODE_ZHU &&\
     aux_handle->enter_boil_time < 1)
    {
        LOGI("aux","煮模式保底时间到认为已经是水开");
        return 1;
    }

    unsigned char very_gentle_count = 0;
    unsigned char before_gentle_rise = 0;
    for(int j = 0; j < ARRAY_DATA_SIZE -1; j++)
    {
        //单个温度处于绝对平稳
        if(aux_handle->temp_array[j] == aux_handle->temp_array[j+1])
        {
            very_gentle_count++;
        }

        //现在的平均温度处于上升状态
        if(aux_handle->average_temp_array[j] < aux_handle->average_temp_array[j+1])
        {
            before_gentle_rise++;
        }
    }

    //最新的10个温度处于绝对平稳状态，最新的平均温度处于绝对的平稳，此逻辑适用于玻璃盖，因此限制温度为80度以上
    if(very_gentle_count == ARRAY_DATA_SIZE - 1 && before_gentle_rise == ARRAY_DATA_SIZE - 1 && aux_handle->current_average_temp >= 80 * 10)
    {
        //首个平均温度和最后一个平均温度时间间隔为2.5*9=22.5s的时间温度上升了超过11度
        if(aux_handle->average_temp_array[ARRAY_DATA_SIZE - 1] - aux_handle->average_temp_array[0] >= 11 * 10)
        {
            LOGI("aux","judge_water_boil: 判断水已煮开1 !!!");
            return 1;
        }
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

        for(int j = 0; j < ARRAY_DATA_SIZE/2; j++)
        {
            rise_count2 += (aux_handle->average_temp_array[j] < aux_handle->average_temp_array[j+5]);
        }
        // rise_count2 += (aux_handle->average_temp_array[0] < aux_handle->average_temp_array[5]);
        // rise_count2 += (aux_handle->average_temp_array[1] < aux_handle->average_temp_array[6]);
        // rise_count2 += (aux_handle->average_temp_array[2] < aux_handle->average_temp_array[7]);

        LOGI("aux","[%s],rise_count1:%d,rise_count2:%d ",__func__, rise_count1, rise_count2);
        if(rise_count1 >= 6 || rise_count2 >= 3)
        {
            LOGI("aux","[%s]认为仍然处于缓慢上升状态 ",__func__);
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
            LOGI("aux","judge_water_boil: 判断水已煮开2 !!!");
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
    LOGI("aux", "煮模式: boil_status_change %s %d => %s %d", 
        boil_status_info[aux_handle->boil_current_tendency],
        aux_handle->boil_current_status_tick, 
        boil_status_info[aux_handle->boil_next_tendency], 
        aux_handle->boil_next_status_tick);

    //初始为空闲，则直接接收传入的新的状态
    if(aux_handle->boil_current_tendency == IDLE && aux_handle->boil_next_tendency != IDLE)
    {
        LOGI("aux", "煮模式: boil_status_change 1");
        aux_handle->boil_current_tendency    = aux_handle->boil_next_tendency;
        aux_handle->boil_current_status_tick = 0;
        aux_handle->boil_next_status_tick    = 0;
    }
    else if(aux_handle->boil_current_tendency != IDLE && aux_handle->boil_next_tendency == IDLE)
    {
        LOGI("aux", "煮模式: boil_status_change 2-0");
        if(aux_handle->boil_next_status_tick > 4 * 5)
        {
            LOGI("aux", "煮模式: boil_status_change 2-1");
            aux_handle->boil_current_tendency = aux_handle->boil_next_tendency;
            aux_handle->boil_current_status_tick = 0;
            aux_handle->boil_next_status_tick = 0;
        }
    }
    else if(aux_handle->boil_current_tendency == RISE && aux_handle->boil_next_tendency != RISE)
    {
        LOGI("aux", "煮模式: boil_status_change 3-0");
        if(aux_handle->boil_next_status_tick > 4 * 6)
        {
            LOGI("aux", "煮模式: boil_status_change 3-1");
            LOGI("aux","煮模式: boil_status_change 3: rise to %s ",boil_status_info[aux_handle->boil_next_tendency]);
            aux_handle->boil_current_tendency = aux_handle->boil_next_tendency;
            aux_handle->boil_current_status_tick = 0;
            aux_handle->boil_next_status_tick = 0;
        }
    }
    else if(aux_handle->boil_current_tendency == DOWN && aux_handle->boil_next_tendency != DOWN)
    {
        LOGI("aux", "煮模式: boil_status_change 4-0");
        if(aux_handle->boil_next_status_tick > 4 * 2)
        {
            LOGI("aux", "煮模式: boil_status_change 4-1");
            //第二次水开之前切换到了down都会停止计时，再次沸后计时。为了实现冷锅直接开火首次沸腾开始计时，或者首次放入食材后开始计时，后续再次放入食材不再进行计时。
            bool time_limit = true;
            if ((aux_handle->tick_first_boil != 0) && ((aos_now_ms() - aux_handle->tick_first_boil) > 5*60*1000) )
            {
                time_limit = false;
                LOGI("aux", "煮模式: 距离首次水开已经超过5分钟，不再重新倒计时");
            }

            if ((aux_handle->enter_boil_time <= 1) && time_limit)  //5分钟内并且首次下菜：重新倒计时; 其他情况：继续倒计时
            {
                aux_handle->aux_boil_counttime_flag = 0;    //停止计时
                aux_handle->aux_remain_time = aux_handle->aux_set_time * 60 * AUX_DATA_HZ;

                if(aux_remaintime_cb != NULL)
                {
                    int left_min = aux_handle->aux_remain_time / (60 * AUX_DATA_HZ);
                    LOGI("aux", "煮模式: 重置倒计时并且暂停计时, %d分钟",left_min);
                    //回调函数通知电控显示倒计时时间发生了变化，倒计时时间的单位是min
                    aux_remaintime_cb(left_min);
                }
            }
            aux_handle->boil_current_tendency    = aux_handle->boil_next_tendency;
            aux_handle->boil_current_status_tick = 0;
            aux_handle->boil_next_status_tick    = 0;
        }
    }
    else if(aux_handle->boil_current_tendency == GENTLE && aux_handle->boil_next_tendency != GENTLE)
    {
        LOGI("aux", "煮模式: boil_status_change 5-0");
        if(aux_handle->boil_next_status_tick > 4 * 8)
        {
            LOGI("aux", "煮模式: boil_status_change 5-1");
            aux_handle->boil_current_tendency = aux_handle->boil_next_tendency;
            aux_handle->boil_current_status_tick = 0;
            aux_handle->boil_next_status_tick = 0;
        }
    }
    else if(aux_handle->boil_current_tendency == BOILED && aux_handle->boil_next_tendency != BOILED)
    {     
        LOGI("aux", "煮模式: boil_status_change 6-0");   
        if(aux_handle->boil_next_tendency == GENTLE)
        {
            LOGI("aux", "煮模式: boil_status_change 6-1");
            LOGI("aux","要切换的是gentle，仍然保持boil状态");
        }
        //status change: BOIL->DOWN
        else if(aux_handle->boil_next_tendency == DOWN)
        {
            // //已经发生过水开的情况了
            // if(aux_handle->aux_boil_type == BOIL_20_MAX && aux_handle->aux_total_tick >= 10 * 60 * INPUT_DATA_HZ)
            // {
            //     LOGI("aux","总时间大于20min，处于10Min后，八段阀切换为大火3档 ");
            //     change_multivalve_gear(0x03, INPUT_RIGHT);
            // }
            // else
            //{
                LOGI("aux", "煮模式: boil_status_change 6-2");
                LOGI("aux","水沸状态切换到下降状态，八段阀切换为大火0档 ");
                change_multivalve_gear(0x00, INPUT_RIGHT);
            //}
            aux_handle->boil_current_tendency    = aux_handle->boil_next_tendency;
            aux_handle->boil_current_status_tick = 0; 
            aux_handle->boil_next_status_tick    = 0;

            aux_handle->put_food_cnt++;
            LOGI("aux","检测到放入食材 (累计第%d次)", aux_handle->put_food_cnt);

            if (aux_handle->put_food_cnt == 1)
            {
                udp_voice_write_sync("已放入食材,开始加热", strlen("已放入食材,开始加热"), 50);

                // if ((aux_handle->tick_first_boil != 0) && ((aos_now_ms() - aux_handle->tick_first_boil) < 5*60*1000) )
                // {
                //     udp_voice_write_sync("已放入食材,开始加热", strlen("已放入食材,开始加热"), 50);
                // }
                // else if ((aux_handle->tick_first_boil != 0) && ((aos_now_ms() - aux_handle->tick_first_boil) >= 5*60*1000) )
                // {
                //     int ms_done = aos_now_ms() - aux_handle->tick_first_boil;
                //     int s_left  = (aux_handle->aux_set_time * 60  * 1000  - ms_done)/1000;
                //     int m_left  = 0;
                //     if ((s_left%60) > 0)
                //         m_left = ((s_left/60) + 1);
                //     else
                //         m_left = (s_left/60);
                //     LOGI("aux", "煮模式: 水开后5分钟放入食材: %d %d %d %d", ms_done, s_left, m_left);                    
                //     char voice_buff[64] = {0x00};
                //     snprintf(voice_buff, sizeof(voice_buff), "开始%d分钟倒计时并关火", m_left);
                //     udp_voice_write_sync(voice_buff, strlen(voice_buff), 50);
                // }
            }
        }
        else if(aux_handle->boil_next_tendency == RISE)
        {
            LOGI("aux", "煮模式: boil_status_change 6-3-0");
            if( aux_handle->boil_next_status_tick > 5 * 10)
            {
                LOGI("aux", "煮模式: boil_status_change 6-3-1");
                aux_handle->boil_current_tendency = aux_handle->boil_next_tendency;
                aux_handle->boil_current_status_tick = 0;
                aux_handle->boil_next_status_tick = 0;
            }
        }
    }


    LOGI("aux", "煮模式: boil_status_change:  current=%s %d | next=%s %d", 
        boil_status_info[aux_handle->boil_current_tendency],
        aux_handle->boil_current_status_tick, 
        boil_status_info[aux_handle->boil_next_tendency], 
        aux_handle->boil_next_status_tick);
}


/**
 * @brief: 煮场景
 * @param {aux_handle_t} *aux_handle
 * @return {*}
 */
void mode_boil_func(aux_handle_t *aux_handle)
{
    //用于判断此次状态和上次状态之间是否发生了变化
    static int before_next_tendency = IDLE;

    // for (int j = 0; j < ARRAY_DATA_SIZE - 1; j++)
    // {
    //     int temp = aux_handle->average_temp_array[ARRAY_DATA_SIZE - 1] - aux_handle->average_temp_array[0];
    //     if(temp > aux_handle->max_up_value)
    //     {
    //         aux_handle->max_up_value = temp;
    //     }
    // }
    // LOGI("aux","max up value:%d", aux_handle->max_up_value);

    //do...while中状态判断逻辑
    do
    {
        //上升趋势========================================================
        if(aux_handle->temp_array[0] - aux_handle->temp_array[ARRAY_DATA_SIZE - 1] <= 0)
        {
            LOGI("aux","开始判断是否上升趋势");            
            
            //两两相邻数据之间的9次比较
            unsigned char very_gentle_count = 0;
            for(int i = 0; i < ARRAY_DATA_SIZE - 1; i++)
            {
                if(aux_handle->temp_array[i] == aux_handle->temp_array[i+1])
                {
                    very_gentle_count++;
                }
            }

            //两两间隔4个数据之间的5次比较
            unsigned char rise_count2 = 0;
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

            LOGI("aux", "判断是否上升 rise_count2=%d, very_gentle_count=%d, rise_count3=%d", rise_count2, very_gentle_count,  rise_count3);
            //rise_count3 = (aux_handle->average_temp_array[aux_handle->average_temp_size - 1] - aux_handle->average_temp_array[aux_handle->average_temp_size - 2] >= 10);
            //rise_count3 +=
            //仅仅靠10个温度难以判断是rise还是gentle
            //if((rise_count1 >= 7 && rise_count2 >= 4) ||
            //此处要防止误判为rise状态
            if((rise_count2 >= 4) || (rise_count3 >= 7 && very_gentle_count <= 7))
            {
                aux_handle->boil_next_tendency = RISE;
                LOGI("aux", "煮模式: 判断趋势 = 上升");
                break;
            }
        }
        else
        {
            LOGI("aux","开始判断是否下降趋势");
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
                LOGI("aux", "煮模式: 判断趋势 = 下降");
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
            //         LOGI("aux","[%s]present state is down ",__func__);
            //         break;
            //     }
            // }

        }

        //平缓趋势======================================================================
        unsigned char gentle_count1 = 0;
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

        LOGI("aux", "判断是否平缓 gentle_count1=%d, abs(before_average - after_average)=%d, ", gentle_count1, abs(before_average - after_average));
        if( gentle_count1 == 9 || abs(before_average - after_average) < 15)
        {
            aux_handle->boil_next_tendency = GENTLE;
            LOGI("aux", "煮模式: 判断趋势 = 平缓");
            break;
        }

        aux_handle->boil_next_tendency = IDLE;
        LOGI("aux", "煮模式: 判断趋势 = 空闲 (因为无法判断趋势,继续保持空闲)");
        break;
    }while(1);

    //待切换状态变化，则计时清零，开始对新状态进行计时：煮模式启动时，初始条件是两者相同都是IDLE，如果boil_next_tendency突然变化了，就会触发赋值操作
    LOGI("aux","煮模式: 本次判断后最新状态(last=%s next=%s)", 
        boil_status_info[before_next_tendency],
        boil_status_info[aux_handle->boil_next_tendency]);
    if(aux_handle->boil_next_tendency != before_next_tendency)
    {
        before_next_tendency = aux_handle->boil_next_tendency;
        aux_handle->boil_next_status_tick = 0;
        LOGI("aux", "煮模式: boil_next_tendency状态更新 (%d %d)", before_next_tendency, aux_handle->boil_next_tendency);
    }    
    else //状态未发生变化，继续对next_status_tick计时
    {
        aux_handle->boil_next_status_tick++;
        LOGI("aux", "煮模式: boil_next_tendency状态不变 (%d %d %d)", before_next_tendency, aux_handle->boil_next_tendency, aux_handle->boil_next_status_tick);        
    }

    boil_status_change(aux_handle);

    int ret = 0;
    //Gentle中判断水开
    if(aux_handle->boil_current_tendency == GENTLE && aux_handle->current_average_temp >50 * 10)
    {
        ret = judge_water_boil(aux_handle);
    }
    else if(aux_handle->boil_current_tendency == RISE)
    {
        unsigned char very_gentle_count = 0;
        for(int j = 0; j < ARRAY_DATA_SIZE -1; j++)
        {
            if(aux_handle->temp_array[j] == aux_handle->temp_array[j+1])
            {
                very_gentle_count++;
            }
        }

        if(very_gentle_count == ARRAY_DATA_SIZE - 1)
        {
            ret = judge_water_boil(aux_handle);
        }
    }

    if(ret)
    {
        aux_handle->boil_current_tendency = BOILED;
        aux_handle->enter_boil_time++; //进入水开次数累加
        LOGI("aux", "煮模式: 切换到水开状态 (累计第%d次水开)", aux_handle->enter_boil_time);
        if (aux_handle->enter_boil_time == 1)
        {
            aux_handle->tick_first_boil = aos_now_ms();
            LOGI("aux", "煮模式: 检测到首次水开");
        }

        // //前两次进入水沸状态都重新倒计时，这里对当前倒计时进行清零。对应的是冷水放食材直接煮开到结束，或者冷水煮开后再放食材煮沸后到结束
        // if(aux_handle->enter_boil_time <= 2)
        // {
        //     aux_handle->aux_remain_time = aux_handle->aux_set_time * 60 * AUX_DATA_HZ;
        // }

        if (aux_handle->aux_boil_counttime_flag == 0)
        {
            aux_handle->aux_boil_counttime_flag = 1;
            LOGI("aux", "煮模式: 开始倒计时");
        }
        aux_handle->aux_boil_counttime_flag = 1;        //开启倒计时

        if(aux_handle->enter_boil_time == 1)
        {
            //首次水开进行蜂鸣
            if(beep_control_cb != NULL)
            {
                beep_control_cb(0x02);

                char voice_buff[64] = {0x00};
                snprintf(voice_buff, sizeof(voice_buff), "水已烧开,开始%d分钟倒计时", aux_handle->aux_set_time);
                udp_voice_write_sync(voice_buff, strlen(voice_buff), 50);
            }

            //首次水开全部调为3档
            change_multivalve_gear(0x03, INPUT_RIGHT);
        }
        //非首次水开
        else
        {
            if (aux_handle->enter_boil_time == 2)
            {                
                if ((aux_handle->tick_first_boil != 0) && ((aos_now_ms() - aux_handle->tick_first_boil) >= 5*60*1000) )
                {
                    int ms_done = aos_now_ms() - aux_handle->tick_first_boil;
                    int s_left  = (aux_handle->aux_set_time * 60  * 1000  - ms_done)/1000;
                    int m_left  = 0;
                    if ((s_left%60) > 0)
                        m_left = ((s_left/60) + 1);
                    else
                        m_left = (s_left/60);
                    LOGI("aux", "煮模式: 水开后5分钟放入食材: %d %d %d %d", ms_done, s_left, m_left);                    
                    char voice_buff[64] = {0x00};
                    snprintf(voice_buff, sizeof(voice_buff), "开始%d分钟倒计时并关火", m_left);
                    udp_voice_write_sync(voice_buff, strlen(voice_buff), 50);
                }
                else
                {                   
                    char voice_buff[64] = {0x00};
                    snprintf(voice_buff, sizeof(voice_buff), "开始%d分钟倒计时关火", aux_handle->aux_set_time);
                    udp_voice_write_sync(voice_buff, strlen(voice_buff), 50);
                }
            }

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

    //水开和温度down之外的特殊处理
    //如果煮模式设置时间大于20min,且当前处于10min后，且处于沸腾状态（如果是非沸腾状态需要设置为1档加热至沸腾），档位非6档，切换档位到6档位
    if(aux_handle->aux_boil_type == BOIL_20_MAX && aux_handle->boil_current_tendency == BOILED && \
    aux_handle->aux_total_tick >= 10 * 60 * INPUT_DATA_HZ && aux_handle->aux_multivalve_gear != 0x06)
    {
        change_multivalve_gear(0x06, INPUT_RIGHT);
    }

    //剩余倒计时开始倒计时了，且整分钟的发生变化，进行发送剩余分钟倒计时给电控
    if(aux_handle->aux_remain_time != aux_handle->aux_set_time * 60 * INPUT_DATA_HZ && aux_handle->aux_remain_time % (60 * AUX_DATA_HZ) == 0)
    {
        if(aux_remaintime_cb != NULL)
        {
            int left_min = aux_handle->aux_remain_time / (60 * AUX_DATA_HZ);
            LOGI("aux","ready send left min to dis,left_min:%d",left_min);
            //回调函数通知电控显示倒计时时间发生了变化，倒计时时间的单位是min
            if(left_min != 0)
                aux_remaintime_cb(left_min);
        }
    }

    //aux暂时设置，enter_boil_time>1之后直接执行aux_handle->aux_remain_time--;即可，不需要再次区分，上方进入水沸的时候会判断是否需要清空
    if(aux_handle->aux_boil_counttime_flag == 1)
    {        
        if(aux_handle->aux_remain_time > 0)
        {
            aux_handle->aux_remain_time--;
        }
        LOGI("aux","倒计时进行中......(%d)", aux_handle->aux_remain_time);

        /*
        if(aux_handle->enter_boil_time == 1)
        {
            LOGI("aux","首次沸腾，开始倒计时 ");
        }
        else if(aux_handle->enter_boil_time == 2)
        {
            LOGI("aux","第二次沸腾，开始倒计时 ");
        }
        else if(aux_handle->enter_boil_time > 2)
        {
            //后续进入沸腾仍然按照之前的倒计时进行
            LOGI("aux","后续沸腾，继续计时 ");
        }
        */
    }
    else
    {
        LOGI("aux","倒计时已经停止！");
    }


    if(aux_handle->aux_remain_time == 40 && aux_handle->boil_current_tendency == BOILED)
    {
        udp_voice_write_sync("请注意: 10秒后即将关火", strlen("请注意: 10秒后即将关火"), 50);
    }

    //煮模式计时结束且当前处于沸腾状态，关火，通知电控模式成功退出
    if(aux_handle->aux_remain_time == 0 && aux_handle->boil_current_tendency == BOILED)
    {
        LOGI("aux","boil count down time is on,close fire[boil success end!!!] ");
        if(aux_close_fire_cb != NULL)
        {
            aux_close_fire_cb(INPUT_RIGHT);             //右灶关火
        }

        aux_handle->aux_switch = 0;                 //辅助烹饪煮模式结束
        if(aux_exit_cb != NULL)
        {
            udp_voice_write_sync("定时时间到", strlen("定时时间到"), 50);
            aux_exit_cb(AUX_SUCCESS_EXIT);              //成功退出
        }

        LOGW("aux","煮模式成功执行完成,八段阀复位,boil mode run finish,multi valve reset");
        change_multivalve_gear(0x00, INPUT_RIGHT);
        cook_aux_reinit(INPUT_RIGHT);
    }
    else if(aux_handle->aux_remain_time <= 2 * 60 * INPUT_DATA_HZ && aux_handle->aux_multivalve_gear != 0x01)
    {
        //LOGW("aux","最后两分钟切换为1档火焰,last 2 min valve change to gear 1");
        //change_multivalve_gear(0x01, INPUT_RIGHT);
    }

    aux_handle->boil_current_status_tick++;

    LOGI("aux","boil_remain_time:%d,  enter_boil_time:%d ",aux_handle->aux_remain_time, aux_handle->enter_boil_time);
    LOGI("aux","total_tick:%d,   current status:[%s], current tick:%d,   next status:%s, next tick:%d,   current gear:%d ",
        aux_handle->aux_total_tick,
        boil_status_info[aux_handle->boil_current_tendency ],aux_handle->boil_current_status_tick,
        boil_status_info[aux_handle->boil_next_tendency],aux_handle->boil_next_status_tick,
        aux_handle->aux_multivalve_gear);
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
    aux_handle->fry_last_change_gear_tick++;

    static uint64_t time_pan_remind     = 0;  //热锅阶段: 温度到达时刻
    static uint64_t time_pan_warn_1     = 0;  //热锅阶段: 干烧逻辑·第一次警告的时刻
    static uint64_t time_pan_warn_2     = 0;  //热锅阶段: 干烧逻辑·第二次警告的时刻

    static uint64_t time_oil_remind     = 0;  //控温阶段: 温度到达时刻
    static uint64_t time_up_trend_burn  = 0;  //控温阶段: 无人开始时刻(焦糊逻辑)
    static uint64_t time_up_trend_dry   = 0;  //控温阶段: 无人开始时刻(干烧逻辑)
    static uint64_t time_oil_warn_1     = 0;  //控温阶段: 干烧逻辑·第一次警告的时间
    static uint64_t time_oil_warn_2     = 0;  //控温阶段: 干烧逻辑·第二次警告的时间  
    
    static bool temp_arrive_flag = false;
    static bool food_put_flag    = false;

    if (aux_handle->aux_total_tick == 10)  //开启炸模式后的第一个温度
    {
        //change_multivalve_gear(0x01, INPUT_RIGHT);
        LOGI("aux","mode_fry_func: enter fry mode temp: %d %d ", aux_handle->aux_total_tick, aux_handle->current_average_temp);
        enter_mode_temp = aux_handle->current_average_temp;
        aux_handle->fry_step = 0;               //重置炸场景步骤

        time_pan_remind     = 0;
        time_pan_warn_1     = 0;
        time_pan_warn_2     = 0;

        time_oil_remind     = 0;
        time_up_trend_burn  = 0;
        time_up_trend_dry   = 0;
        time_oil_warn_1     = 0;
        time_oil_warn_2     = 0;

        temp_arrive_flag    = false;
        food_put_flag       = false;
    }

    //进入炸模式后，如果当前步骤还未确定，且处于进入模式的4s-30s内，则判断是否处于热锅状态
    if (aux_handle->fry_step == 0)
    {
        unsigned char gentle_count = 0;
        unsigned char rise_count = 0;
        //首先判断10组温度没有发生大的跳变，处于较为稳定的状态
        for (int i = 0; i < ARRAY_DATA_SIZE - 1; i++)
        {
            //相邻的温度，新的温度大于旧的温度，新的温度大于旧的温度的幅度小于等于2度
            if (aux_handle->temp_array[i + 1] >= aux_handle->temp_array[i] && 
               aux_handle->temp_array[i + 1] - aux_handle->temp_array[i] <= 20)
            {
                gentle_count++;
            }
        }

        //温度处于较为平稳的阶段，才能判断是否处于稳定上升的阶段
        if (gentle_count >= 9)
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

        if (aux_handle->aux_total_tick <= 4 * 5)
        {
            //LOGI("aux","初始阶段，5s内不做处理 ");
        }
        else if (aux_handle->aux_total_tick > 4 * 5 && aux_handle->aux_total_tick <= 4 * 30)
        {
            //判断是否处于热锅阶段
            if (rise_count >= 6)
            {
                aux_handle->rise_quick_tick++;
            }
            else if (rise_count < 2)
            {
                aux_handle->rise_slow_tick++;
            }
            else        //如果都不满足暂时不判断
            {
                //如果30s即将过去仍然判断不到是热油还是热锅，直接默认为热油阶段，准备开始控温逻辑
                if (aux_handle->aux_total_tick == 4 * 30)
                {
                    LOGI("aux","炸模式(%d): 30s没有检测到处于何种阶段，直接设定为热油阶段", aux_handle->fry_step);
                    aux_handle->fry_step = 2;
                }
            }
        }

        //连续3s都判断为此状态，因为10组温度数据代表的是2.5s内的温度数据，3s仍为此状态比较稳妥的判断当前的步骤
        if (aux_handle->rise_quick_tick >= 3 * AUX_DATA_HZ)
        {
            LOGI("aux","炸模式(%d): 检测到处于热锅阶段(%d)", aux_handle->fry_step, aux_handle->rise_quick_tick);
            udp_voice_write_sync("开始热锅", strlen("开始热锅"), 50);
            aux_handle->fry_step = 1;
        }
        else if (aux_handle->rise_slow_tick >= 3 * AUX_DATA_HZ)
        {
            LOGI("aux","炸模式(%d): 检测到处于热油阶段(%d)", aux_handle->fry_step, aux_handle->rise_slow_tick);
            udp_voice_write_sync("开始热油", strlen("开始热油"), 50);
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
    if(aux_handle->fry_step == 1 && gentle_flag == true)  //调小火
    {
        LOGI("aux", "炸模式(%d热锅): %d", aux_handle->fry_step, aux_handle->current_average_temp);
        if(aux_handle->current_average_temp >= 160 * 10 && aux_handle->current_average_temp <= 180 * 10)
        {
            change_multivalve_gear(0x04, INPUT_RIGHT);
            //aux_handle->fry_last_change_gear_tick = 0;
        }
        else if(aux_handle->current_average_temp > 180 * 10)
        {
            change_multivalve_gear(0x07, INPUT_RIGHT);
            //aux_handle->fry_last_change_gear_tick = 0;
            if (aux_handle->first_hot_pot_flag == 0)
            {
                aux_handle->first_hot_pot_flag = 1;
                time_pan_remind = aos_now_ms();
                if(beep_control_cb != NULL)
                {
                    beep_control_cb(0x02);          //提醒用户加油
                    udp_voice_write_sync("请到油", strlen("请到油"), 50);
                }
            }        
        }
    }

    //LOGI("aux", "炸模式(%d %d %d %d)", aux_handle->fry_step, aux_handle->first_put_food_flag, (int)time_pan_remind, (int)time_pan_warn_1);
    //已经检测到热锅之后或者已经超过了30s,开始判断是否放入油或食材;即默认热锅最多30s
    if ((aux_handle->fry_step == 1  /* || aux_handle->aux_total_tick > 30 * 4*/) && aux_handle->first_put_food_flag == 0)
    {
        if (time_pan_remind != 0)
        {
            if (time_pan_warn_1 == 0)
            {
                if ((aos_now_ms() - time_pan_remind) >= 60*1000)
                {
                    LOGI("aux", "防干烧60秒时间到,调7档");
                    udp_voice_write_sync("防止干烧,切换小火", strlen("防止干烧,切换小火"), 50);
                    change_multivalve_gear(0x07, INPUT_RIGHT);
                    time_pan_warn_1 = aos_now_ms();
                    beep_control_cb(0x02);
                }    
            }
            else
            {
                if (time_pan_warn_2 == 0)
                {
                    if ((aos_now_ms() - time_pan_warn_1) >= 120*1000)
                    {
                        udp_voice_write_sync("防止干烧,关闭右灶", strlen("防止干烧,关闭右灶"), 50);
                        beep_control_cb(0x02);  
                        aux_close_fire_cb(INPUT_RIGHT);             
                        time_pan_warn_2 = aos_now_ms();    
    
                        aux_handle->aux_switch = 0;       
                        if(aux_exit_cb != NULL)
                        {
                            aux_exit_cb(AUX_SUCCESS_EXIT);
                        }    
                    }    
                }    
            }
        }

        //下油检测逻辑1：0.25s相邻的两个温度发生大于5度的波动
        // if(abs(aux_handle->temp_array[ARRAY_DATA_SIZE - 1] - aux_handle->temp_array[ARRAY_DATA_SIZE - 2]) > 50)
        // {
        //     aux_handle->fry_step = 2;              //进入到了控温步骤
        //     aux_handle->first_put_food_flag = 1;
        //     udp_voice_write_sync("开始热油", strlen("开始热油"), 50);
        //     LOGI("aux","判断可能是放入了食用油或食材 %d", abs(aux_handle->temp_array[ARRAY_DATA_SIZE - 1] - aux_handle->temp_array[ARRAY_DATA_SIZE - 2]));
            
        //     time_pan_warn_1 = 0;
        //     time_pan_warn_2 = 0;
        // } 
        //下油检测逻辑2：
        int diff = ( (int)(aux_handle->temp_array[0]) - (int)(aux_handle->temp_array[ARRAY_DATA_SIZE - 1]) );
        bool res = judge_cook_trend_down(aux_handle->temp_array, ARRAY_DATA_SIZE, 4);  //4组温度是否合理? 
        if (res && (diff >= 40*10))
        {  
            aux_handle->fry_step = 2;              //进入到了控温步骤
            aux_handle->first_put_food_flag = 1;
            udp_voice_write_sync("开始热油", strlen("开始热油"), 50);
            LOGI("aux","热锅阶段下油: %d %d %d", aux_handle->temp_array[0], aux_handle->temp_array[ARRAY_DATA_SIZE - 1], diff);
            
            time_pan_warn_1 = 0;
            time_pan_warn_2 = 0;
        } 
    }

    //进入控温逻辑，当温度处于跳动状态的时候无法进行控温操作
    if(aux_handle->fry_step == 2 && gentle_flag == true)
    {
        if(aux_handle->current_average_temp <= (aux_handle->aux_set_temp * 10 - 15 * 10))
        {
            LOGI("aux", "炸模式(%d热油-快速升温): %d, %d [距离15以上]", aux_handle->fry_step, aux_handle->current_average_temp/10, aux_handle->aux_set_temp);
            if(aux_handle->aux_multivalve_gear != 0 && aux_handle->fry_last_change_gear_tick >= 10 * 4)
            {
                aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;      //记录调档之前的平均温度

                LOGI("aux", "当前温度 < (目标温度-15),调0档");
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
        else if(aux_handle->current_average_temp > aux_handle->aux_set_temp * 10 - 20 * 10)
        {
            LOGI("aux", "炸模式(%d热油-初步控温): %d, %d [距离15]", aux_handle->fry_step, aux_handle->current_average_temp/10, aux_handle->aux_set_temp);
            if(aux_handle->aux_multivalve_gear != 0x03 && aux_handle->fry_last_change_gear_tick >= 10 * 4)
            {
                aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;
                LOGI("aux", "当前温度 > (目标温度-20),调3档");
                change_multivalve_gear(0x03, INPUT_RIGHT);
                aux_handle->fry_last_change_gear_tick = 0;
                aux_handle->fry_step = 3;
                LOGI("aux", "开始进入精细控温阶段");
            }
        }
    }

    if(aux_handle->fry_step == 3 && gentle_flag == true)
    {
        //距离目标温度-5度的时候开始降档，提前减速，后续方便控温
        if(aux_handle->current_average_temp >= aux_handle->aux_set_temp * 10 - 10 * 10)
        {
            LOGI("aux", "炸模式(%d热油-初步控温): %d, %d [距离10]", aux_handle->fry_step, aux_handle->current_average_temp/10, aux_handle->aux_set_temp);
            // //上次调火4s后
            // unsigned char gear;
            // //判断调档后温度下降了
            // if(aux_handle->current_average_temp <=  aux_handle->fry_last_gear_average_temp)
            // {
            //     if(aux_handle->fry_last_change_gear_tick >= 15 * 4)
            //     {
            //         gear = aux_handle->aux_multivalve_gear - 1;
            //         aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;
            //         change_multivalve_gear(gear, INPUT_RIGHT);
            //         aux_handle->fry_last_change_gear_tick = 0;
            //     }
            //     else if(aux_handle->fry_last_change_gear_tick >= 10 * 4)
            //     {
            //         gear = aux_handle->aux_multivalve_gear - 2;
            //         aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;
            //         change_multivalve_gear(gear, INPUT_RIGHT);
            //         aux_handle->fry_last_change_gear_tick = 0;
            //     }
            // }



            // if(aux_handle->aux_multivalve_gear != 0x06 && aux_handle->fry_last_change_gear_tick >= 10 * 4)
            // {
            //     aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;

            //     change_multivalve_gear(0x06, INPUT_RIGHT);
            //     aux_handle->fry_last_change_gear_tick = 0;
            //     aux_handle->fry_step = 3;
            // }
            //应该在此处判断斜率，然后根据斜率进行调档
            int temp = aux_handle->average_temp_array[ARRAY_DATA_SIZE - 1] - aux_handle->average_temp_array[0];
            if(temp > aux_handle->max_up_value)
            {
                aux_handle->max_up_value = temp;
            }
            LOGI("aux","max up value:%d ",aux_handle->max_up_value);

            //这里间隔时间设置为5s
            if(aux_handle->fry_last_change_gear_tick >= 5 * 4)
            {
                if(aux_handle->aux_multivalve_gear != 0x07 && aux_handle->max_up_value > 70)
                {
                    aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;
                    change_multivalve_gear(0x07, INPUT_RIGHT);
                    aux_handle->fry_last_change_gear_tick = 0;
                    aux_handle->fry_step = 4;
                }

                if(aux_handle->aux_multivalve_gear != 0x06 && aux_handle->max_up_value <= 70 && aux_handle->max_up_value > 30)
                {
                    aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;
                    change_multivalve_gear(0x06, INPUT_RIGHT);
                    aux_handle->fry_last_change_gear_tick = 0;
                    aux_handle->fry_step = 4;
                }

                if(aux_handle->aux_multivalve_gear != 0x05 && aux_handle->max_up_value <= 30)
                {
                    aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;
                    change_multivalve_gear(0x05, INPUT_RIGHT);
                    aux_handle->fry_last_change_gear_tick = 0;
                    aux_handle->fry_step = 4;
                }
            }
        }
    }

    if(aux_handle->fry_step >= 3 && aux_handle->fry_step <= 4 && aux_handle->first_reach_set_temp_flag == 0)
    {
        if(aux_handle->current_average_temp >= aux_handle->aux_set_temp * 10)
        {
            LOGI("aux","首次达到设定的目标温度，current_average_temp:%d,set_temp:%d",aux_handle->current_average_temp, aux_handle->aux_set_temp * 10);
            beep_control_cb(0x02);
            udp_voice_write_sync("请放入食材", strlen("请放入食材"), 50);
            aux_handle->first_reach_set_temp_flag = 1;
            time_oil_remind = aos_now_ms();

            temp_arrive_flag = true;
        }
    }

    if (food_put_flag) //((aux_handle->fry_step == 4) && (aux_handle->first_reach_set_temp_flag == 1))
    {
        bool exist_flag = check_any_large_diff(aux_handle->temp_array, ARRAY_DATA_SIZE);    
        if (!exist_flag)  //无人存在
        {
            if (time_up_trend_burn == 0)
            {
                time_up_trend_burn = aos_now_ms();//首次进入温度上升的时间
                LOGI("aux","炸模式: (焦糊逻辑)无人阶段开始...... ");
            }
            else
            {
                int time_diff = (aos_now_ms() - time_up_trend_burn);
                if (time_diff > 3*60*1000)
                {
                    udp_voice_write_sync("请搅拌,注意焦糊", strlen("请搅拌,注意焦糊"), 50);
                    beep_control_cb(0x02);
                    time_up_trend_burn = 0;
                    LOGI("aux","炸模式: 这里是焦糊提醒");
                }
            }
        }
        else
        {
            time_up_trend_burn = 0;
        }
    }

    if (temp_arrive_flag || food_put_flag) //((aux_handle->fry_step == 4) && (aux_handle->first_reach_set_temp_flag == 1))
    {
        bool body_exist_flag = check_any_large_diff(aux_handle->temp_array, ARRAY_DATA_SIZE);    
        if (!body_exist_flag)  //无人存在
        {
            if (time_up_trend_dry == 0)
            {
                time_up_trend_dry = aos_now_ms();//首次进入温度上升的时间
                LOGI("aux","炸模式: (干烧逻辑)无人阶段开始...... ");
            }
            else
            {
                int time_diff = (aos_now_ms() - time_up_trend_dry);
                int valut_limit = 0;
                if (food_put_flag) 
                    valut_limit = 5*60*1000;
                else 
                    valut_limit = 2*60*1000;
                if (time_diff > valut_limit)
                {
                    if (time_pan_warn_1 == 0)
                    {
                        //udp_voice_write_sync("防止干烧,切换小火", strlen("防止干烧,切换小火"), 50);
                        beep_control_cb(0x02);  
                        change_multivalve_gear(0x07, INPUT_RIGHT); //火力7档  
                        time_pan_warn_1 = aos_now_ms();
                        LOGI("aux","炸模式: 干烧提醒1");
                        udp_voice_write_sync("为防止干烧,切换小火", strlen("为防止干烧,切换小火"), 50);
                    }
                    else
                    {
                        if (time_pan_warn_2 == 0)
                        {
                            if ((aos_now_ms() - time_pan_warn_1) > valut_limit)
                            {
                                //udp_voice_write_sync("防止干烧,关闭右灶", strlen("防止干烧,关闭右灶"), 50);
                                beep_control_cb(0x02);  
                                aux_close_fire_cb(INPUT_RIGHT);
                                time_pan_warn_2 = aos_now_ms();
                                LOGI("aux","炸模式: 干烧提醒2");
                                udp_voice_write_sync("为防止干烧,关闭右灶", strlen("为防止干烧,关闭右灶"), 50);
                    
                                aux_handle->aux_switch = 0;                 
                                if(aux_exit_cb != NULL)
                                {
                                    aux_exit_cb(AUX_SUCCESS_EXIT);
                                }   
                            }         
                        }
                    }
                }
            }
        }
        else
        {
            time_up_trend_dry = 0;
            time_pan_warn_1   = 0;
            time_pan_warn_2   = 0;
        }
    }

    // if ((aux_handle->fry_step == 3 || aux_handle->fry_step == 4) && (aux_handle->first_reach_set_temp_flag == 1))
    // {
    //     if (time_oil_remind != 0)
    //     {
    //         if (time_pan_warn_1 == 0)
    //         {
    //             if ((aos_now_ms() - time_pan_remind) >= 6*60*1000)
    //             {
    //                 change_multivalve_gear(0x07, INPUT_RIGHT);
    //                 time_pan_warn_1 = aos_now_ms();
    //                 beep_control_cb(0x02); 
    //             }    
    //         }
    //         else
    //         {
    //             if (time_pan_warn_2 == 0)
    //             {
    //                 if ((aos_now_ms() - time_pan_warn_1) >= 8*60*1000)
    //                 {
    //                     beep_control_cb(0x02);  
    //                     aux_close_fire_cb(INPUT_RIGHT);             
    //                     time_pan_warn_2 = aos_now_ms();    
    
    //                     aux_handle->aux_switch = 0;       
    //                     if(aux_exit_cb != NULL)
    //                     {
    //                         aux_exit_cb(AUX_SUCCESS_EXIT);
    //                     }    
    //                 }    
    //             }    
    //         }
    //     }
    // }

    if(aux_handle->fry_step == 4 && gentle_flag == true)
    {
        if (time_pan_warn_1 != 0)
        {
            LOGI("aux", "炸模式:防干烧阶段,不再调火");
            return;
        }

        if(aux_handle->fry_last_change_gear_tick >= 10 * 4 && aux_handle->current_average_temp >= aux_handle->aux_set_temp * 10 - 5 * 10)
        {
            LOGI("aux", "炸模式(%d热油-进步控温): %d, %d[距离5]", aux_handle->fry_step, aux_handle->current_average_temp/10, aux_handle->aux_set_temp);
            unsigned char gear = 0;
            //温度高于目标温度5摄氏度以上，降低火力操作（档位越大火力越小）
            if(aux_handle->current_average_temp >= aux_handle->aux_set_temp * 10 + 10 * 5)
            {
                gear = 7; //aux_handle->aux_multivalve_gear + 3;
                LOGI("aux","控温阶段: 修正火力档位 %d 档 (x+3) ", gear);
            }
            else if(aux_handle->current_average_temp >= aux_handle->aux_set_temp * 10)
            {
                gear = 6; //aux_handle->aux_multivalve_gear + 2;
                LOGI("aux","控温阶段: 修正火力档位 %d 档 (x+2) ", gear);
            }
            else if(aux_handle->current_average_temp >= aux_handle->aux_set_temp * 10 - 5 * 10)
            {
                gear = 5; //aux_handle->aux_multivalve_gear + 1;
                LOGI("aux","控温阶段: 修正火力档位 %d 档 (x+1) ", gear);
            }
            aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;
            change_multivalve_gear(gear, INPUT_RIGHT);
            aux_handle->fry_last_change_gear_tick = 0;

        }
        else if(aux_handle->fry_last_change_gear_tick >= 10 * 4 && aux_handle->current_average_temp < aux_handle->aux_set_temp * 10 - 5 * 10)
        {
            //温度高于目标温度10摄氏度以上，增大火力操作（档位越小火力越大）
            // unsigned char gear = aux_handle->aux_multivalve_gear - 1;
            // LOGI("aux","控温阶段: 修正火力档位 %d 档 (x-1) ", gear);
            // aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;
            // change_multivalve_gear(gear, INPUT_RIGHT);
            // aux_handle->fry_last_change_gear_tick = 0;

            aux_handle->fry_step = 2;
            aux_handle->first_reach_set_temp_flag = 0;
            time_up_trend_burn = 0;
            time_up_trend_dry  = 0;
            time_pan_warn_1    = 0;
            time_pan_warn_2    = 0;

            if (aux_handle->current_average_temp < aux_handle->aux_set_temp * 10 - 20 * 10)
            {
                food_put_flag = true;
                LOGI("aux","检测到放入食物");
            }
        }
    }

    LOGI("aux","current step:%d,current gear:%d ",aux_handle->fry_step, aux_handle->aux_multivalve_gear);
    return;
}

void chao_heat_pan_oil_onion(func_ptr_fsm_t* fsm, aux_handle_t *aux_handle)
{
    static temp_value_t temp_value_last = {0x00};
    uint16_t temp_cur = aux_handle->temp_array[ARRAY_DATA_SIZE - 1];            // 最新温度
    uint16_t temp_mid = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);    // 中位温度
    uint16_t temp_min = findMin(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最小温度
    uint16_t temp_max = findMax(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最大温度

    static bool temp_arrivate     = false;
    static uint64_t time_remind   = 0;  //温度到达时间
    
    static uint64_t time_dn_trend = 0;  //下降趋势的起始时间
    static uint16_t temp_dn_trend = 0;  //下降趋势的最低温度
    static uint16_t temp_up_trend = 0;  //下降趋势的最高温度

    static uint64_t time_warn_1     = 0;    //第一次警告的时间
    static uint64_t time_warn_2     = 0;    //第二次警告的时间

    static int slope_cnt            = 0;

    if (fsm->state_time == 0)
    {
        set_fsm_time(fsm);
        temp_value_last = save_current_temp(temp_mid);

        if (aux_handle->fsm_chao_loop_cnt == 0)
            udp_voice_write_sync("开始热锅热油", strlen("开始热锅热油"), 50);
        else
            udp_voice_write_sync("再次热锅热油", strlen("再次热锅热油"), 50);
        LOGI("aux","炒模式(%d): 进入热锅", aux_handle->fsm_chao_loop_cnt);


        temp_arrivate   = false;       
        time_remind     = 0;    //温度到达时间   

        time_dn_trend   = 0;    //下降趋势的起始时间
        temp_dn_trend   = 0;    //下降趋势的最低温度
        temp_up_trend   = 0;      
        
        time_warn_1     = 0;
        time_warn_2     = 0;

        slope_cnt       = 0;
    }

    double slope;
    bool   slope_flag = false;
    if ((aos_now_ms() - temp_value_last.time) >= (3*1000))  //每隔3秒钟就判断1次是否切换状态（如果用户下油了，就切换到热油状态）
    {        
        temp_value_t temp_value_now = 
        {
            .time = aos_now_ms(),
            .temp = temp_mid
        };

        slope = calculateSlope(temp_value_last, temp_value_now);
        slope_cnt++;
        slope_flag = true;
        LOGI("aux", "炒模式(%d): *****判断斜率(%d)***** (%d %d) ===> (%d %d) [斜率=%f · 最新温度=%d · 中位数温度=%d]", 
            aux_handle->fsm_chao_loop_cnt, 
            slope_cnt,
            (int)(temp_value_last.time/1000), temp_value_last.temp, 
            (int)(temp_value_now.time/1000),  temp_value_now.temp, 
            slope, temp_cur, temp_mid);

        temp_value_last = temp_value_now;
    }

    uint16_t temp_low  = 200*10;
    uint16_t temp_high = 210*10;
    uint8_t  gear_low  = 1;
    uint8_t  gear_high = 4;
    if (aux_handle->fsm_chao_loop_cnt >= 2)
    {
        temp_low  = 120*10;
        temp_high = 130*10;
        gear_low  = 5;
        gear_high = 7;
    }

    if (temp_cur < temp_low)
    {
        if (!temp_arrivate)
        {
            change_multivalve_gear(gear_low, INPUT_RIGHT);
        }
        else
        {
            if (time_warn_1 == 0)
            {            
                //if (temp_cur < (temp_low-200))
                {
                    change_multivalve_gear(0x05, INPUT_RIGHT);
                }
            }
        }
    }
    else if (temp_cur < temp_high)
    {
        if (!temp_arrivate)
        {
            change_multivalve_gear(gear_high, INPUT_RIGHT);
        }
        else
        {
            if (time_warn_1 == 0)
            {
                if (temp_cur < (temp_high - 200))
                {
                    change_multivalve_gear(gear_high, INPUT_RIGHT);
                }
            }
        }

        if (!temp_arrivate)
        {
            temp_arrivate = true;
            time_remind   = aos_now_ms();
            udp_voice_write_sync("请操作", strlen("请操作"), 50);
            beep_control_cb(0x02);            
        }
    }
    else
    {
        change_multivalve_gear(0x07, INPUT_RIGHT);

        if (!temp_arrivate)
        {
            temp_arrivate = true;
            time_remind   = aos_now_ms();
            udp_voice_write_sync("请操作", strlen("请操作"), 50);
            beep_control_cb(0x02);            
        }
    }

    //情况一：锅里本来就有食物(前6秒就能判断出来升温慢，然后切入到煎食物阶段)  
    if (aux_handle->fsm_chao_loop_cnt == 0)
    {
        if (!is_fsm_timeout(fsm, 6500))  //锅里有菜,直接跳转到热菜阶段
        {
            if (slope_flag && slope < 10.0)
            {
                LOGI("aux", "炒模式(%d): 起始温升慢 ⇨ 炒食材", aux_handle->fsm_chao_loop_cnt);
                beep_control_cb(0x02);  
                switch_fsm_state(fsm, chao_heat_food);
            }
        }
    } 

    /*
    1. 不管温度是否到过190度,用户放油或者葱姜蒜了,那么程序就主动切换到热油、爆香去（温度都是先下降后上升）
    2. 如果用户直接放菜了,那么程序就主动切换到热菜去（温度先下降后上升）
    */
    if (time_dn_trend == 0)
    {  
        LOGI("aux","炒模式(%d): wait time_dn_trend 3", aux_handle->fsm_chao_loop_cnt);      
        int diff = ( (int)(aux_handle->temp_array[0]) - (int)(aux_handle->temp_array[ARRAY_DATA_SIZE - 1]) );
        bool res = judge_cook_trend_down(aux_handle->temp_array, ARRAY_DATA_SIZE, 3);
        if (res && (diff >= 30*10))
        {  
            LOGI("aux", "炒模式(%d): 检测到温度骤降 ↓↓↓ (温差=%d)",  aux_handle->fsm_chao_loop_cnt, diff);
            time_dn_trend = aos_now_ms();
            temp_dn_trend = temp_min;
            temp_up_trend = temp_max;
        }  
    }
    else
    {
        if (temp_min < temp_dn_trend)
            temp_dn_trend = temp_min;

        if (temp_max > temp_up_trend)
            temp_up_trend = temp_max;

        if (aos_now_ms() - time_dn_trend >= 10*1000)
        {
            LOGI("aux","炒模式(%d): 10秒内从%d上升到%d 差值=%d", aux_handle->fsm_chao_loop_cnt, temp_dn_trend, temp_max, temp_max-temp_dn_trend);
            if ((temp_max <= 80*10) /*|| (temp_dn_trend <= 40*10)*/)  //这里有点担心: 油加多了或者葱姜蒜加多了，会不会也降到40度啊           
            {
                LOGI("aux", "炒模式(%d): ⇨ 炒食材 (耗时%d秒)",aux_handle->fsm_chao_loop_cnt, (int)((aos_now_ms() - fsm->state_time)/1000) );
                switch_fsm_state(fsm, chao_heat_food);
                beep_control_cb(0x02);
            }
            else
            {
                LOGI("aux", "炒模式(%d): ⇨ 热锅热油爆香 (耗时%d秒)",aux_handle->fsm_chao_loop_cnt, (int)((aos_now_ms() - fsm->state_time)/1000));
                switch_fsm_state(fsm, chao_heat_pan_oil_onion);
                aux_handle->fsm_chao_loop_cnt++;
                beep_control_cb(0x02);
            }
        }
    }

    if (temp_arrivate)
    {
        if (time_warn_1 == 0)
        {
            if (aos_now_ms() - time_remind > 30*1000)
            {
                udp_voice_write_sync("为防止干烧,切换小火", strlen("为防止干烧,切换小火"), 50);
                beep_control_cb(0x02);  
                change_multivalve_gear(0x07, INPUT_RIGHT);
                time_warn_1 = aos_now_ms();  //报警1触发时段
            }
        }
        else
        {
            if (time_warn_2 == 0)
            {
                if (aos_now_ms() - time_warn_1 > 60*1000)
                {
                    udp_voice_write_sync("为防止干烧,关闭右灶", strlen("为防止干烧,关闭右灶"), 50);
                    beep_control_cb(0x02);  
                    aux_close_fire_cb(INPUT_RIGHT);             //右灶关火  
                    time_warn_2 = aos_now_ms();     //报警2触发时段

                    aux_handle->aux_switch = 0;                 //辅助烹饪煮模式结束
                    if(aux_exit_cb != NULL)
                    {
                        aux_exit_cb(AUX_SUCCESS_EXIT);
                    }
                }
            }
        }
    }
}

void chao_heat_pan(func_ptr_fsm_t* fsm, aux_handle_t *aux_handle)
{
    //去除温度曲线的毛刺，借助chatgpt，从第一个稳定的温度开始（如何找第一个稳定的温度：连续10个温度，相邻两个温度之间不超过3度），相邻两个温度的绝对值不能超过±3度，超过了就把这个温度去除掉
    static temp_value_t temp_value_last = {0x00};

    static bool     temp_arrivate   = false;    //温度是否达到
    static uint64_t tick_arrivate   = 0;        //温度到达时刻

    static uint64_t time_dn_trend   = 0;        //下降趋势的起始时刻
    static uint16_t temp_dn_trend   = 0;        //下降趋势的最低温度
    static uint16_t temp_up_trend   = 0;        //上升趋势的最高温度    

    static uint64_t time_warn_1     = 0;        //第一次警告的时间
    static uint64_t time_warn_2     = 0;        //第二次警告的时间

    static int slope_cnt            = 0;

    uint16_t temp_cur = aux_handle->temp_array[ARRAY_DATA_SIZE - 1];            // 最新温度
    uint16_t temp_mid = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);    // 中位温度
    uint16_t temp_min = findMin(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最小温度
    uint16_t temp_max = findMax(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最大温度

    if (fsm->state_time == 0)
    {
        udp_voice_write_sync("开始热锅", strlen("开始热锅"), 50);
        LOGI("aux","炒模式(热锅阶段): 进入热锅");
        set_fsm_time(fsm);
        temp_value_last = save_current_temp(temp_mid); //从进入模式的第一个温度开始，每隔3秒计算一次斜率

        temp_arrivate = false;
        tick_arrivate = 0; 

        time_dn_trend = 0;   
        temp_dn_trend = 0;  
        temp_up_trend = 0;        
        
        time_warn_1   = 0;
        time_warn_2   = 0;

        slope_cnt     = 0;
    }

    double slope;
    bool   slope_flag = false;
    if ((aos_now_ms() - temp_value_last.time) >= (3*1000))  //每隔3秒钟就判断1次是否切换状态（如果用户下油了，就切换到热油状态）
    {
        temp_value_t temp_value_now = 
        {
            .time = aos_now_ms(),
            .temp = temp_mid
        };

        slope = calculateSlope(temp_value_last, temp_value_now);
        slope_flag = true;
        slope_cnt++;        
        LOGI("aux", "炒模式(热锅阶段): *****判断斜率(%d)***** (%d %d) ===> (%d %d) [斜率=%f]", 
            slope_cnt,
            (int)(temp_value_last.time/1000), temp_value_last.temp, 
            (int)(temp_value_now.time/1000),  temp_value_now.temp, 
            slope);

        temp_value_last = temp_value_now;
    }

    //档位调节逻辑 状态切换逻辑 干烧报警逻辑这三个逻辑没有依赖关系

    //档位调节逻辑(注意不要太频繁)
    if (time_warn_1 == 0)
    {
        if (!temp_arrivate)
        {
            if (temp_cur < 180*10)
            {
                change_multivalve_gear(0x01, INPUT_RIGHT);
            }
            else
            {
                if (temp_cur < 200*10)
                {
                    change_multivalve_gear(0x04, INPUT_RIGHT);
                }
                else
                {
                    change_multivalve_gear(0x07, INPUT_RIGHT);
                }
        
                if (!temp_arrivate)
                {
                    udp_voice_write_sync("请下油", strlen("请下油"), 50);
                    beep_control_cb(0x02);
                    temp_arrivate = true;
                    tick_arrivate = aos_now_ms();
                }
            }
        }
        else
        {
            //这里是否会导致火力切换频繁??? 需要加个时间限定一下
            if (temp_cur < 150*10)
            {
                change_multivalve_gear(0x01, INPUT_RIGHT);
            }
            else
            {
                change_multivalve_gear(0x07, INPUT_RIGHT);
            }
        }
    }

    //状态切换逻辑：升温慢    
    if (!is_fsm_timeout(fsm, 6000))  //如果升温较慢,直接跳转到热菜阶段（温度一直上升,但是上升的很慢）
    {
        if (slope_flag && slope < 10.0)
        {
            LOGI("aux", "炒模式(热锅阶段): 温升较慢 ⇨ 炒食材");
            switch_fsm_state(fsm, chao_heat_food);
        }
    }
    //状态切换逻辑：先降温再升温
    if (time_dn_trend == 0)
    {              
        int diff = ( (int)(aux_handle->temp_array[0]) - (int)(aux_handle->temp_array[ARRAY_DATA_SIZE - 1]) );
        bool res = judge_cook_trend_down(aux_handle->temp_array, ARRAY_DATA_SIZE, 3);
        if (res && (diff >= 30*10))
        {  
            LOGI("aux", "炒模式(热锅阶段): 检测到温度骤降 ↓↓↓ (温差=%d)", diff);
            time_dn_trend = aos_now_ms();
            temp_dn_trend = temp_min;
            temp_up_trend = temp_max;
        }  
    }
    else
    {
        if (temp_min < temp_dn_trend)
            temp_dn_trend = temp_min;
        if (temp_max > temp_up_trend)
            temp_up_trend = temp_max;

        if (aos_now_ms() - time_dn_trend >= 8*1000)
        {
            LOGI("aux","炒模式(热锅阶段): 温度下降后, 8秒内从%d上升到%d 温差=%d", temp_dn_trend, temp_max, temp_max-temp_dn_trend);
            if ((temp_max <= 80*10) /*|| (temp_dn_trend <= 40*10)*/)  //要用temp_max，而不能用temp_up_trend         
            {
                LOGI("aux", "炒模式(热锅阶段): ⇨ 炒食材 (耗时%d秒)", (int)((aos_now_ms() - fsm->state_time)/1000) );
                switch_fsm_state(fsm, chao_heat_food);
                beep_control_cb(0x02);
            }
            else
            {
                LOGI("aux", "炒模式(热锅阶段): ⇨ 热油 (耗时%d秒)", (int)((aos_now_ms() - fsm->state_time)/1000));
                switch_fsm_state(fsm, chao_heat_oil);
                beep_control_cb(0x02);
            }
        }
    }    

    //干烧报警逻辑
    if (temp_arrivate)
    {
        if (time_warn_1 == 0)
        {
            if (aos_now_ms() - tick_arrivate > 30*1000)
            {
                udp_voice_write_sync("为防止干烧,切换最小火", strlen("为防止干烧,切换最小火"), 50);
                beep_control_cb(0x02);
                change_multivalve_gear(0x07, INPUT_RIGHT);
                time_warn_1 = aos_now_ms();  //报警1触发时段
            }
        }
        else
        {
            if (time_warn_2 == 0)
            {
                if (aos_now_ms() - time_warn_1 > 60*1000)
                {
                    udp_voice_write_sync("为防止干烧,关闭右灶", strlen("为防止干烧,关闭右灶"), 50);
                    beep_control_cb(0x02);  
                    aux_close_fire_cb(INPUT_RIGHT); //右灶关火  
                    time_warn_2 = aos_now_ms();     //报警2触发时段

                    aux_handle->aux_switch = 0;     //辅助烹饪煮模式结束
                    if(aux_exit_cb != NULL)
                    {
                        aux_exit_cb(AUX_SUCCESS_EXIT);
                    }
                }
            }
        }
    }
}

void chao_heat_oil(func_ptr_fsm_t* fsm, aux_handle_t *aux_handle)
{
    static temp_value_t temp_value_last = {0x00};

    static bool     temp_arrivate   = false;    //温度是否达到
    static uint64_t tick_arrivate   = 0;        //温度到达时刻

    static uint64_t time_dn_trend   = 0;        //下降趋势的起始时刻
    static uint16_t temp_dn_trend   = 0;        //下降趋势的最低温度
    static uint16_t temp_up_trend   = 0;        //上升趋势的最高温度    
    
    static uint64_t time_warn_1     = 0;        //第一次警告的时间
    static uint64_t time_warn_2     = 0;        //第二次警告的时间

    uint16_t temp_cur = aux_handle->temp_array[ARRAY_DATA_SIZE - 1];            // 最新温度
    uint16_t temp_mid = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);    // 中位温度
    uint16_t temp_min = findMin(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最小温度
    uint16_t temp_max = findMax(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最大温度

    if (fsm->state_time == 0)
    {
        udp_voice_write_sync("开始热油", strlen("开始热油"), 50);
        LOGI("aux","炒模式(热油阶段): 进入热油");
        set_fsm_time(fsm);
        temp_value_last = save_current_temp(temp_mid);

        temp_arrivate = false;
        tick_arrivate = 0; 

        time_dn_trend = 0;   
        temp_dn_trend = 0;  
        temp_up_trend = 0;        
        
        time_warn_1   = 0;
        time_warn_2   = 0;
    }

    double slope;
    bool   slope_flag = false;
    if ((aos_now_ms() - temp_value_last.time) >= (3*1000))  //每隔3秒钟就判断1次是否切换状态（如果用户下油了，就切换到热油状态）
    {
        temp_value_t temp_value_now = 
        {
            .time = aos_now_ms(),
            .temp = temp_mid
        };

        slope = calculateSlope(temp_value_last, temp_value_now);
        slope_flag = true;
        LOGI("aux","炒模式(热油阶段): *****判断斜率***** (%d %d) ===> (%d %d) [斜率=%f]",
            (int)(temp_value_last.time/1000), temp_value_last.temp, 
            (int)(temp_value_now.time/1000),  temp_value_now.temp, 
            slope);

        temp_value_last = temp_value_now;
    }

    //档位调节逻辑(注意不要太频繁)
    if (time_warn_1 == 0)
    {
        if (!temp_arrivate)
        {
            if (temp_cur < 180*10)
            {
                change_multivalve_gear(0x05, INPUT_RIGHT);
            }
            else
            {
                change_multivalve_gear(0x07, INPUT_RIGHT);
        
                if (!temp_arrivate)
                {
                    udp_voice_write_sync("请下葱姜蒜", strlen("请下葱姜蒜"), 50);
                    beep_control_cb(0x02);
                    temp_arrivate = true;
                    tick_arrivate = aos_now_ms();
                }
            }
        }
        else
        {
            //这里是否会导致火力切换频繁??? 需要加个时间限定一下
            if (temp_cur < 160*10)
            {
                change_multivalve_gear(0x05, INPUT_RIGHT);
            }
            else
            {
                change_multivalve_gear(0x07, INPUT_RIGHT);
            }
        }
    }

    //状态切换逻辑：升温慢    
    // if (!is_fsm_timeout(fsm, 10*1000))  //以前是1档火力,判断时间为6秒，现在是5档火力，判断时间改为长一点10秒
    // {
    //     if (slope_flag && slope < 10.0)
    //     {
    //         LOGI("aux", "炒模式(热油阶段): 温升较慢 ⇨ 炒食材");
    //         switch_fsm_state(fsm, chao_heat_food);
    //     }
    // }
    //状态切换逻辑：先降温再升温
    if (time_dn_trend == 0)
    {              
        int diff = ( (int)(aux_handle->temp_array[0]) - (int)(aux_handle->temp_array[ARRAY_DATA_SIZE - 1]) );
        bool res = judge_cook_trend_down(aux_handle->temp_array, ARRAY_DATA_SIZE, 3);
        if (res && (diff >= 30*10))
        {  
            LOGI("aux", "炒模式(热油阶段): 检测到温度骤降 ↓↓↓ (温差=%d)", diff);
            time_dn_trend = aos_now_ms();
            temp_dn_trend = temp_min;
            temp_up_trend = temp_max;
        }  
    }
    else
    {
        if (temp_min < temp_dn_trend)
            temp_dn_trend = temp_min;
        if (temp_max > temp_up_trend)
            temp_up_trend = temp_max;

        if (aos_now_ms() - time_dn_trend >= 10*1000)
        {
            LOGI("aux","炒模式(热油阶段): 10秒内从%d上升到%d 差值=%d", temp_dn_trend, temp_max, temp_max-temp_dn_trend);
            if ((temp_max <= 80*10) /*|| (temp_dn_trend <= 40*10)*/)  //要用temp_max，而不能用temp_up_trend         
            {
                LOGI("aux", "炒模式(热油阶段): ⇨ 炒食材 (耗时%d秒)", (int)((aos_now_ms() - fsm->state_time)/1000) );
                switch_fsm_state(fsm, chao_heat_food);
                beep_control_cb(0x02);
            }
            else
            {
                LOGI("aux", "炒模式(热油阶段): ⇨ 爆香 (耗时%d秒)", (int)((aos_now_ms() - fsm->state_time)/1000));
                switch_fsm_state(fsm, chao_heat_onion);
                beep_control_cb(0x02);
            }
        }
    } 

    //干烧报警逻辑
    if (temp_arrivate)
    {
        if (time_warn_1 == 0)
        {
            if (aos_now_ms() - tick_arrivate > 30*1000)
            {
                udp_voice_write_sync("为防止干烧,切换小火", strlen("为防止干烧,切换小火"), 50);
                beep_control_cb(0x02);  
                change_multivalve_gear(0x07, INPUT_RIGHT);
                time_warn_1 = aos_now_ms();  //报警1触发时段
            }
        }
        else
        {
            if (time_warn_2 == 0)
            {
                if (aos_now_ms() - time_warn_1 > 60*1000)
                {
                    udp_voice_write_sync("为防止干烧,关闭右灶", strlen("为防止干烧,关闭右灶"), 50);
                    beep_control_cb(0x02);  
                    aux_close_fire_cb(INPUT_RIGHT);             //右灶关火  
                    time_warn_2 = aos_now_ms();     //报警2触发时段

                    aux_handle->aux_switch = 0;                 //辅助烹饪煮模式结束
                    if(aux_exit_cb != NULL)
                    {
                        aux_exit_cb(AUX_SUCCESS_EXIT);
                    }
                }
            }
        }
    }
}

void chao_heat_onion(func_ptr_fsm_t* fsm, aux_handle_t *aux_handle)
{
    static temp_value_t temp_value_last = {0x00};

    static bool     temp_arrivate   = false;    //温度是否达到
    static uint64_t tick_arrivate   = 0;        //温度到达时刻
    static uint64_t tick_switch     = 0;

    static uint64_t time_dn_trend   = 0;        //下降趋势的起始时刻
    static uint16_t temp_dn_trend   = 0;        //下降趋势的最低温度
    static uint16_t temp_up_trend   = 0;        //上升趋势的最高温度    
    
    static uint64_t time_warn_1     = 0;        //第一次警告的时间
    static uint64_t time_warn_2     = 0;        //第二次警告的时间

    uint16_t temp_cur = aux_handle->temp_array[ARRAY_DATA_SIZE - 1];            // 最新温度
    uint16_t temp_mid = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);    // 中位温度
    uint16_t temp_min = findMin(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最小温度
    uint16_t temp_max = findMax(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最大温度

    if (fsm->state_time == 0)
    {
        udp_voice_write_sync("开始爆香", strlen("开始爆香"), 50);
        LOGI("aux","炒模式(爆香阶段): 进入爆香");
        set_fsm_time(fsm);
        temp_value_last = save_current_temp(temp_mid);

        temp_arrivate = false;
        tick_arrivate = 0; 
        tick_switch   = 0;

        time_dn_trend = 0;   
        temp_dn_trend = 0;  
        temp_up_trend = 0;        
        
        time_warn_1   = 0;
        time_warn_2   = 0;
    }

    double slope;
    bool   slope_flag = false;
    if ((aos_now_ms() - temp_value_last.time) >= (3*1000))  //每隔3秒钟就判断1次是否切换状态（如果用户下油了，就切换到热油状态）
    {
        temp_value_t temp_value_now = 
        {
            .time = aos_now_ms(),
            .temp = temp_mid
        };

        slope = calculateSlope(temp_value_last, temp_value_now);
        slope_flag = true;
        LOGI("aux", "炒模式(爆香阶段): *****判断斜率***** (%d %d) ===> (%d %d) [斜率=%f]",
            (int)(temp_value_last.time/1000), temp_value_last.temp, 
            (int)(temp_value_now.time/1000),  temp_value_now.temp, 
            slope);

        temp_value_last = temp_value_now;
    }

    //档位调节逻辑(注意不要太频繁)
    if (time_warn_1 == 0)
    {
        if (!temp_arrivate)
        {
            if (temp_cur < 120*10)
            {
                change_multivalve_gear(0x05, INPUT_RIGHT);
            }
            else
            {
                change_multivalve_gear(0x07, INPUT_RIGHT);
        
                if (!temp_arrivate)
                {
                    udp_voice_write_sync("请下食材", strlen("请下食材"), 50);
                    beep_control_cb(0x02);
                    temp_arrivate = true;
                    tick_arrivate = aos_now_ms();
                }
            }
        }
        else
        {
            if ((tick_switch == 0) || aos_now_ms() - tick_switch >= 10*1000)
            {
                tick_switch = aos_now_ms();
                if (temp_cur < 120*10)
                    change_multivalve_gear(0x05, INPUT_RIGHT);
                else
                    change_multivalve_gear(0x07, INPUT_RIGHT);
            }
        }
    }

    if (time_dn_trend == 0)
    {  
        int diff = ( (int)(aux_handle->temp_array[0]) - (int)(aux_handle->temp_array[ARRAY_DATA_SIZE - 1]) );
        bool res = judge_cook_trend_down(aux_handle->temp_array, ARRAY_DATA_SIZE, 3);
        if (res && (diff >= 30*10))
        {  
            LOGI("aux", "炒模式(爆香阶段): 检测到温度骤降 ↓↓↓ (温差=%d)",  diff);
            time_dn_trend = aos_now_ms();
            temp_dn_trend = temp_min;
            temp_up_trend = temp_max;
        }  
    }
    else
    {
        if (temp_min < temp_dn_trend)
            temp_dn_trend = temp_min;

        if (temp_max > temp_up_trend)
            temp_up_trend = temp_max;

        if (aos_now_ms() - time_dn_trend >= 10*1000)
        {
            LOGI("aux","炒模式(爆香阶段): 10秒内从%d上升到%d 差值=%d", temp_dn_trend, temp_max, temp_max-temp_dn_trend);
            if ((temp_max <= 80*10) /*|| (temp_dn_trend <= 40*10)*/)  //这里有点担心: 油加多了或者葱姜蒜加多了，会不会也降到40度啊           
            {
                LOGI("aux", "炒模式(爆香阶段): ⇨ 炒食材 (耗时%d秒)", (int)((aos_now_ms() - fsm->state_time)/1000) );
                switch_fsm_state(fsm, chao_heat_food);
                beep_control_cb(0x02);
            }
            else
            {
                time_dn_trend = 0;  //再次等待温度下降(本次温度下降并不是因为下菜引起的)
                temp_dn_trend = 0;
                temp_up_trend = 0;
                // LOGI("aux", "炒模式(%d): ⇨ 热锅热油爆香 (耗时%d秒)",aux_handle->fsm_chao_loop_cnt, (int)((aos_now_ms() - fsm->state_time)/1000));
                // switch_fsm_state(fsm, chao_heat_pan_oil_onion);
                // aux_handle->fsm_chao_loop_cnt++;
                // beep_control_cb(0x02);
            }
        }
    }

    if (temp_arrivate)
    {
        if (time_warn_1 == 0)
        {
            if (aos_now_ms() - tick_arrivate > 30*1000)
            {
                udp_voice_write_sync("为防止干烧,切换小火", strlen("为防止干烧,切换小火"), 50);
                beep_control_cb(0x02);  
                change_multivalve_gear(0x07, INPUT_RIGHT);
                time_warn_1 = aos_now_ms();  //报警1触发时段
            }
        }
        else
        {
            if (time_warn_2 == 0)
            {
                if (aos_now_ms() - time_warn_1 > 60*1000)
                {
                    udp_voice_write_sync("为防止干烧,关闭右灶", strlen("为防止干烧,关闭右灶"), 50);
                    beep_control_cb(0x02);  
                    aux_close_fire_cb(INPUT_RIGHT);             //右灶关火  
                    time_warn_2 = aos_now_ms();     //报警2触发时段

                    aux_handle->aux_switch = 0;                 //辅助烹饪煮模式结束
                    if(aux_exit_cb != NULL)
                    {
                        aux_exit_cb(AUX_SUCCESS_EXIT);
                    }
                }
            }
        }
    }
}

void chao_heat_food(func_ptr_fsm_t* fsm, aux_handle_t *aux_handle)
{
    static temp_value_t temp_value_last = {0x00};

    static bool temp_arrivate     = false;
    static uint64_t tick_switch   = 0;

    static bool up_trend_flag     = false;
    static uint64_t time_up_trend = 0;
    static uint64_t time_warn_1   = 0;    //第一次警告的时间
    static uint64_t time_warn_2   = 0;    //第二次警告的时间 

    static bool bujiu_flag        = false;
    static uint64_t time_warn_0   = 0;


    uint16_t temp_cur = aux_handle->temp_array[ARRAY_DATA_SIZE - 1];            // 最新温度
    uint16_t temp_mid = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);    // 中位温度
    uint16_t temp_min = findMin(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最小温度
    uint16_t temp_max = findMax(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最大温度

    if (fsm->state_time == 0)
    {
        set_fsm_time(fsm);
        temp_value_last = save_current_temp(temp_mid);

        udp_voice_write_sync("开始炒食材", strlen("开始炒食材"), 50);
        LOGI("aux","炒模式(炒): 进入炒食材");

        temp_arrivate = false;  
        tick_switch   = 0;  

        up_trend_flag = false;
        time_up_trend = 0;
        time_warn_1   = 0;    //第一次警告的时间
        time_warn_2   = 0;    //第二次警告的时间

        bujiu_flag    = false;
        time_warn_0   = 0;
    }

    double slope;
    bool   slope_flag = false;
    if ((aos_now_ms() - temp_value_last.time) >= (3*1000))  //每隔3秒钟就判断1次是否切换状态（如果用户下油了，就切换到热油状态）
    {
        temp_value_t temp_value_now = 
        {
            .time = aos_now_ms(),
            .temp = temp_mid
        };

        slope = calculateSlope(temp_value_last, temp_value_now);
        slope_flag = true;
        LOGI("aux","炒模式(炒): *****判断斜率***** (%d %d) ===> (%d %d) [斜率=%f]",
            (int)(temp_value_last.time/1000), temp_value_last.temp, 
            (int)(temp_value_now.time/1000),  temp_value_now.temp, 
            slope);

        temp_value_last = temp_value_now;
    }

    if ((time_warn_1 == 0) && (!bujiu_flag))
    {
        if ((tick_switch == 0) || aos_now_ms() - tick_switch >= 5*1000)
        {
            if (temp_cur < 100*10)
            {
                change_multivalve_gear(0x00, INPUT_RIGHT);
                tick_switch = aos_now_ms();
            }
            else
            {
                temp_arrivate = true;//这里有异常，只有每隔10秒才判断一次
                change_multivalve_gear(0x02, INPUT_RIGHT);
                tick_switch = aos_now_ms();
            }
        }
    }

    bool body_exist_flag = false;
    if (check_any_large_diff(aux_handle->temp_array, ARRAY_DATA_SIZE))  //温度波动是否大于5
    {
        body_exist_flag = true;
        LOGI("aux","炒模式(炒): 有人存在");
    }
    else
    {
        body_exist_flag = false;
        LOGI("aux","炒模式(炒): 无人存在!!! 无人存在!!! 无人存在!!!");
    }

    if (!body_exist_flag)  //无人存在
    {
        if (time_warn_0 == 0) 
        {
            time_warn_0 = aos_now_ms();
        }
        else
        {
            int time_diff = (aos_now_ms() - time_warn_0);
            if (time_diff > 5*1000)
            {
                if (!bujiu_flag)
                {
                    //udp_voice_write_sync("为防止焦糊,切换5档火力", strlen("为防止焦糊,切换5档火力"), 50);
                    change_multivalve_gear(0x05, INPUT_RIGHT);  
                    bujiu_flag = true;
                }
            }
        }
    }
    else
    {
        bujiu_flag    = false;
        time_warn_0   = 0;
    }

    if (!body_exist_flag)  //无人存在
    {      
        if (time_up_trend == 0) 
        {
            time_up_trend = aos_now_ms();//首次进入温度上升的时间
        }
        else
        {
            int time_diff = (aos_now_ms() - time_up_trend);
            if (time_diff > 15*1000)
            {
                if (time_warn_1 == 0)
                {
                    udp_voice_write_sync("为防止干烧,降低火力", strlen("为防止干烧,降低火力"), 50);
                    beep_control_cb(0x02);  
                    change_multivalve_gear(0x07, INPUT_RIGHT); //火力7档  
                    time_warn_1 = aos_now_ms();
                }
                else
                {
                    if (time_warn_2 == 0)
                    {
                        if ((aos_now_ms() - time_warn_1) > 60*1000)
                        {
                            udp_voice_write_sync("为防止干烧,关闭右灶", strlen("为防止干烧,关闭右灶"), 50);
                            beep_control_cb(0x02);  
                            aux_close_fire_cb(INPUT_RIGHT);             //右灶关火  
                            time_warn_2 = aos_now_ms();
                
                            aux_handle->aux_switch = 0;                 //辅助烹饪煮模式结束
                            if(aux_exit_cb != NULL)
                            {
                                aux_exit_cb(AUX_SUCCESS_EXIT);
                            }   
                        }         
                    }
                }
            }
        }       
    }
    else  //有人存在
    {
        time_up_trend = 0;
        time_warn_1   = 0;
        time_warn_2   = 0;
    }
}

/**
 * @brief:
 * @param {aux_handle_t} *aux_handle
 * @return {*}
 */
void mode_chao_func(aux_handle_t *aux_handle)
{
    if (aux_handle->fsm_chao.state_func == NULL)
    {
        switch_fsm_state(&aux_handle->fsm_chao, chao_heat_pan);//chao_heat_pan chao_heat_pan_oil_onion
    }

    run_fsm(&aux_handle->fsm_chao, aux_handle);
}

bool judge_cook_trend_turn_over(uint16_t array[], uint16_t length)
{
    // int count_rise  = 0; 
    // int count_down  = 0; 
    // bool step1      = false;
    // bool step2      = false;
    // bool step3      = false;

    // // 遍历数组，按顺序比较相邻两个元素
    // for (uint16_t i = 0; i < 5; i++) {
    //     if (array[i + 1] > array[i]) {
    //         count_rise++;
    //     }
    // }

    // // 遍历数组，按顺序比较相邻两个元素
    // for (uint16_t i = 4; i < 9; i++) {
    //     if (array[i + 1] < array[i]) {
    //         count_down++;
    //     }
    // }

    // if ((count_rise >= 3) && ((array[0] <= 100*10) || (array[1] <= 100*10)))
    // {
    //     step1 = true;
    // }

    // if ((count_down >= 3) && ((array[8] <= 100*10) || (array[9] <= 100*10)))
    // {
    //     step2 = true;
    // }

    // if ((array[4] >= 150*10) || (array[5] >= 150*10))
    // {
    //     step3 = true;
    // }

    // // 根据统计数据与阈值的关系返回结果
    // if (step1 && step2 && step3) {
    //     LOGI("aux", "judge_cook_trend_turn_over: 翻面  (上升个数=%d 下降个数=%d)", count_rise, count_down);
    //     return true; 
    // } else {
    //     return false; 
    // }



    int diff = ( (int)(array[ARRAY_DATA_SIZE - 1]) - (int)(array[0]) );
    bool res = judge_cook_trend_rise(array, ARRAY_DATA_SIZE, 3);
    if (res && (diff > 30*10))
    {
        LOGI("aux", " ********检测到翻面 检测到翻面 检测到翻面**********  (diff=%d)", diff);
        return true;
    } 
    else
    {
        return false;
    }
}

bool judge_cook_trend_rise(uint16_t array[], uint16_t length, int threshold)
{
    int count = 0; // 统计后一个数小于等于前一个数的次数

    // 遍历数组，按顺序比较相邻两个元素
    for (uint16_t i = 0; i < length - 1; i++) {
        if (array[i + 1] > array[i]) {
            count++;
        }
    }

    // 根据统计数据与阈值的关系返回结果
    if (count >= threshold) {
        LOGI("aux","judge_cook_trend_rise: 缓升!!! (上升个数=%d 判断阈值=%d)", count, threshold);
        return true; // 如果统计数量大于等于阈值，返回true
    } else {
        return false; // 如果统计数量小于阈值，返回false
    }
}

bool judge_cook_trend_down(uint16_t array[], uint16_t length, int threshold) 
{
    int count = 0; // 统计后一个数小于等于前一个数的次数

    // 遍历数组，按顺序比较相邻两个元素
    for (uint16_t i = 0; i < length - 1; i++) {
        if (array[i + 1] < array[i]) {
            count++; // 如果后一个数小于等于前一个数，计数加一
        }
    }

    // 根据统计数据与阈值的关系返回结果
    if (count >= threshold) {
        LOGI("aux","judge_cook_trend_down: 缓降!!! (下降个数=%d 判断阈值=%d)", count, threshold);
        return true; // 如果统计数量大于等于阈值，返回true
    } else {
        return false; // 如果统计数量小于阈值，返回false
    }
}

bool check_any_large_diff(uint16_t array[], uint16_t length) 
{
    for (uint16_t i = 0; i < length - 1; i++) {
        // 计算相邻元素的绝对差值
        uint16_t current = array[i];
        uint16_t next = array[i + 1];
        uint16_t diff = (current > next) ? (current - next) : (next - current);

        // 如果差值超过5，立即返回true
        if (diff > 5*10) {
            LOGI("aux", "check_any_large_diff: 温度有波动 %d", diff);
            return true;
        }
    }

    // 遍历所有相邻元素后未找到符合条件的差值，返回false
    return false;
}

void jian_heat_pan_oil(func_ptr_fsm_t* fsm, aux_handle_t *aux_handle)
{
    static temp_value_t temp_value_last = {0x00};

    static uint64_t time_down_trend = 0;    //下降趋势的起始时间
    static uint16_t temp_dn_trend   = 0;    //下降期间的最低温度
    static uint16_t temp_up_trend   = 0;    //下降期间的最高温度

    static bool temp_arrivate       = false;
    static uint64_t time_remind     = 0;    //温度到达时间
    static uint64_t time_warn_1     = 0;    //第一次警告的时间
    static uint64_t time_warn_2     = 0;    //第二次警告的时间    

    static int slope_cnt            = 0;

    uint16_t temp_cur = aux_handle->temp_array[ARRAY_DATA_SIZE - 1];            // 最新温度
    uint16_t temp_mid = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);    // 中位数温度
    uint16_t temp_min = findMin(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最小温度
    uint16_t temp_max = findMax(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最大温度


    if (fsm->state_time == 0)
    {
        set_fsm_time(fsm);
        temp_value_last = save_current_temp(temp_mid);

        if (aux_handle->fsm_jian_loop_cnt == 0)
        {
            udp_voice_write_sync("开始热锅", strlen("开始热锅"), 50);
        }
        else
        {
            udp_voice_write_sync("开始热油", strlen("开始热油"), 50);
        }
        LOGI("aux","煎模式(%d): 进入热锅或者热油阶段", aux_handle->fsm_jian_loop_cnt);

        time_down_trend = 0;
        temp_dn_trend   = 0;
        temp_up_trend   = 0;

        temp_arrivate   = false;
        time_remind     = 0;
        time_warn_1     = 0;
        time_warn_2     = 0;

        slope_cnt       = 0;
    }

    double slope;
    bool   slope_flag = false;
    if ((aos_now_ms() - temp_value_last.time) >= (3*1000))
    {
        temp_value_t temp_value_now = 
        {
            .time = aos_now_ms(),
            .temp = temp_mid
        };

        slope = calculateSlope(temp_value_last, temp_value_now);
        slope_cnt++;
        slope_flag = true;

        LOGI("aux", "煎模式(%d): *****判断斜率(%d)***** (%d %d) ===> (%d %d) [斜率=%f]", 
            aux_handle->fsm_jian_loop_cnt, 
            slope_cnt,
            (int)(temp_value_last.time/1000), temp_value_last.temp, 
            (int)(temp_value_now.time/1000),  temp_value_now.temp, 
            slope);

        temp_value_last = temp_value_now;
    }

    if (time_warn_1 == 0)
    {
        if (!temp_arrivate)
        {
            if (temp_cur < 180*10)
            {
                change_multivalve_gear(0x01, INPUT_RIGHT);
            }
            else
            {
                if (temp_cur < 200*10)
                {
                    change_multivalve_gear(0x04, INPUT_RIGHT);
                }
                else
                {
                    change_multivalve_gear(0x07, INPUT_RIGHT);
                }
        
                if (!temp_arrivate)
                {
                    if (aux_handle->fsm_jian_loop_cnt == 0)
                        udp_voice_write_sync("请下油", strlen("请下油"), 50);
                    else
                        udp_voice_write_sync("请下食材", strlen("请下食材"), 50);
                    beep_control_cb(0x02);
                    temp_arrivate = true;
                    time_remind = aos_now_ms();
                }
            }
        }
        else
        {
            //热油阶段时，如果把锅晃几下，会不会导致火力切换频繁? 是否需要加个时间限定一下
            //如果加了5秒时间限定了，那用户真的下菜时，是不是会最大会耽误5秒？
            if (temp_cur < 160*10)
            {
                change_multivalve_gear(0x01, INPUT_RIGHT);
            }
            else
            {
                change_multivalve_gear(0x07, INPUT_RIGHT);
            }
        }
    } 

    //情况一：锅里本来就有食物(前6秒就能判断出来升温慢，然后切入到煎食物阶段)  
    if (aux_handle->fsm_jian_loop_cnt == 0)
    {
        if (!is_fsm_timeout(fsm, 6500))  //锅里有菜,直接跳转到热菜阶段
        {
            if (slope_flag && (slope < 10.0) && (slope > 0.1))
            {
                LOGI("aux", "煎模式(%d): 起始6秒内温升慢 ⇨ 煎食材 (%f)", aux_handle->fsm_jian_loop_cnt, slope);
                beep_control_cb(0x02);  
                switch_fsm_state(fsm, jian_heat_food);
            }
        }
    }

    //情况二：热锅过程中(无论锅温是否到过了指定温度190度)，锅里加入了油或者食物(温度曲线是先下降然后上升，根据最低温度来判断是切入到热油还是煎食物)
    if (time_down_trend == 0)
    {
        LOGI("aux","煎模式(%d): wait time_down_trend 3", aux_handle->fsm_jian_loop_cnt);      
        int diff = ( (int)(aux_handle->temp_array[0]) - (int)(aux_handle->temp_array[ARRAY_DATA_SIZE - 1]) );
        bool res = judge_cook_trend_down(aux_handle->temp_array, ARRAY_DATA_SIZE, 3);
        if (res && (diff >= 20*10))
        {
            LOGI("aux", "煎模式(%d): 检测到温度骤降 ↓↓↓ (温差=%d)",  aux_handle->fsm_jian_loop_cnt, diff);
            time_down_trend = aos_now_ms();
            temp_dn_trend   = temp_min;
            temp_up_trend   = temp_max;

            if (time_warn_1 != 0) //开大火加热，并观察曲线，我觉得这里是漏掉的(第一次报警后，这时人工介入，放油或者放食材)
            {
                LOGI("aux", "煎模式(%d): 温度骤降,打破报警1的小火阶段,开始大火加热", aux_handle->fsm_jian_loop_cnt);
                change_multivalve_gear(0x01, INPUT_RIGHT);
            }
        }
    }
    else
    {
        if (temp_min < temp_dn_trend)
            temp_dn_trend = temp_min;

        if (temp_max > temp_up_trend)
            temp_up_trend = temp_max;

        if (aos_now_ms() - time_down_trend >= 10*1000)
        {
            LOGI("aux","煎模式(%d): 加热10秒后,从%d上升到%d(限定条件是100度) 差值=%d", aux_handle->fsm_jian_loop_cnt, temp_dn_trend, temp_max, temp_max-temp_dn_trend);
            if (temp_max < 100*10)  //热锅到90度，然后提前下油，加热8秒后，发现温度是94，因此这里的限定条件要从100度改成80度
            {//这里有矛盾的地方: 
                LOGI("aux", "煎模式(%d): ⇨ 煎食材 (耗时%d秒)",aux_handle->fsm_jian_loop_cnt, (int)((aos_now_ms() - fsm->state_time)/1000));
                switch_fsm_state(fsm, jian_heat_food);
                beep_control_cb(0x02);
            }
            else
            {
                if (aux_handle->fsm_jian_loop_cnt == 0)
                {
                    LOGI("aux", "煎模式(%d): ⇨ 热锅热油 (耗时%d秒)", aux_handle->fsm_jian_loop_cnt, (int)((aos_now_ms() - fsm->state_time)/1000));
                    switch_fsm_state(fsm, jian_heat_pan_oil);
                    aux_handle->fsm_jian_loop_cnt++;
                    beep_control_cb(0x02);
                }
                else
                {
                    time_down_trend = 0;
                    temp_dn_trend   = 0;
                    temp_up_trend   = 0;
                }
            }
        }
    }

    if (temp_arrivate)
    {
        if (time_warn_1 == 0)
        {
            if (aos_now_ms() - time_remind > 60*1000)
            {
                udp_voice_write_sync("防止干烧,切换小火", strlen("防止干烧,切换小火"), 50);
                beep_control_cb(0x02);  
                change_multivalve_gear(0x07, INPUT_RIGHT);
                time_warn_1 = aos_now_ms();  //报警1触发时段
            }
        }
        else
        {
            if (time_warn_2 == 0)
            {
                if (aos_now_ms() - time_warn_1 > 120*1000)
                {
                    udp_voice_write_sync("防止干烧,关闭右灶", strlen("防止干烧,关闭右灶"), 50);
                    beep_control_cb(0x02);  
                    aux_close_fire_cb(INPUT_RIGHT);             //右灶关火  
                    time_warn_2 = aos_now_ms();     //报警2触发时段

                    aux_handle->aux_switch = 0;                 //辅助烹饪煮模式结束
                    if(aux_exit_cb != NULL)
                    {
                        aux_exit_cb(AUX_SUCCESS_EXIT);
                    }
                }
            }
        }
    }
}

void jian_heat_pan(func_ptr_fsm_t* fsm, aux_handle_t *aux_handle)
{
    static temp_value_t temp_value_last = {0x00};
    uint16_t temp_cur = aux_handle->temp_array[ARRAY_DATA_SIZE - 1];            // 最新温度
    uint16_t temp_mid = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);    // 中位数温度
    uint16_t temp_min = findMin(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最小温度
    uint16_t temp_max = findMax(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最大温度

    static uint64_t time_down_trend = 0;    //下降趋势的起始时间
    static uint16_t temp_dn_trend   = 0;    //下降期间的最低温度
    static uint16_t temp_up_trend   = 0;    //下降期间的最高温度

    static bool is_remind           = false;
    static bool temp_arrivate       = false;
    static uint64_t time_remind     = 0;    //温度到达时间
    static uint64_t time_warn_1     = 0;    //第一次警告的时间
    static uint64_t time_warn_2     = 0;    //第二次警告的时间

    if (fsm->state_time == 0)
    {
        udp_voice_write_sync("开始热锅", strlen("开始热锅"), 50);
        LOGI("aux","煎模式(热锅阶段): 进入热锅");
        set_fsm_time(fsm);
        temp_value_last = save_current_temp(temp_mid);  //temp_cur        

        time_down_trend = 0;
        temp_dn_trend   = 0;
        temp_up_trend   = 0;

        is_remind       = false;
        temp_arrivate   = false;
        time_remind     = 0;
        time_warn_1     = 0;
        time_warn_2     = 0;
    }

    double slope;
    bool   slope_flag = false;
    if ((aos_now_ms() - temp_value_last.time) >= (3*1000))  //每隔3秒钟就判断1次是否切换状态（如果用户下油了，就切换到热油状态）
    {
        static int slope_cnt = 0;
        temp_value_t temp_value_now = 
        {
            .time = aos_now_ms(),
            .temp = temp_mid
        };

        slope = calculateSlope(temp_value_last, temp_value_now);
        slope_cnt++;
        slope_flag = true;

        LOGI("aux", "煎模式(热锅阶段): *****判断斜率(%d)***** (%d %d) ===> (%d %d) [斜率=%f · 最新温度=%d · 中位数温度=%d]", slope_cnt,
            (int)(temp_value_last.time/1000), temp_value_last.temp, 
            (int)(temp_value_now.time/1000),  temp_value_now.temp, 
            slope, temp_cur, temp_mid);

        temp_value_last = temp_value_now;
    }

    if (temp_cur < 190*10)
    {
        if (!temp_arrivate)
        {
            change_multivalve_gear(0x01, INPUT_RIGHT); //随意控制
        }
        else
        {
            if (time_warn_1 == 0)
            {
                if (temp_cur < (175*10))
                {
                    change_multivalve_gear(0x01, INPUT_RIGHT); 
                }
            }
            else
            {
                // 这个时候需要开大火吗？ 报警1触发后的温度下降,因为报警1触发后开7档，有可能温度就下来了，这时在开1档位不是白做了吗？
                // 无法区分这时温度低是因为报警开7档导致的还是因为用户突然下油下菜介入后导致的

                if (temp_cur < 150*10)
                {
                    change_multivalve_gear(0x01, INPUT_RIGHT);
                    LOGI("aux", "煎模式(热锅阶段): 报警1触发后的温降 (用户介入 %d)", temp_cur);
                }
            }
        }

        //情况一：锅里本来就有食物(前6秒就能判断出来升温慢，然后切入到煎食物阶段)                
        if (!is_fsm_timeout(fsm, 6500))  //锅里有菜,直接跳转到热菜阶段
        {
            if (slope_flag && slope < 10.0)
            {
                LOGI("aux", "煎模式(热锅阶段): 起始温升较慢 ⇨ 煎食材");
                beep_control_cb(0x02);  
                switch_fsm_state(fsm, jian_heat_food);
            }
        }

        //情况二：热锅过程中(无论锅温是否到过了指定温度190度)，锅里加入了油或者食物(温度曲线是先下降然后上升，根据最低温度来判断是切入到热油还是煎食物)
        if (time_down_trend == 0)
        {
            LOGI("aux","煎模式(热锅阶段): wait time_down_trend 3");      
            int diff = ( (int)(aux_handle->temp_array[0]) - (int)(aux_handle->temp_array[ARRAY_DATA_SIZE - 1]) );
            bool res = judge_cook_trend_down(aux_handle->temp_array, ARRAY_DATA_SIZE, 3);
            if (res && (diff >= 300))
            {  
                LOGI("aux", "煎模式(热锅阶段): 检测到温度骤降 ↓↓↓ (温度=%d 温差=%d)",  temp_mid, diff);
                time_down_trend = aos_now_ms();
                temp_dn_trend   = temp_min;
                temp_up_trend   = temp_max;
            }  
        }
        else
        {
            if (temp_min < temp_dn_trend)
                temp_dn_trend = temp_min;
            if (temp_max > temp_up_trend)
                temp_up_trend = temp_max;

            if (aos_now_ms() - time_down_trend >= 10*1000)
            {
                LOGI("aux","煎模式(热锅阶段): 10秒内 最低温度=%d ===> 本轮最高=%d 差值=%d", temp_dn_trend, temp_max, temp_max-temp_dn_trend);
                if (temp_max < 100)
                {
                    LOGI("aux", "煎模式(热锅阶段): 温度骤降1 ===> 切换热菜 (耗时%d秒 温度=%d 最低温度=%d)", (int)((aos_now_ms() - fsm->state_time)/1000), temp_mid, temp_dn_trend);
                    switch_fsm_state(fsm, jian_heat_food);
                }
                else
                {
                    LOGI("aux", "煎模式(热锅阶段): 温度骤降2 ===> 切换热油 (耗时%d秒 温度=%d 最低温度=%d)", (int)((aos_now_ms() - fsm->state_time)/1000), temp_mid, temp_dn_trend);
                    switch_fsm_state(fsm, jian_heat_oil);
                }
            }
        }
    }
    else
    {
        change_multivalve_gear(0x07, INPUT_RIGHT);
        temp_arrivate = true;

        if (!is_remind)
        {
            udp_voice_write_sync("请放油", strlen("请放油"), 50);
            beep_control_cb(0x02);
            is_remind = true;
            time_remind = aos_now_ms();//以提醒时间为起点,在30秒内如果还在找个状态没有切到其他状态去，就要触发防干烧
        }
    }

    if (is_remind)
    {
        if (time_warn_1 == 0)
        {
            if (aos_now_ms() - time_remind > 30*1000)
            {
                udp_voice_write_sync("为防止干烧, 切换小火", strlen("为防止干烧, 切换小火"), 50);
                beep_control_cb(0x02);  
                change_multivalve_gear(0x07, INPUT_RIGHT);
                time_warn_1 = aos_now_ms();  //报警1触发时段
            }
        }
        else
        {
            if (time_warn_2 == 0)
            {
                if (aos_now_ms() - time_warn_1 > 10*1000)
                {
                    udp_voice_write_sync("为防止干烧, 关闭右灶", strlen("为防止干烧, 关闭右灶"), 50);
                    beep_control_cb(0x02);  
                    aux_close_fire_cb(INPUT_RIGHT);             //右灶关火  
                    time_warn_2 = aos_now_ms();     //报警2触发时段

                    aux_handle->aux_switch = 0;                 //辅助烹饪煮模式结束
                    if(aux_exit_cb != NULL)
                    {
                        aux_exit_cb(AUX_SUCCESS_EXIT);
                    }            
                }
            }
        }
    }
}

void jian_heat_oil(func_ptr_fsm_t* fsm, aux_handle_t *aux_handle)
{
    static temp_value_t temp_value_last = {0x00};
    uint16_t temp_cur = aux_handle->temp_array[ARRAY_DATA_SIZE - 1];            // 最新温度
    uint16_t temp_mid = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);    // 中位温度
    uint16_t temp_min = findMin(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最小温度
    uint16_t temp_max = findMax(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最大温度

    static bool up_trend_flag       = false;
    static bool dn_trend_flag       = false;
    static uint64_t time_down_trend = 0;    //下降趋势的起始时间
    static uint16_t temp_dn_trend   = 0;    //下降趋势的最低温度
    static uint16_t temp_up_trend   = 0;    //下降趋势的最高温度


    static bool temp_arrivate       = false;
    static bool is_remind           = false;
    static uint64_t time_remind     = 0;    //温度到达时间
    static uint64_t time_warn_1     = 0;    //第一次警告的时间
    static uint64_t time_warn_2     = 0;    //第二次警告的时间

    if (fsm->state_time == 0)
    {
        udp_voice_write_sync("开始热油", strlen("开始热油"), 50);
        LOGI("aux","煎模式(热油阶段): 进入热油");
        set_fsm_time(fsm);
        temp_value_last = save_current_temp(temp_mid);

        up_trend_flag       = false;
        dn_trend_flag       = false;
        time_down_trend     = 0;    //下降趋势的起始时间
        temp_dn_trend       = 0;    //下降趋势的最低温度
        temp_up_trend       = 0;    //下降趋势的最高温度    
    
        temp_arrivate       = false;
        is_remind           = false;
        time_remind         = 0;    //温度到达时间
        time_warn_1         = 0;    //第一次警告的时间
        time_warn_2         = 0;    //第二次警告的时间    
    }

    double slope;
    bool   slope_flag = false;
    if ((aos_now_ms() - temp_value_last.time) >= (3*1000))  //每隔3秒钟就判断1次是否切换状态（如果用户下油了，就切换到热油状态）
    {
        static int slope_cnt = 0;
        temp_value_t temp_value_now = {
            .time = aos_now_ms(),
            .temp = temp_mid
        };

        slope = calculateSlope(temp_value_last, temp_value_now);
        slope_cnt++;
        slope_flag = true;
        LOGI("aux","煎模式(热油阶段): *****判断斜率(%d)***** (%d %d) ===> (%d %d) [斜率=%f · 最新温度=%d · 中位数温度=%d]", slope_cnt,
            (int)(temp_value_last.time/1000), temp_value_last.temp, 
            (int)(temp_value_now.time/1000),  temp_value_now.temp, 
            slope, temp_cur, temp_mid);

        temp_value_last = temp_value_now;
    }

    if (temp_cur < 200*10)
    {
        if (!temp_arrivate)
        {
            change_multivalve_gear(0x01, INPUT_RIGHT); //随意控制
        }
        else
        {
            if (time_warn_1 == 0)
            {
                if (temp_cur < (180*10))
                {
                    change_multivalve_gear(0x01, INPUT_RIGHT); 
                }
            }
            else
            {
                // 这个时候需要开大火吗？ 报警1触发后的温度下降,因为报警1触发后开7档，有可能温度就下来了，这时在开1档位不是白做了吗？
                // 无法区分这时温度低是因为报警开7档导致的还是因为用户突然下油下菜介入后导致的

                if (temp_cur < 150*10)
                {
                    change_multivalve_gear(0x01, INPUT_RIGHT);
                    LOGI("aux", "煎模式(热油阶段): 报警1触发后的温降 (用户介入 %d)", temp_cur);
                }
            }
        }

        //如果放油后，马上放菜，能识别吗？ 因为没有一个温度上升的过程了
        if (!is_fsm_timeout(fsm, 6000))  //锅里有菜,直接跳转到热菜阶段
        {
            if (slope_flag && slope < 10.0)
            {
                LOGI("aux", "煎模式(热油阶段): 温升较慢 (⇨炒食材)");
                switch_fsm_state(fsm, jian_heat_food);
            }
        }

        if (!up_trend_flag) //等待温度上升
        {
            LOGI("aux","煎模式(热油阶段): wait up_trend_flag 5");
            if (judge_cook_trend_rise(aux_handle->temp_array, ARRAY_DATA_SIZE, 5))
            {
                up_trend_flag = true;
                LOGI("aux","煎模式(热油阶段): 检测到温度上升 ↑↑↑");
            }
        }
        else    // 温度已经上升
        {
            if (!dn_trend_flag) // 等待温度下降
            {
                LOGI("aux","煎模式(热油阶段): wati dn_trend_flag 3");
                int diff = ( (int)(aux_handle->temp_array[0]) - (int)(aux_handle->temp_array[ARRAY_DATA_SIZE - 1]) );
                bool res = judge_cook_trend_down(aux_handle->temp_array, ARRAY_DATA_SIZE, 3);
                if (res && (diff >= 300))
                {
                    dn_trend_flag   = true;  //只检测到温度下降，但是不知道是因为加了葱姜蒜还是加了食材
                    time_down_trend = aos_now_ms();
                    temp_dn_trend   = temp_min;
                    temp_up_trend   = temp_max;
                    LOGI("aux", "煎模式(热油阶段): 检测到温度骤降 ↓↓↓ (温度=%d 温差=%d)",  temp_mid, diff);
                    return;                    
                }

            }
            else    // 温度已经下降
            {
                if (temp_min < temp_dn_trend)
                {
                    temp_dn_trend = temp_min;
                }

                if (temp_max > temp_up_trend)
                {
                    temp_up_trend = temp_max;
                }

                if ((aos_now_ms() - time_down_trend) >= 10*1000)
                {
                    LOGI("aux","煎模式(热油阶段): 10秒内 最低温度=%d ===> 本轮最高=%d 差值=%d", temp_dn_trend, temp_max, temp_max-temp_dn_trend);
                    if (temp_max < 100)
                    {
                        LOGI("aux", "煎模式(热油阶段): 温升较慢 (⇨ 煎食材)");
                        switch_fsm_state(fsm, jian_heat_food);
                    }
                    else
                    {
                        LOGI("aux", "煎模式(热油阶段): 温升较快 (⇨ 继续热油)");
                        switch_fsm_state(fsm, jian_heat_oil);
                    }
                }
            }
        }
    }
    else
    {
        change_multivalve_gear(0x07, INPUT_RIGHT); //火力调为7档
        temp_arrivate = true;
        if (!is_remind)
        {
            udp_voice_write_sync("请下食材", strlen("请下食材"), 50);
            beep_control_cb(0x02);
            is_remind = true;
            time_remind = aos_now_ms();
        }
    }


    if (is_remind)
    {
        if (time_warn_1 == 0)
        {
            if (aos_now_ms() - time_remind > 30*1000)
            {
                udp_voice_write_sync("为防止干烧, 切换小火", strlen("为防止干烧, 切换小火"), 50);
                beep_control_cb(0x02);  
                change_multivalve_gear(0x07, INPUT_RIGHT); //火力7档  
                time_warn_1 = aos_now_ms();
            }
        }
        else
        {
            if (time_warn_2 == 0)
            {
                if (aos_now_ms() - time_warn_1 > 10*1000)
                {
                    udp_voice_write_sync("为防止干烧, 关闭右灶", strlen("为防止干烧, 关闭右灶"), 50);
                    beep_control_cb(0x02);  
                    aux_close_fire_cb(INPUT_RIGHT);              
                    time_warn_2 = aos_now_ms();

                    aux_handle->aux_switch = 0;                 
                    if(aux_exit_cb != NULL)
                    {
                        aux_exit_cb(AUX_SUCCESS_EXIT);
                    }            
                }
            }
        }
    }
}

void jian_heat_food(func_ptr_fsm_t* fsm, aux_handle_t *aux_handle)
{
    static temp_value_t temp_value_last = {0x00};
    uint16_t temp_cur = aux_handle->temp_array[ARRAY_DATA_SIZE - 1];            // 最新温度
    uint16_t temp_mid = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);    // 中位温度
    uint16_t temp_min = findMin(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最小温度
    uint16_t temp_max = findMax(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最大温度
    
    static bool up_trend_flag       = false;
    static uint64_t time_up_trend   = 0;
    static uint64_t time_warn_1     = 0;    //第一次警告的时间
    static uint64_t time_warn_2     = 0;    //第二次警告的时间

    static bool temp_arrivate       = false;

    static uint64_t time_turn       = 0;    //翻面的时间
    static uint64_t time_turn_1     = 0;    //翻面提醒1
    static uint64_t time_turn_2     = 0;    //翻面提醒2

    static uint64_t time_switch     = 0;
    static int      cnt_switch      = 0;

    if (fsm->state_time == 0)
    {
        set_fsm_time(fsm);
        temp_value_last = save_current_temp(temp_mid);

        udp_voice_write_sync("开始煎食材", strlen("开始煎食材"), 50);
        LOGI("aux","煎模式(煎): 进入煎食材");

        temp_arrivate = false;
    
        up_trend_flag = false;
        time_up_trend = 0;
        time_warn_1   = 0;    //第一次警告的时间
        time_warn_2   = 0;    //第二次警告的时间

        time_turn     = aos_now_ms();
        time_turn_1   = 0;
        time_turn_2   = 0;

        time_switch   = 0;
        cnt_switch    = 0;
    }

    double slope;
    bool   slope_flag = false;
    if ((aos_now_ms() - temp_value_last.time) >= (3*1000))  //每隔3秒钟就判断1次是否切换状态（如果用户下油了，就切换到热油状态）
    {
        static int slope_cnt = 0;
        temp_value_t temp_value_now = 
        {
            .time = aos_now_ms(),
            .temp = temp_mid
        };

        slope = calculateSlope(temp_value_last, temp_value_now);
        slope_cnt++;
        slope_flag = true;
        LOGI("aux","煎模式(煎): *****判断斜率(%d)***** (%d %d) ===> (%d %d) [斜率=%f · 最新温度=%d · 中位数温度=%d]", 
            slope_cnt,
            (int)(temp_value_last.time/1000), temp_value_last.temp, 
            (int)(temp_value_now.time/1000),  temp_value_now.temp, 
            slope, temp_cur, temp_mid);

        temp_value_last = temp_value_now;
    }

    // if (temp_cur < (100*10))
    // {
    //     if ((time_warn_1 == 0) && (time_turn_1 == 0)&& (time_turn_2 == 0)) //非0就是锁定档位
    //     {
    //         if (!temp_arrivate)
    //         {
    //             change_multivalve_gear(0x03, INPUT_RIGHT);
    //         }
    //         else
    //         {
    //             if (temp_cur < (80*10))
    //             {
    //                 change_multivalve_gear(0x03, INPUT_RIGHT);
    //             }
    //         }    
    //     }
    // }
    // else
    // {
    //     temp_arrivate = true;

    //     if ((time_warn_1 == 0) && (time_turn_2 == 0)) //非0就是锁定档位
    //     {
    //         change_multivalve_gear(0x06, INPUT_RIGHT);
    //     }
    // }

    if ((time_warn_1 == 0) && (time_turn_1 == 0) && (time_turn_2 == 0)) //不处于锁定状态
    {
        if (!is_fsm_timeout(fsm, 30*1000))
        {
            change_multivalve_gear(0x03, INPUT_RIGHT);
        }
        else
        {
            if ((time_switch == 0) || ((aos_now_ms() - time_switch) >= 10*1000))
            {
                if ((cnt_switch & 0x01) == 0x00)
                {
                    if (!is_fsm_timeout(fsm, 120*1000))
                        change_multivalve_gear(0x06, INPUT_RIGHT);
                    else
                        change_multivalve_gear(0x07, INPUT_RIGHT);
                }
                else
                {
                    if (!is_fsm_timeout(fsm, 120*1000))
                        change_multivalve_gear(0x03, INPUT_RIGHT);
                    else
                        change_multivalve_gear(0x05, INPUT_RIGHT);
                }

                time_switch = aos_now_ms();  
                cnt_switch++;  
            }
        }
    }

    bool body_exist_flag = false;
    if (check_any_large_diff(aux_handle->temp_array, ARRAY_DATA_SIZE))  //温度波动是否大于5
    {
        body_exist_flag = true;
        LOGI("aux","煎模式(煎): 有人");  //960 960 960 970 960 970 970 970 960 960
    }
    else
    {
        body_exist_flag = false;
        LOGI("aux","煎模式(煎): 无人存在!!!");
    }

    if (!body_exist_flag)  //无人存在
    {
        if (time_up_trend == 0)
        {
            time_up_trend = aos_now_ms();//首次进入温度上升的时间
        }
        else
        {
            int time_diff = (aos_now_ms() - time_up_trend);
            if (time_diff > 80*1000)
            {
                if (time_warn_1 == 0)
                {
                    udp_voice_write_sync("防止干烧,切换小火", strlen("防止干烧,切换小火"), 50);
                    beep_control_cb(0x02);  
                    change_multivalve_gear(0x07, INPUT_RIGHT); //火力7档  
                    time_warn_1 = aos_now_ms();
                }
                else
                {
                    if (time_warn_2 == 0)
                    {
                        if ((aos_now_ms() - time_warn_1) > 60*1000)
                        {
                            udp_voice_write_sync("防止干烧,关闭右灶", strlen("防止干烧,关闭右灶"), 50);
                            beep_control_cb(0x02);  
                            aux_close_fire_cb(INPUT_RIGHT);             
                            time_warn_2 = aos_now_ms();
                
                            aux_handle->aux_switch = 0;                 
                            if(aux_exit_cb != NULL)
                            {
                                aux_exit_cb(AUX_SUCCESS_EXIT);
                            }   
                        }         
                    }
                }
            }
        }
    }
    else
    {
        time_up_trend = 0;
        time_warn_1   = 0;
        time_warn_2   = 0;
    }

    bool food_turn_flag = judge_cook_trend_turn_over(aux_handle->temp_array, ARRAY_DATA_SIZE);
    if (!food_turn_flag)
    {
        if (time_turn_1 == 0)
        {
            if ((aos_now_ms() - time_turn) > 60*1000)
            {
                udp_voice_write_sync("防止焦糊,请翻面", strlen("防止焦糊,请翻面"), 50);
                beep_control_cb(0x02);  
                change_multivalve_gear(0x06, INPUT_RIGHT);
                time_turn_1 = aos_now_ms();  //更新上次翻面时间
            }    
        }
        else
        {
            if (time_turn_2 == 0)
            {
                if ((aos_now_ms() - time_turn_1) > 15*1000)
                {
                    udp_voice_write_sync("防止焦糊,切换小火", strlen("防止焦糊,切换小火"), 50);
                    beep_control_cb(0x02);  
                    change_multivalve_gear(0x07, INPUT_RIGHT);  
                    time_turn_2 = aos_now_ms();
                }
            }
        } 
    }
    else
    {
        time_turn   = aos_now_ms();  //更新上次翻面时间
        time_turn_1 = 0;
        time_turn_2 = 0;
    }   
}

/**
 * @brief:
 * @param {aux_handle_t} *aux_handle
 * @return {*}
 */
void mode_jian_func(aux_handle_t *aux_handle)
{
    if (aux_handle->fsm_jian.state_func == NULL)  //state_func为空，说明是首次启动状态机，赋一个默认状态
    {
        switch_fsm_state(&aux_handle->fsm_jian, jian_heat_pan_oil);
    }

    run_fsm(&aux_handle->fsm_jian, aux_handle); //(*(aux_handle->fsm_jian.state_func))(&aux_handle->fsm_jian, aux_handle);
}

/**
 * @brief:辅助烹饪参数重置函数
 * @param {aux_handle_t} *aux_handle
 * @return {*}
 */
void aux_cook_reset(aux_handle_t *aux_handle)
{
    memset(&aux_handle->fsm_jian, 0x00, sizeof(aux_handle->fsm_jian));
    aux_handle->fsm_jian_loop_cnt = 0;
    memset(&aux_handle->fsm_chao, 0x00, sizeof(aux_handle->fsm_chao));
    aux_handle->fsm_chao_loop_cnt = 0;
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
    static uint64_t time_mem = 0;
    aux_handle_t *aux_handle = get_aux_handle(input_dir);

    //aux_handle->aux_total_tick++;
    //aux_temp_save(aux_handle, temp);
    //LOGI("aux","present target temp:%d,environment temp:%d ",temp,environment_temp);

    if (aux_handle->aux_switch == 0 || aux_handle->ignition_switch == 0)
    {
        if ((time_mem == 0) || (aos_now_ms() - time_mem) >= (1500))
        {
            time_mem = aos_now_ms();
            LOGI("aux","辅助烹饪未启动: 目标温度=%d, 环境温度=%d", temp/10, environment_temp/10);
        }

        cook_aux_init(INPUT_RIGHT);
        aux_cook_reset(aux_handle);
        return;
    }

    if(aux_handle->pan_fire_status == 1)
    {
        LOGW("aux","发生移锅小火，退出辅助烹饪");
        udp_voice_write_sync("发生移锅小火", strlen("发生移锅小火"), 50);
        cook_aux_init(INPUT_RIGHT);
        aux_exit_cb(AUX_SUCCESS_EXIT); //AUX_ERROR_EXIT
        return;
    }

    aux_handle->aux_total_tick++;
    aux_temp_save(aux_handle, temp);
    printf_cur_temp_array(aux_handle, temp);

    // if(aux_handle->aux_total_tick >= MAX_FIRE_TIME)
    // {
    //     LOGI("aux","兜底关火时间%d到，关闭火焰 ",MAX_FIRE_TIME/4);
    //     aux_close_fire_cb(INPUT_RIGHT);         //右灶关火
    //     aux_handle->aux_switch = 0;             //退出辅助烹饪模式
    // }

    if (aux_handle->temp_size < 10)
    {
        LOGI("aux", "temp data is not enough, func return");
        return;
    }

    switch (aux_handle->aux_type)
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
            LOGI("aux","Error cook Mode ");
            return;
        }
    }
}

void mars_ac_init()
{
    unsigned char value = 0;
    int len = sizeof(value);
    int ret = aos_kv_get(AUX_COOK_VOICE_KV, &value, &len);
    if (ret == 0 && len > 0) 
    {
        if (value == 0x31)
        {
            voice_switch = 1;
            LOGW("mars", "打开辅助烹饪的语音开关");
        }
    }
}
