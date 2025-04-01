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

ring_buffer_t fire_gear_queue;

typedef struct {
    uint64_t time;
    uint16_t temp;
}temp_value_t;

temp_value_t save_current_temp(uint16_t cur_temp)
{
    temp_value_t value = {
        .time = aos_now_ms(),
        .temp = cur_temp
        };

    return value;
}

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

void printf_cur_temp_array(aux_handle_t* aux_handle)
{
    uint8_t szArray[128] = {0x00};
    for (int i=0; i<ARRAY_DATA_SIZE; i++)
    {
        sprintf(szArray + strlen(szArray), "%d ", aux_handle->temp_array[i]);
    }
    LOG_GRE("数据统计: %s  (最小值=%d 最大值=%d 平均值=%d 中位值=%d)",
    szArray,
    findMin(aux_handle->temp_array, ARRAY_DATA_SIZE),
    findMax(aux_handle->temp_array, ARRAY_DATA_SIZE),
    findAver(aux_handle->temp_array, ARRAY_DATA_SIZE),
    findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE));
}

void udp_voice_init(void)
{
    fd = (intptr_t)HAL_UDP_create_without_connect(NULL, 5711);
    if ((intptr_t) - 1 == fd)
    {
        LOG_BAI("error: HAL_UDP_create_without_connect fail");
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

    LOG_BAI("播报语音消息: %s  (%s:%d)", p_data, udp_dest_addr.addr, udp_dest_addr.port);
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
        LOG_BAI("语音消息·进入队列 (0x%08X %s)", buff, p_data);
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
        LOG_BAI("语音消息·移出队列 (0x%08X %s:%d)", buff, udp_dest_addr.addr, udp_dest_addr.port);
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
char *boil_status_info[]={"idle","gentle","rise","down","boiled"};
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
    //     LOG_BAI("error: 目标档位(%d)与当前档位(%d)一致，不下发切换指令", gear, aux_handle->aux_multivalve_gear);
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
    //     LOG_BAI("error: 上一个切换(目标%d档)正在进行中，不下发切换指令", aux_handle->aux_aim_multivalve_gear);
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
    //         LOG_BAI("error: 上一个切换正在进行中......(%d)", aux_handle->aux_aim_multivalve_gear);
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
        LOG_BAI("error: 目标档位(%d)与当前档位(%d)一致，不下发切换指令", gear, aux_handle->aux_multivalve_gear);
        return -1;
    }

    //煮模式最后两分钟,只接受1档设置
    if(aux_handle->aux_type == MODE_ZHU && aux_handle->aux_remain_time <= 2 * 60 * INPUT_DATA_HZ && gear != 0x01)
    {
        return -1;
    }

    //判断尚未到达目标档位，不进行档位调整
    if(aux_handle->aux_multivalve_gear != aux_handle->aux_aim_multivalve_gear)
    {
        LOG_BAI("error: 上一个火力切换(目标%d档)正在进行中，不下发切换指令", aux_handle->aux_aim_multivalve_gear);
        return -1;
    }

    if(multi_valve_cb != NULL)
    {
        aux_handle->aux_aim_multivalve_gear = gear;
        multi_valve_cb(INPUT_RIGHT, gear);
        snprintf(voice_buff, sizeof(voice_buff), "切换火力%d档", gear);
        udp_voice_write_sync(voice_buff, strlen(voice_buff), 50);
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

    LOG_BAI("aux seting:============================== \
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
    static uint16_t window_average[ARRAY_DATA_SIZE] = {0x00};
    static uint8_t  window_average_size = 0;

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
        LOG_BAI("aux deal temp data error!!!");
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
            LOG_BAI("aux deal average temp data error!!! ");
        }
    }

    // window_average[window_average_size] = average_temp;
    // window_average_size++;
    // if(aux_handle->aux_total_tick % ARRAY_DATA_SIZE == 0)
    // {
    //     uint8_t szArray[128] = {0x00};
    //     uint8_t szArrayAverage[128] = {0x00};
    //     for (int i=0; i<ARRAY_DATA_SIZE; i++)
    //     {
    //         sprintf(szArray + strlen(szArray), "%d ", aux_handle->temp_array[i]);
    //         sprintf(szArrayAverage + strlen(szArrayAverage),  "%d ", window_average[i]);
    //     }

    //     LOG_GRE("*****************************");
    //     LOG_GRE("统计 实时温度: %s", szArray);
    //     //LOG_BAI("统计 平均温度(隔离): %d", average_temp);
    //     //LOG_BAI("统计 平均温度(连续): %s", szArrayAverage);

    //     memset(window_average, 0x00, sizeof(window_average));
    //     window_average_size = 0x00;
    // }
}


/**
 * @brief:判断水开的函数
 * @param {aux_handle_t} *aux_handle
 * @return {*} 0:水未开，1：水已开
 */
int judge_water_boil(aux_handle_t *aux_handle)
{
    LOG_BAI("enter boil judge");
    //煮模式中的保底时间到，尚未发生过水开场景，保底认为水开
    if(aux_handle->aux_total_tick >= BOIL_MODE_MAX_HEAT_TIME && aux_handle->aux_type == MODE_ZHU &&\
     aux_handle->enter_boil_time < 1)
    {
        LOG_BAI("煮模式保底时间到认为已经是水开");
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
            LOG_BAI("[%s]very_gentle_temp get boiled!!! ",__func__);
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

        LOG_BAI("[%s],rise_count1:%d,rise_count2:%d ",__func__, rise_count1, rise_count2);
        if(rise_count1 >= 6 || rise_count2 >= 3)
        {
            LOG_BAI("[%s]认为仍然处于缓慢上升状态 ",__func__);
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
            LOG_BAI("[change_status],to idle ");
            aux_handle->boil_current_tendency = aux_handle->boil_next_tendency;
            aux_handle->boil_current_status_tick = 0;
            aux_handle->boil_next_status_tick = 0;
        }
    }
    else if(aux_handle->boil_current_tendency == RISE && aux_handle->boil_next_tendency != RISE)
    {
        if(aux_handle->boil_next_status_tick > 4 * 6)
        {
            LOG_BAI("[change_status],rise to %s ",boil_status_info[aux_handle->boil_next_tendency]);
            aux_handle->boil_current_tendency = aux_handle->boil_next_tendency;
            aux_handle->boil_current_status_tick = 0;
            aux_handle->boil_next_status_tick = 0;
        }
    }
    else if(aux_handle->boil_current_tendency == DOWN && aux_handle->boil_next_tendency != DOWN)
    {
        if(aux_handle->boil_next_status_tick > 4 * 2)
        {
            //第二次水开之前切换到了down都会停止计时，再次沸后计时。为了实现冷锅直接开火首次沸腾开始计时，或者首次放入食材后开始计时，后续再次放入食材不再进行计时。
            if(aux_handle->enter_boil_time <= 1)
            {
                aux_handle->aux_boil_counttime_flag = 0;    //停止计时
                aux_handle->aux_remain_time = aux_handle->aux_set_time * 60 * AUX_DATA_HZ;

                if(aux_remaintime_cb != NULL)
                {
                    int left_min = aux_handle->aux_remain_time / (60 * AUX_DATA_HZ);
                    LOG_GRE("重置倒计时,left_min:%d",left_min);
                    //回调函数通知电控显示倒计时时间发生了变化，倒计时时间的单位是min
                    aux_remaintime_cb(left_min);
                }
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
            LOG_BAI("要切换的是gentle，仍然保持boil状态");
        }
        //status change: BOIL->DOWN
        else if(aux_handle->boil_next_tendency == DOWN)
        {
            // //已经发生过水开的情况了
            // if(aux_handle->aux_boil_type == BOIL_20_MAX && aux_handle->aux_total_tick >= 10 * 60 * INPUT_DATA_HZ)
            // {
            //     LOG_BAI("总时间大于20min，处于10Min后，八段阀切换为大火3档 ");
            //     change_multivalve_gear(0x03, INPUT_RIGHT);
            // }
            // else
            //{
                LOG_BAI("水沸状态切换到下降状态，八段阀切换为大火1档 ");
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
    // LOG_BAI("max up value:%d", aux_handle->max_up_value);

    //do...while中状态判断逻辑
    do
    {
        //上升趋势========================================================
        if(aux_handle->temp_array[0] - aux_handle->temp_array[ARRAY_DATA_SIZE - 1] <= 0)
        {
            LOG_BAI("begin rise tendency judge");
            unsigned char very_gentle_count = 0;
            unsigned char rise_count2 = 0;
            //两两相邻数据之间的9次比较
            for(int i = 0; i < ARRAY_DATA_SIZE - 1; i++)
            {
                if(aux_handle->temp_array[i] == aux_handle->temp_array[i+1])
                {
                    very_gentle_count++;
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

            LOG_BAI("very_gentle_count:%d,rise_count2:%d,rise_count3:%d", very_gentle_count, rise_count2, rise_count3);
            //rise_count3 = (aux_handle->average_temp_array[aux_handle->average_temp_size - 1] - aux_handle->average_temp_array[aux_handle->average_temp_size - 2] >= 10);
            //rise_count3 +=
            //仅仅靠10个温度难以判断是rise还是gentle
            //if((rise_count1 >= 7 && rise_count2 >= 4) ||
            //此处要防止误判为rise状态
            if(rise_count2 >= 4 || (rise_count3 >= 7 && very_gentle_count <= 7))
            {
                aux_handle->boil_next_tendency = RISE;
                LOG_BAI("[%s]present state is rising ",__func__);
                break;
            }
        }
        else
        {
            LOG_BAI("begin down tendency judge");
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
                LOG_BAI("[%s]present state is down ",__func__);
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
            //         LOG_BAI("[%s]present state is down ",__func__);
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

        if( gentle_count1 == 9 || abs(before_average - after_average) < 15)
        {
            aux_handle->boil_next_tendency = GENTLE;
            LOG_BAI("[%s]present state is gentle ",__func__);
            break;
        }

        aux_handle->boil_next_tendency = IDLE;
        break;
    }while(1);

    //待切换状态变化，则计时清零，开始对新状态进行计时
    if(aux_handle->boil_next_tendency != before_next_tendency)
    {
        LOG_BAI("next status change");
        before_next_tendency = aux_handle->boil_next_tendency;
        aux_handle->boil_next_status_tick = 0;
    }
    //状态未发生变化，继续对next_status_tick计时
    else
    {
        aux_handle->boil_next_status_tick++;
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
            LOG_BAI("切换到水开状态 ");

            //进入水开次数累加
            aux_handle->enter_boil_time++;

            // //前两次进入水沸状态都重新倒计时，这里对当前倒计时进行清零。对应的是冷水放食材直接煮开到结束，或者冷水煮开后再放食材煮沸后到结束
            // if(aux_handle->enter_boil_time <= 2)
            // {
            //     aux_handle->aux_remain_time = aux_handle->aux_set_time * 60 * AUX_DATA_HZ;
            // }

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
            LOG_GRE("ready send left min to dis,left_min:%d",left_min);
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

        if(aux_handle->enter_boil_time == 1)
        {
            LOG_BAI("首次沸腾，开始倒计时 ");

        }
        else if(aux_handle->enter_boil_time == 2)
        {
            LOG_BAI("第二次沸腾，开始倒计时 ");
        }
        else if(aux_handle->enter_boil_time > 2)
        {
            //后续进入沸腾仍然按照之前的倒计时进行
            LOG_BAI("后续沸腾，继续计时 ");
        }
    }



    //煮模式计时结束且当前处于沸腾状态，关火，通知电控模式成功退出
    if(aux_handle->aux_remain_time == 0 && aux_handle->boil_current_tendency == BOILED)
    {
        LOG_BAI("boil count down time is on,close fire[boil success end!!!] ");
        if(aux_close_fire_cb != NULL)
        {
            aux_close_fire_cb(INPUT_RIGHT);             //右灶关火
        }

        aux_handle->aux_switch = 0;                 //辅助烹饪煮模式结束
        if(aux_exit_cb != NULL)
        {
            aux_exit_cb(AUX_SUCCESS_EXIT);              //成功退出
        }

        LOG_RED("煮模式成功执行完成,八段阀复位,boil mode run finish,multi valve reset");
        change_multivalve_gear(0x00, INPUT_RIGHT);
        cook_aux_reinit(INPUT_RIGHT);
    }
    else if(aux_handle->aux_remain_time <= 2 * 60 * INPUT_DATA_HZ && aux_handle->aux_multivalve_gear != 0x01)
    {
        LOG_RED("最后两分钟切换为1档火焰,last 2 min valve change to gear 1");
        change_multivalve_gear(0x01, INPUT_RIGHT);
    }

    aux_handle->boil_current_status_tick++;

    LOG_BAI("boil_remain_time:%d,enter_boil_time:%d ",aux_handle->aux_remain_time,aux_handle->enter_boil_time);
    LOG_BAI("[%s]current status:[%s],total_tick:%d,current_status_tick:%d,next_status:%s,next_status_tick:%d,current gear:%d ",\
    __func__,boil_status_info[aux_handle->boil_current_tendency ],aux_handle->aux_total_tick,aux_handle->boil_current_status_tick,\
    boil_status_info[aux_handle->boil_next_tendency],aux_handle->boil_next_status_tick,aux_handle->aux_multivalve_gear);
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

    if (aux_handle->aux_total_tick == 1)  //开启炸模式后的第一个温度
    {
        LOG_BAI("enter fry mode temp:%d ", aux_handle->current_average_temp);
        enter_mode_temp = aux_handle->current_average_temp;
        aux_handle->fry_step = 0;               //重置炸场景步骤
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
            //LOG_BAI("初始阶段，5s内不做处理 ");
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
                    LOG_GRE("30s没有检测到处于何种阶段，直接设定为热油阶段");
                    aux_handle->fry_step = 2;
                }
            }
        }

        //连续3s都判断为此状态，因为10组温度数据代表的是2.5s内的温度数据，3s仍为此状态比较稳妥的判断当前的步骤
        if (aux_handle->rise_quick_tick >= 3 * AUX_DATA_HZ)
        {
            LOG_GRE("检测到处于热锅阶段(%d)", aux_handle->rise_quick_tick);
            aux_handle->fry_step = 1;
        }
        else if (aux_handle->rise_slow_tick >= 3 * AUX_DATA_HZ)
        {
            LOG_GRE("检测到处于热油阶段(%d)", aux_handle->rise_slow_tick);
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
        if(aux_handle->current_average_temp >= 180 * 10 && aux_handle->current_average_temp < 200 * 10 && aux_handle->first_hot_pot_flag == 0)
        {
            change_multivalve_gear(0x04, INPUT_RIGHT);
            aux_handle->fry_last_change_gear_tick = 0;
            aux_handle->first_hot_pot_flag = 1;
            if(beep_control_cb != NULL)
            {
                beep_control_cb(0x02);          //提醒用户加油
            }
        }
        else if(aux_handle->current_average_temp > 200 * 10)
        {
            change_multivalve_gear(0x07, INPUT_RIGHT);
            aux_handle->fry_last_change_gear_tick = 0;
        }
    }

    //已经检测到热锅之后或者已经超过了30s,开始判断是否放入油或食材;即默认热锅最多30s
    if ((aux_handle->fry_step == 1  || aux_handle->aux_total_tick > 30 * 4) && aux_handle->first_put_food_flag == 0)
    {
        //0.25s相邻的两个温度发生大于5度的波动
        if(abs(aux_handle->temp_array[ARRAY_DATA_SIZE - 1] - aux_handle->temp_array[ARRAY_DATA_SIZE - 2]) > 50)
        {
            aux_handle->fry_step = 2;              //进入到了控温步骤
            aux_handle->first_put_food_flag = 1;
            LOG_BAI("判断可能是放入了食用油或食材 %d", abs(aux_handle->temp_array[ARRAY_DATA_SIZE - 1] - aux_handle->temp_array[ARRAY_DATA_SIZE - 2]));
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
        else if(aux_handle->current_average_temp > aux_handle->aux_set_temp * 10 - 20 * 10)
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
        if(aux_handle->current_average_temp >= aux_handle->aux_set_temp * 10 - 10 * 10)
        {
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
            LOG_BAI("max up value:%d ",aux_handle->max_up_value);

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
            LOG_GRE("首次达到设定的目标温度，current_average_temp:%d,set_temp:%d",aux_handle->current_average_temp, aux_handle->aux_set_temp * 10);
            beep_control_cb(0x02);
            aux_handle->first_reach_set_temp_flag = 1;
        }
    }

    if(aux_handle->fry_step == 4 && gentle_flag == true)
    {
        if(aux_handle->fry_last_change_gear_tick >= 10 * 4 && aux_handle->current_average_temp >= aux_handle->aux_set_temp * 10 - 5 * 10)
        {
            unsigned char gear = 0;
            //温度高于目标温度5摄氏度以上，降低火力操作（档位越大火力越小）
            if(aux_handle->current_average_temp >= aux_handle->aux_set_temp * 10 + 10 * 5)
            {
                gear = aux_handle->aux_multivalve_gear + 3;
                LOG_BAI("控温阶段: 修正火力档位 %d 档 (x+3) ", gear);
            }
            else if(aux_handle->current_average_temp >= aux_handle->aux_set_temp * 10)
            {
                gear = aux_handle->aux_multivalve_gear + 2;
                LOG_BAI("控温阶段: 修正火力档位 %d 档 (x+2) ", gear);
            }
            else if(aux_handle->current_average_temp >= aux_handle->aux_set_temp * 10 - 5 * 10)
            {
                gear = aux_handle->aux_multivalve_gear + 1;
                LOG_BAI("控温阶段: 修正火力档位 %d 档 (x+1) ", gear);
            }
            aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;
            change_multivalve_gear(gear, INPUT_RIGHT);
            aux_handle->fry_last_change_gear_tick = 0;

        }
        else if(aux_handle->fry_last_change_gear_tick >= 10 * 4 && aux_handle->current_average_temp < aux_handle->aux_set_temp * 10 - 5 * 10)
        {
            //温度高于目标温度10摄氏度以上，增大火力操作（档位越小火力越大）
            unsigned char gear = aux_handle->aux_multivalve_gear - 1;
            LOG_BAI("控温阶段: 修正火力档位 %d 档 (x-1) ", gear);
            aux_handle->fry_last_gear_average_temp = aux_handle->current_average_temp;
            change_multivalve_gear(gear, INPUT_RIGHT);
            aux_handle->fry_last_change_gear_tick = 0;
        }
    }

    LOG_BAI("current step:%d,current gear:%d ",aux_handle->fry_step, aux_handle->aux_multivalve_gear);
    return;
}

void chao_heat_pan(func_ptr_fsm_t* fsm, aux_handle_t *aux_handle)
{
    //去除温度曲线的毛刺，借助chatgpt，从第一个稳定的温度开始（如何找第一个稳定的温度：连续10个温度，相邻两个温度之间不超过3度），相邻两个温度的绝对值不能超过±3度，超过了就把这个温度去除掉
    static temp_value_t temp_value_last = {0x00};
    uint16_t temp_cur = aux_handle->temp_array[ARRAY_DATA_SIZE - 1];            // 最新温度
    uint16_t temp_mid = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);    // 中位温度
    uint16_t temp_min = findMin(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最小温度
    uint16_t temp_max = findMax(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最大温度
    static bool fire_1_switch_flag = false;
    static bool fire_4_switch_flag = false;
    static bool fire_7_switch_flag = false;
    static bool temp_arrivate = false;
    static bool is_remind = false;

    if (fsm->state_time == 0)
    {
        udp_voice_write_sync("开始热锅", strlen("开始热锅"), 50);
        LOG_BAI("炒模式: 进入热锅");
        set_fsm_time(fsm);
        temp_value_last = save_current_temp(findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE));  //cur_temp

        fire_1_switch_flag = false;
        fire_4_switch_flag = false;
        fire_7_switch_flag = false;
        is_remind = false;
        temp_arrivate = false;
    }

    //一直处于热锅阶段里面
    if (temp_mid < 190*10)
    {
        if (!fire_1_switch_flag)
        {
            change_multivalve_gear(0x01, INPUT_RIGHT); //火力1档
            fire_1_switch_flag = true;
            fire_4_switch_flag = false;
            fire_7_switch_flag = false;
        }

        if (temp_arrivate && (temp_mid < 150*10))
        {
            printf_cur_temp_array(aux_handle);
            LOG_BAI("炒模式: 温度骤降 ===> 热锅结束 (耗时%d秒 温度=%d)", (int)((aos_now_ms() - fsm->state_time)/1000), temp_mid);
            switch_fsm_state(fsm, chao_heat_oil);
        }
    }
    else if (temp_mid < 210*10)
    {
        temp_arrivate = true;
        if (!fire_4_switch_flag)
        {
            change_multivalve_gear(0x04, INPUT_RIGHT); //火力4档
            fire_4_switch_flag = true;
            fire_1_switch_flag = false;
            fire_7_switch_flag = false;
        }

        if (!is_remind)
        {
            char* tips = "请下油";
            udp_voice_write_sync(tips, strlen(tips), 50);
            is_remind = true;
        }
    }
    else
    {
        temp_arrivate = true;
        if (!fire_7_switch_flag)
        {
            change_multivalve_gear(0x07, INPUT_RIGHT); //火力7档
            fire_7_switch_flag = true;
            fire_1_switch_flag = false;
            fire_4_switch_flag = false;
        }
    }

    if ((aos_now_ms() - temp_value_last.time) >= (10*1000))  //每隔10秒钟就判断1次是否切换状态（如果用户下油了，就切换到热油状态）
    {
        temp_value_t temp_value_now = {
            .time = aos_now_ms(),
            .temp = temp_mid
        };

        double slope = calculateSlope(temp_value_last, temp_value_now);
        LOG_BAI("炒模式: *****判断状态*****(%d %d) (%d %d) %d %f", temp_value_last.time, temp_value_last.temp, temp_value_now.time, temp_value_now.temp, temp_mid, slope);

        if ((slope > 3.0) && (temp_mid <= 2000))    //根据斜率来判断当前的状态
        {
            //switch_fsm_state(fsm, chao_heat_oil);
        }

        if ((slope < 0.2) && (temp_mid <= 1700))    //根据斜率来判断当前的状态
        {
            //switch_fsm_state(fsm, chao_heat_onion);
        }

        temp_value_last = temp_value_now;
    }
}

void chao_heat_oil(func_ptr_fsm_t* fsm, aux_handle_t *aux_handle)
{
    static temp_value_t temp_value_last = {0x00};
    uint16_t temp_cur = aux_handle->temp_array[ARRAY_DATA_SIZE - 1];            // 最新温度
    uint16_t temp_mid = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);    // 中位温度
    uint16_t temp_min = findMin(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最小温度
    uint16_t temp_max = findMax(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最大温度
    static bool fire_1_switch_flag = false;
    static bool fire_4_switch_flag = false;
    static bool fire_7_switch_flag = false;
    static bool temp_arrivate = false;
    static bool is_remind = false;

    if (fsm->state_time == 0)
    {
        udp_voice_write_sync("开始热油", strlen("开始热油"), 50);
        LOG_BAI("炒模式: 进入热油");
        set_fsm_time(fsm);
        temp_value_last = save_current_temp(findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE));

        fire_1_switch_flag = false;
        fire_4_switch_flag = false;
        fire_7_switch_flag = false;
        is_remind = false;
        temp_arrivate = false;
    }

    if (temp_mid < 200*10)
    {
        if (!fire_1_switch_flag)
        {
            LOG_BAI("炒模式: 切换火力到1档 (%d)", temp_mid);
            change_multivalve_gear(0x01, INPUT_RIGHT); //火力1档加热
            fire_1_switch_flag = true;
            fire_4_switch_flag = false;
            fire_7_switch_flag = false;
        }

        if (temp_arrivate && (temp_mid < 170*10))  //放入葱姜蒜后，温度一定小于170度？
        {
            printf_cur_temp_array(aux_handle);
            LOG_BAI("炒模式: 温度骤降 ===> 热油结束 (耗时%d秒 温度=%d)", (int)((aos_now_ms() - fsm->state_time)/1000), temp_mid);
            switch_fsm_state(fsm, chao_heat_onion);
        }
    }
    else if (temp_mid < 210*10)
    {
        temp_arrivate = true;
        if (!fire_4_switch_flag)
        {
            LOG_BAI("炒模式: 切换火力到4档 (%d)", temp_mid);
            change_multivalve_gear(0x04, INPUT_RIGHT); //火力调为4档
            fire_4_switch_flag = true;
            fire_1_switch_flag = false;
            fire_7_switch_flag = false;
        }

        if (!is_remind)
        {
            udp_voice_write_sync("请下葱姜蒜", strlen("请下葱姜蒜"), 50);
            is_remind = true;
        }
    }
    else
    {
        temp_arrivate = true;
        if (!fire_7_switch_flag)
        {
            LOG_BAI("炒模式: 切换火力到7档 (%d)", temp_mid);
            change_multivalve_gear(0x07, INPUT_RIGHT); //火力调为7档
            fire_7_switch_flag = true;
            fire_1_switch_flag = false;
            fire_4_switch_flag = false;
        }
    }
}

void chao_heat_onion(func_ptr_fsm_t* fsm, aux_handle_t *aux_handle)
{
    static temp_value_t temp_value_last = {0x00};
    uint16_t temp_cur = aux_handle->temp_array[ARRAY_DATA_SIZE - 1];            // 最新温度
    uint16_t temp_mid = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);    // 中位温度
    uint16_t temp_min = findMin(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最小温度
    uint16_t temp_max = findMax(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最大温度
    static bool fire_1_switch_flag = false;
    static bool fire_4_switch_flag = false;
    static bool fire_7_switch_flag = false;
    static bool temp_arrivate = false;
    static bool is_remind = false;

    if (fsm->state_time == 0)
    {
        udp_voice_write_sync("开始爆香", strlen("开始爆香"), 50);
        LOG_BAI("炒模式: 进入爆香");
        set_fsm_time(fsm);
        temp_value_last = save_current_temp(findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE));

        fire_4_switch_flag = false;
        fire_7_switch_flag = false;
        is_remind = false;
        temp_arrivate = false;
    }

    if (temp_mid < 170*10)
    {
        if (!fire_4_switch_flag)
        {
            LOG_BAI("炒模式: 切换火力到4档 (%d)", temp_mid);
            change_multivalve_gear(0x04, INPUT_RIGHT); //火力1档加热
            fire_4_switch_flag = true;
            fire_7_switch_flag = false;
        }

        if (temp_arrivate && (temp_mid < 100*10))  //放入食材后，温度一定小于100度？
        {
            printf_cur_temp_array(aux_handle);
            LOG_BAI("炒模式: 温度骤降 ===> 爆香结束 (耗时%d秒 温度=%d)", (int)((aos_now_ms() - fsm->state_time)/1000), temp_mid);
            switch_fsm_state(fsm, chao_heat_food);
        }
    }
    else
    {
        temp_arrivate = true;
        if (!fire_7_switch_flag)
        {
            LOG_BAI("炒模式: 切换火力到7档 (%d)", temp_mid);
            change_multivalve_gear(0x07, INPUT_RIGHT); //火力调为4档
            fire_7_switch_flag = true;
            fire_4_switch_flag = false;
        }

        if (!is_remind)
        {
            udp_voice_write_sync("请下食材", strlen("请下食材"), 50);
            is_remind = true;
        }
    }
}

void chao_heat_food(func_ptr_fsm_t* fsm, aux_handle_t *aux_handle)
{
    static temp_value_t temp_value_last = {0x00};
    uint16_t temp_cur = aux_handle->temp_array[ARRAY_DATA_SIZE - 1];            // 最新温度
    uint16_t temp_mid = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);    // 中位温度
    uint16_t temp_min = findMin(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最小温度
    uint16_t temp_max = findMax(aux_handle->temp_array,    ARRAY_DATA_SIZE);    // 最大温度
    static bool fire_0_switch_flag = false;
    static bool fire_2_switch_flag = false;
    static bool temp_arrivate = false;
    static bool is_remind = false;

    if (fsm->state_time == 0)
    {
        udp_voice_write_sync("开始炒食材", strlen("开始炒食材"), 50);
        LOG_BAI("炒模式: 进入炒食材");
        set_fsm_time(fsm);
        temp_value_last = save_current_temp(temp_mid);
        fire_0_switch_flag = false;
        fire_2_switch_flag = false;
    }

    if (temp_mid < (100*10))
    {
        if (!fire_0_switch_flag)
        {
            LOG_BAI("煎模式: 切换火力到0档 (%d)", temp_mid);
            change_multivalve_gear(0x00, INPUT_RIGHT);
            fire_0_switch_flag = true;
            fire_2_switch_flag = false;
        }
    }
    else
    {
        if (!fire_2_switch_flag)
        {
            LOG_BAI("煎模式: 切换火力到2档 (%d)", temp_mid);
            change_multivalve_gear(0x02, INPUT_RIGHT);
            fire_2_switch_flag = true;
            fire_0_switch_flag = false;
        }
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
        switch_fsm_state(&aux_handle->fsm_chao, chao_heat_pan);
    }

    run_fsm(&aux_handle->fsm_chao, aux_handle);
}

void jian_heat_pan_old(func_ptr_fsm_t* fsm, aux_handle_t *aux_handle)
{
    char voice_buff[64] = {0x00};
    static temp_value_t temp_value_last = {0x00};
    static temp_value_t temp_value_last_5 = {0x00};
    uint16_t temp_cur = aux_handle->temp_array[ARRAY_DATA_SIZE - 1];            // 最新温度
    uint16_t temp_mid = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);    // 中位数温度
    static bool is_remind = false;
    static bool fire_1_switch_flag = false;
    static bool fire_4_switch_flag = false;
    static bool fire_7_switch_flag = false;
    static bool temp_arrivate = false;

    if (fsm->state_time == 0)
    {
        udp_voice_write_sync("开始热锅", strlen("开始热锅"), 50);
        LOG_BAI("煎模式: 进入热锅");
        set_fsm_time(fsm);
        temp_value_last = save_current_temp(temp_mid);  //temp_cur
        temp_value_last_5 = temp_value_last;
        is_remind = false;
        temp_arrivate = false;
        fire_1_switch_flag = false;
        fire_4_switch_flag = false;
        fire_7_switch_flag = false;

    }

    //一直处于热锅阶段里面
    if (temp_cur < 190*10)  //大于190度且持续时间超过5秒，才切换档位，而不是那种立即切换，立即切换有可能在一直切换档位(因为当前温度有可能在200这个临界点跳来跳去)
    {
        if (!fire_1_switch_flag)
        {
            LOG_BAI("煎模式: 切换火力到1档 (%d)", temp_mid);
            change_multivalve_gear(0x01, INPUT_RIGHT); //火力1档加热
            fire_1_switch_flag = true;
            fire_4_switch_flag = false;
            fire_7_switch_flag = false;
        }

        if (temp_arrivate && (temp_mid < 140*10))
        {
            printf_cur_temp_array(aux_handle);
            //LOG_BAI("温度骤降,热锅结束 (耗时%lu秒 温度=%d)", ((aos_now_ms() - fsm->state_time)/1000), temp_mid);
            LOG_BAI("煎模式: 温度骤降 ===> 热锅结束 (耗时%d秒 温度=%d)", (int)((aos_now_ms() - fsm->state_time)/1000), temp_mid);
            switch_fsm_state(fsm, jian_heat_oil);
        }
    }
    else if (temp_mid < 210*10) //temp_cur
    {
        temp_arrivate = true;
        if (!fire_4_switch_flag)
        {
            LOG_BAI("煎模式: 切换火力到4档 (%d)", temp_mid);
            change_multivalve_gear(0x04, INPUT_RIGHT); //火力调为4档
            fire_4_switch_flag = true;
            fire_1_switch_flag = false;
            fire_7_switch_flag = false;
        }

        if (!is_remind)
        {
            char* tips = "请下油";
            udp_voice_write_sync(tips, strlen(tips), 50);
            is_remind = true;
        }
    }
    else
    {
        temp_arrivate = true;
        if (!fire_7_switch_flag)
        {
            LOG_BAI("煎模式: 切换火力到7档 (%d)", temp_mid);
            change_multivalve_gear(0x07, INPUT_RIGHT); //火力调为7档
            fire_7_switch_flag = true;
            fire_1_switch_flag = false;
            fire_4_switch_flag = false;
        }
    }

    if ((aos_now_ms() - temp_value_last.time) >= (10*1000))
    {
        temp_value_t temp_value_now = {
            .time = aos_now_ms(),
            .temp = temp_mid //temp_cur
        };

        double slope = calculateSlope(temp_value_last, temp_value_now);
        LOG_BAI("煎模式: *****热锅里判断状态(10秒一次)***** (%d %d) ===> (%d %d) 斜率=%f · 最新温度=%d · 中位数温度=%d", (int)(temp_value_last.time/1000), temp_value_last.temp, (int)(temp_value_now.time/1000), temp_value_now.temp, slope, temp_cur, temp_mid);

        // if ((slope > 30.0) && (temp_mid <= 150*10))
        // {
        //     switch_fsm_state(fsm, jian_heat_oil);
        //     return;
        // }
        // else if ((slope < 5.0) && (temp_mid <= 170*10))
        // {
        //     switch_fsm_state(fsm, jian_heat_food);
        //     return;
        // }

        temp_value_last = temp_value_now;
    }

    if ((aos_now_ms() - temp_value_last_5.time) >= (5*1000))
    {
        temp_value_t temp_value_now = {
            .time = aos_now_ms(),
            .temp = temp_mid //temp_cur
        };

        double slope = calculateSlope(temp_value_last_5, temp_value_now);
        LOG_BAI("煎模式: *****热锅里判断状态(5秒一次)***** (%d %d) ===> (%d %d) 斜率=%f · 最新温度=%d · 中位数温度=%d", (int)(temp_value_last_5.time/1000), temp_value_last_5.temp, (int)(temp_value_now.time/1000), temp_value_now.temp, slope, temp_cur, temp_mid);

        // if ((slope > 30.0) && (temp_mid <= 150*10))
        // {
        //     switch_fsm_state(fsm, jian_heat_oil);
        //     return;
        // }
        // else if ((slope < 5.0) && (temp_mid <= 170*10))
        // {
        //     switch_fsm_state(fsm, jian_heat_food);
        //     return;
        // }

        temp_value_last_5 = temp_value_now;
    }

    // if (is_fsm_timeout(fsm, 60*1000) && (temp_mid <= 1500))
    // {
    //     LOG_BAI("煎模式: *****热锅状态时间太长*****(热锅时间=%d秒 中位数温度=%d度)", (aos_now_ms() - fsm->state_time) / 1000, temp_mid/10);
    //     switch_fsm_state(fsm, jian_heat_oil);
    // }
}

void jian_heat_pan(func_ptr_fsm_t* fsm, aux_handle_t *aux_handle)
{
    char voice_buff[64] = {0x00};
    static temp_value_t temp_value_last = {0x00};
    static temp_value_t temp_value_last_5 = {0x00};
    uint16_t temp_cur = aux_handle->temp_array[ARRAY_DATA_SIZE - 1];            // 最新温度
    uint16_t temp_mid = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);    // 中位数温度
    static bool is_remind = false;
    static bool fire_1_switch_flag = false;
    static bool fire_7_switch_flag = false;
    static bool temp_arrivate = false;
    static uint64_t fire_1_switch_tick = 0;
    static uint64_t fire_7_switch_tick = 0;

    if (fsm->state_time == 0)
    {
        udp_voice_write_sync("开始热锅", strlen("开始热锅"), 50);
        LOG_BAI("煎模式: 进入热锅");
        set_fsm_time(fsm);
        temp_value_last = save_current_temp(temp_mid);  //temp_cur
        temp_value_last_5 = temp_value_last;
        is_remind = false;
        temp_arrivate = false;
        fire_1_switch_flag = false;
        fire_7_switch_flag = false;
        fire_1_switch_tick = 0;
        fire_7_switch_tick = 0;
    }

    //一直处于热锅阶段里面
    if (temp_cur < 190*10)  //大于190度且持续时间超过5秒，才切换档位，而不是那种立即切换，立即切换有可能在一直切换档位(因为当前温度有可能在200这个临界点跳来跳去)
    {
        if (!temp_arrivate)
        {
            if (!fire_1_switch_flag)
            {
                LOG_BAI("煎模式: 切换火力到1档 (%d)", temp_mid);
                change_multivalve_gear(0x01, INPUT_RIGHT); //火力1档加热
                fire_1_switch_flag = true;
                fire_7_switch_flag = false;
            }
        }
        else
        {
            if (temp_mid < 150*10)
            {
                printf_cur_temp_array(aux_handle);
                //LOG_BAI("温度骤降,热锅结束 (耗时%lu秒 温度=%d)", ((aos_now_ms() - fsm->state_time)/1000), temp_mid);
                LOG_BAI("煎模式: 温度骤降 ===> 热锅结束 (耗时%d秒 温度=%d)", (int)((aos_now_ms() - fsm->state_time)/1000), temp_mid);
                switch_fsm_state(fsm, jian_heat_oil);
            }
            else if (temp_mid < 170*10)
            {
                if (!fire_1_switch_flag)
                {
                    LOG_BAI("煎模式: 切换火力到4档 (%d)", temp_mid);
                    change_multivalve_gear(0x04, INPUT_RIGHT); //火力4档加热
                    fire_1_switch_flag = true;
                    fire_7_switch_flag = false;
                }
            }
        }
    }
    else
    {
        if (!fire_7_switch_flag)
        {
            LOG_BAI("煎模式: 切换火力到7档 (%d)", temp_mid);
            change_multivalve_gear(0x07, INPUT_RIGHT); //火力调为4档
            fire_7_switch_flag = true;
            fire_1_switch_flag = false;
        }

        temp_arrivate = true;

        if (!is_remind)
        {
            udp_voice_write_sync("请放油", strlen("请放油"), 50);
            is_remind = true;
        }
    }

    // if ((aos_now_ms() - temp_value_last.time) >= (10*1000))
    // {
    //     temp_value_t temp_value_now = {
    //         .time = aos_now_ms(),
    //         .temp = temp_mid //temp_cur
    //     };

    //     double slope = calculateSlope(temp_value_last, temp_value_now);
    //     LOG_BAI("煎模式: *****热锅里判断状态(10秒一次)***** (%d %d) ===> (%d %d) 斜率=%f · 最新温度=%d · 中位数温度=%d", (int)(temp_value_last.time/1000), temp_value_last.temp, (int)(temp_value_now.time/1000), temp_value_now.temp, slope, temp_cur, temp_mid);

    //     // if ((slope > 30.0) && (temp_mid <= 150*10))
    //     // {
    //     //     switch_fsm_state(fsm, jian_heat_oil);
    //     //     return;
    //     // }
    //     // else if ((slope < 5.0) && (temp_mid <= 170*10))
    //     // {
    //     //     switch_fsm_state(fsm, jian_heat_food);
    //     //     return;
    //     // }

    //     temp_value_last = temp_value_now;
    // }

    // if ((aos_now_ms() - temp_value_last_5.time) >= (5*1000))
    // {
    //     temp_value_t temp_value_now = {
    //         .time = aos_now_ms(),
    //         .temp = temp_mid //temp_cur
    //     };

    //     double slope = calculateSlope(temp_value_last_5, temp_value_now);
    //     LOG_BAI("煎模式: *****热锅里判断状态(5秒一次)***** (%d %d) ===> (%d %d) 斜率=%f · 最新温度=%d · 中位数温度=%d", (int)(temp_value_last_5.time/1000), temp_value_last_5.temp, (int)(temp_value_now.time/1000), temp_value_now.temp, slope, temp_cur, temp_mid);

    //     // if ((slope > 30.0) && (temp_mid <= 150*10))
    //     // {
    //     //     switch_fsm_state(fsm, jian_heat_oil);
    //     //     return;
    //     // }
    //     // else if ((slope < 5.0) && (temp_mid <= 170*10))
    //     // {
    //     //     switch_fsm_state(fsm, jian_heat_food);
    //     //     return;
    //     // }

    //     temp_value_last_5 = temp_value_now;
    // }

    // if (is_fsm_timeout(fsm, 60*1000) && (temp_mid <= 1500))
    // {
    //     LOG_BAI("煎模式: *****热锅状态时间太长*****(热锅时间=%d秒 中位数温度=%d度)", (aos_now_ms() - fsm->state_time) / 1000, temp_mid/10);
    //     switch_fsm_state(fsm, jian_heat_oil);
    // }
}

void jian_heat_oil_old(func_ptr_fsm_t* fsm, aux_handle_t *aux_handle)
{
    // static temp_value_t temp_value_last = {0x00};
    // uint16_t temp_cur = aux_handle->temp_array[ARRAY_DATA_SIZE - 1];            // 最新温度
    // uint16_t temp_mid = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);    // 中位数温度

    // static bool is_remind = false;
    // static bool fire_1_switch_flag = false;
    // static bool fire_4_switch_flag = false;
    // static bool fire_7_switch_flag = false;
    // static bool temp_arrivate = false;


    // if (fsm->state_time == 0)
    // {
    //     udp_voice_write_sync("开始热油", strlen("开始热油"), 50);
    //     LOG_BAI("煎模式: 进入热油");
    //     set_fsm_time(fsm);
    //     temp_value_last = save_current_temp(temp_mid);
    //     is_remind = false;
    //     temp_arrivate = false;
    //     fire_1_switch_flag = false;
    //     fire_4_switch_flag = false;
    //     fire_7_switch_flag = false;
    // }

    // //一直处于热锅阶段里面
    // if (temp_mid < 180*10)
    // {
    //     if (!fire_1_switch_flag)
    //     {
    //         LOG_BAI("煎模式: 切换火力到1档 (%d)", temp_mid);
    //         change_multivalve_gear(0x01, INPUT_RIGHT); //火力1档加热
    //         fire_1_switch_flag = true;
    //         fire_4_switch_flag = false;
    //         fire_7_switch_flag = false;
    //     }

    //     if (temp_arrivate && (temp_mid < 100*10))
    //     {
    //         printf_cur_temp_array(aux_handle);
    //         LOG_BAI("煎模式: 温度骤降 ===> 热油结束 (耗时%d秒 温度=%d)", (int)((aos_now_ms() - fsm->state_time)/1000), temp_mid);
    //         //LOG_BAI("温度骤降,热油结束 (耗时%lu秒 温度=%d)", ((aos_now_ms() - fsm->state_time)/1000), temp_mid);
    //         switch_fsm_state(fsm, jian_heat_food);
    //     }
    // }
    // else if (temp_mid < 210*10)
    // {
    //     temp_arrivate = true;
    //     if (!fire_4_switch_flag)
    //     {
    //         LOG_BAI("煎模式: 切换火力到4档 (%d)", temp_mid);
    //         change_multivalve_gear(0x04, INPUT_RIGHT); //火力调为4档
    //         fire_4_switch_flag = true;
    //         fire_1_switch_flag = false;
    //         fire_7_switch_flag = false;
    //     }

    //     if (!is_remind)
    //     {
    //         udp_voice_write_sync("请下食材", strlen("请下食材"), 50);
    //         is_remind = true;
    //     }
    // }
    // else
    // {
    //     temp_arrivate = true;
    //     if (!fire_7_switch_flag)
    //     {
    //         LOG_BAI("煎模式: 切换火力到7档 (%d)", temp_mid);
    //         change_multivalve_gear(0x07, INPUT_RIGHT); //火力调为7档
    //         fire_7_switch_flag = true;
    //         fire_1_switch_flag = false;
    //         fire_4_switch_flag = false;
    //     }
    // }

    // if ((aos_now_ms() - temp_value_last.time) >= (10*1000))
    // {
    //     temp_value_t temp_value_now = {
    //         .time = aos_now_ms(),
    //         .temp = cur_temp
    //     };
    //     uint16_t middle_temp = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);
    //     double slope = calculateSlope(temp_value_last, temp_value_now);
    //     LOG_BAI("煎模式: *****热油里判断状态*****(%d %d) (%d %d) %d %f", temp_value_last.time, temp_value_last.temp, temp_value_now.time, temp_value_now.temp, middle_temp, slope);

    //     if ((slope < 0.2) && (middle_temp <= 1700))
    //     {
    //         switch_fsm_state(fsm, jian_heat_food);
    //     }

    //     temp_value_last = temp_value_now;
    // }


/*
    if (is_fsm_timeout(fsm, 4000))
    {
        switch_fsm_state(fsm, jian_heat_food);
    }  */
}


void jian_heat_oil(func_ptr_fsm_t* fsm, aux_handle_t *aux_handle)
{
    static temp_value_t temp_value_last = {0x00};
    uint16_t temp_cur = aux_handle->temp_array[ARRAY_DATA_SIZE - 1];            // 最新温度
    uint16_t temp_mid = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);    // 中位数温度

    static bool is_remind = false;
    static bool fire_1_switch_flag = false;
    static bool fire_4_switch_flag = false;
    static bool fire_7_switch_flag = false;
    static bool temp_arrivate = false;


    if (fsm->state_time == 0)
    {
        udp_voice_write_sync("开始热油", strlen("开始热油"), 50);
        LOG_BAI("煎模式: 进入热油");
        set_fsm_time(fsm);
        temp_value_last = save_current_temp(temp_mid);
        is_remind = false;
        temp_arrivate = false;
        fire_1_switch_flag = false;
        fire_4_switch_flag = false;
        fire_7_switch_flag = false;
    }

    //一直处于热锅阶段里面
    if (temp_mid < 200*10)
    {
        if (!temp_arrivate)
        {
            if (!fire_1_switch_flag)
            {
                LOG_BAI("煎模式: 切换火力到1档 (%d)", temp_mid);
                change_multivalve_gear(0x01, INPUT_RIGHT); //火力1档加热
                fire_1_switch_flag = true;
                fire_7_switch_flag = false;
            }
        }
        else
        {
            if (temp_mid < 150*10)
            {
                printf_cur_temp_array(aux_handle);
                LOG_BAI("煎模式: 温度骤降 ===> 热油结束 (耗时%d秒 温度=%d)", (int)((aos_now_ms() - fsm->state_time)/1000), temp_mid);
                //LOG_BAI("温度骤降,热油结束 (耗时%lu秒 温度=%d)", ((aos_now_ms() - fsm->state_time)/1000), temp_mid);
                switch_fsm_state(fsm, jian_heat_food);
            }
            else if (temp_mid < 180*10)
            {
                if (!fire_1_switch_flag)
                {
                    LOG_BAI("煎模式: 切换火力到1档 (%d)", temp_mid);
                    change_multivalve_gear(0x01, INPUT_RIGHT); //火力1档加热
                    fire_1_switch_flag = true;
                    fire_7_switch_flag = false;
                }
            }

        }
    }
    else
    {
        if (!fire_7_switch_flag)
        {
            LOG_BAI("煎模式: 切换火力到7档 (%d)", temp_mid);
            change_multivalve_gear(0x07, INPUT_RIGHT); //火力调为7档
            fire_7_switch_flag = true;
            fire_1_switch_flag = false;
        }

        temp_arrivate = true;

        if (!is_remind)
        {
            udp_voice_write_sync("请下食材", strlen("请下食材"), 50);
            is_remind = true;
        }
    }

    // if ((aos_now_ms() - temp_value_last.time) >= (10*1000))
    // {
    //     temp_value_t temp_value_now = {
    //         .time = aos_now_ms(),
    //         .temp = cur_temp
    //     };
    //     uint16_t middle_temp = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);
    //     double slope = calculateSlope(temp_value_last, temp_value_now);
    //     LOG_BAI("煎模式: *****热油里判断状态*****(%d %d) (%d %d) %d %f", temp_value_last.time, temp_value_last.temp, temp_value_now.time, temp_value_now.temp, middle_temp, slope);

    //     if ((slope < 0.2) && (middle_temp <= 1700))
    //     {
    //         switch_fsm_state(fsm, jian_heat_food);
    //     }

    //     temp_value_last = temp_value_now;
    // }


/*
    if (is_fsm_timeout(fsm, 4000))
    {
        switch_fsm_state(fsm, jian_heat_food);
    }  */
}

void jian_heat_food(func_ptr_fsm_t* fsm, aux_handle_t *aux_handle)
{
    static temp_value_t temp_value_last = {0x00};
    uint16_t temp_cur = aux_handle->temp_array[ARRAY_DATA_SIZE - 1];            // 最新温度
    uint16_t temp_mid = findMedian(aux_handle->temp_array, ARRAY_DATA_SIZE);    // 中位数温度
    static bool fire_3_switch_flag = false;
    static bool fire_6_switch_flag = false;
    static uint64_t fire_3_switch_tick = 0;
    static uint64_t fire_6_switch_tick = 0;

    if (fsm->state_time == 0)
    {
        udp_voice_write_sync("开始煎食材", strlen("开始煎食材"), 50);
        LOG_BAI("煎模式: 进入煎食材");
        set_fsm_time(fsm);
        temp_value_last = save_current_temp(temp_mid);

        fire_3_switch_flag = false;
        fire_6_switch_flag = false;
        fire_3_switch_tick = 0;
        fire_6_switch_tick = 0;
    }

    if (temp_mid < (100*10))
    {
        if ((!fire_3_switch_flag)  && ((aos_now_ms() - fire_6_switch_tick) > 10*1000))
        {
            LOG_BAI("煎模式: 切换火力到3档 (%d)", temp_mid);
            change_multivalve_gear(0x03, INPUT_RIGHT);
            fire_3_switch_flag = true;
            fire_6_switch_flag = false;
            fire_3_switch_tick = aos_now_ms();
            fire_6_switch_tick = 0;
        }
    }
    else
    {
        if ((!fire_6_switch_flag) && ((aos_now_ms() - fire_3_switch_tick) > 10*1000))
        {
            LOG_BAI("煎模式: 切换火力到6档 (%d)", temp_mid);
            change_multivalve_gear(0x06, INPUT_RIGHT);
            fire_6_switch_flag = true;
            fire_3_switch_flag = false;
            fire_6_switch_tick = aos_now_ms();
            fire_3_switch_tick = 0;
        }
    }


/*
    if (is_fsm_timeout(fsm, 6000))
    {
        switch_fsm_state(fsm, mode_jian_func_heat_pan);
    }  */
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
        switch_fsm_state(&aux_handle->fsm_jian, jian_heat_pan);
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
    memset(&aux_handle->fsm_chao, 0x00, sizeof(aux_handle->fsm_chao));
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
    //LOG_BAI("present target temp:%d,environment temp:%d ",temp,environment_temp);

    if (aux_handle->aux_switch == 0 || aux_handle->ignition_switch == 0)
    {
        if ((time_mem == 0) || (aos_now_ms() - time_mem) >= (1500))
        {
            time_mem = aos_now_ms();
            LOG_BAI("辅助烹饪未启动: 目标温度=%d, 环境温度=%d", temp/10, environment_temp/10);
        }

        cook_aux_init(INPUT_RIGHT);
        aux_cook_reset(aux_handle);
        return;
    }

    if(aux_handle->pan_fire_status == 1)
    {
        LOG_RED("发生移锅小火，退出辅助烹饪");
        udp_voice_write_sync("发生移锅小火，退出辅助烹饪", strlen("发生移锅小火，退出辅助烹饪"), 50);
        cook_aux_init(INPUT_RIGHT);
        aux_exit_cb(AUX_ERROR_EXIT);
        return;
    }

    aux_handle->aux_total_tick++;
    LOG_BAI("最新采集温度(%d) = %d", aux_handle->aux_total_tick, temp);
    aux_temp_save(aux_handle, temp);

    // if(aux_handle->aux_total_tick >= MAX_FIRE_TIME)
    // {
    //     LOG_BAI("兜底关火时间%d到，关闭火焰 ",MAX_FIRE_TIME/4);
    //     aux_close_fire_cb(INPUT_RIGHT);         //右灶关火
    //     aux_handle->aux_switch = 0;             //退出辅助烹饪模式
    // }

    if(aux_handle->temp_size < 10)
    {
        LOG_BAI("temp data is not enough,func return");
        return;
    }
    else
    {
        printf_cur_temp_array(aux_handle);
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
            LOG_BAI("Error cook Mode ");
            return;
        }
    }
}


