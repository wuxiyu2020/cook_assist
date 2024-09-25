#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ca_apps.h"

#define log_module_name "water_boil"
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)
#define log_info(...)  LOGI(log_module_name, ##__VA_ARGS__)
#define log_warn(...)  LOGW(log_module_name, ##__VA_ARGS__)
#define log_error(...) LOGE(log_module_name, ##__VA_ARGS__)
#define log_fatal(...) LOGF(log_module_name, ##__VA_ARGS__)

/*　全局变量　*/
bool g_water_boil_app_is_open = false;
bool g_water_is_boil = false;
unsigned int g_water_boil_tick = 0;  // 水开时的时间
unsigned short g_water_boil_temp = 0;  // 水开时的温度
unsigned short g_update_water_boil_temp = 0;  // 由于开盖或者跳升导致更新的水开温度
unsigned int g_update_water_boil_tick = 0;  // 由于开盖或者跳升导致更新的水开时间

long water_boil_scenes = 1;

/*　静态变量　*/
static bool have_add_signals = false;
static bool have_add_signals_worker = false;
static bool have_connect = false;

/*　                     各个场景所用变量　                   */
// 公共
static unsigned int last_rise_ff_node_slope = 0;  // 记录越来越快链表最后一个节点的温度上升速率
static unsigned int last_rise_end_temp = 0;  // 记录上升过程中上一次触发上升时的end temp  用来结合各个场景计算短时上升的速率
static unsigned int last_rise_end_tick = 0;  // 记录上升过程中上一次触发上升时的end tick  用来结合各个场景计算短时上升的速率

// WATER_BOIL_SCENE_1
static unsigned int water_boil_scenes1_ticks = 0;  // 记录场景１出现的时间点
static unsigned int water_boil_scenes1_temp_range_min_slope = 0;  // 记录场景1温度范围内最下的上升速率

// WATER_BOIL_SCENE_1_2
static unsigned int water_boil_scenes1_2_trigger_tick = 0;  // 记录场景1.2触发时的tick

// WATER_BOIL_SCENE_2
// static unsigned int water_boil_scenes2_ticks = 0;  // 累计记录场景2温度上升时的速率大于last_rise_ff_node_slope的总时间
static bool water_boil_scene2_exist_ff = false;
static short water_boil_scene2_slope_gt_1500_count = 0;

// WATER_BOIL_SCENE_3
static unsigned int water_boil_scene3_delay_start_tick = 0;  // 记录场景３触发时的tick

// WATER_BOIL_SCENE_3_1
static temp_tendency_info_t *water_boil_scenes3_1_jump_rise_node;  // 记录场景3.1跳升时的node

// WATER_BOIL_SCENE_8
// #define WATER_BOIL_SCENE8_MONITOR_FLOOR_TEMP    380
// #define WATER_BOIL_SCENE8_MONITOR_CEILING_TEMP    570
// #define WATER_BOIL_SCENE8_ARR_SIZE    ((WATER_BOIL_SCENE8_MONITOR_CEILING_TEMP - WATER_BOIL_SCENE8_MONITOR_FLOOR_TEMP) / 10 + 1)    // 计算监控温度个数
// #define WATER_BOIL_SCENE8_RANGE_38_TO_42_JUDGE_TIME    (60 * 4)  // 分阶段判断平稳时间
// #define WATER_BOIL_SCENE8_RANGE_43_TO_46_JUDGE_TIME    (45 * 4)
// #define WATER_BOIL_SCENE8_RANGE_47_TO_57_JUDGE_TIME    (30 * 4)
// static unsigned short water_boil_scene8_arr[WATER_BOIL_SCENE8_ARR_SIZE];  // 金属盖时，监控记录某个温度上下2度内温度个数
// bool is_water_boil_scene8_arr_init = false;
// unsigned int water_boil_scene8_last_monitor_temp_tick = 0;  // 记录上一个被监控温度出现时的tick，后面被监控的温度tick和记录的tick不能超过5秒  　防止过了很久，场景８逻辑又被执行
// unsigned int  water_boil_scene8_extra_wait_time = 0;  // 金属盖情况下,在原来分段等段平缓时间上,再根据平缓节点特性,额外增加等待时间 存在平缓时间超过300ticks的,加20秒；400ticks加40秒；500ticks加60秒;

// WATER_BOIL_SCENE_10
static bool water_boil_scene10_exist_85_sum30 = false;
static int water_boil_scene10_temp_gte90_count = 0;
static bool water_boil_scene10_is_register_future = false;

// WATER_BOIL_SCENE_11
static unsigned int water_boil_scene11_gte_50_tick = 0;

// WATER_BOIL_SCENE_12
static bool water_boil_scene12_is_get_2_jump_rise = false;
static unsigned int water_boil_scene12_2th_jump_rise_temp = 0;

/*　重置相关信息　*/
void reset_water_boil_value()
{
    g_water_boil_app_is_open = false;
    have_add_signals = false;
    have_add_signals_worker = false;
    g_water_is_boil = false;
    have_connect = false;
    water_boil_scenes = 1;
    last_rise_ff_node_slope = 0;
    last_rise_end_temp = 0;
    last_rise_end_tick = 0;
    water_boil_scenes1_ticks = 0;
    water_boil_scenes1_temp_range_min_slope = 0;
    // water_boil_scenes2_ticks = 0;
    water_boil_scene2_exist_ff = false;
    water_boil_scene2_slope_gt_1500_count = 0;
    water_boil_scenes3_1_jump_rise_node = NULL;
    g_water_boil_tick = 0;
    g_water_boil_temp = 0;
    g_update_water_boil_temp = 0;
    g_update_water_boil_tick = 0;
    water_boil_scenes1_2_trigger_tick = 0;
    water_boil_scene3_delay_start_tick = 0;
    // is_water_boil_scene8_arr_init = false;
    // water_boil_scene8_last_monitor_temp_tick = 0;
    // water_boil_scene8_extra_wait_time = 0;
    water_boil_scene10_exist_85_sum30 = false;
    water_boil_scene10_temp_gte90_count = 0;
    water_boil_scene10_is_register_future = false;
    water_boil_scene11_gte_50_tick = 0;
    water_boil_scene12_is_get_2_jump_rise = false;
    water_boil_scene12_2th_jump_rise_temp = 0;

    reset_water_boil_signals();
}

/* 由于底层平稳的计算方法，虽然某段到某段是平稳触发的，但是实际可能是上升的，在业务判断平稳要持续的时间时，需要修正平稳的时间，修正方法为：
*　查看平稳tail节点的start end温度是否一致，一致的话，就不用修正
*　不一致的话，就要把end_tick - start_tick的时间加到要求平稳的时间里面去
*/
static unsigned int amend_gental_judget_tick(unsigned int wait_ticks)
{
    temp_tendency_info_t *p = g_buildin_temp_gental_tendency_tail;
    if (p->tdc_end_avg_temp != p->tdc_start_avg_temp)
        wait_ticks += (p->tdc_end_tick - p->tdc_start_tick);
    return wait_ticks;
}


static int access_judge()
{
    if (g_water_is_boil) return 1;
    if (g_last_tick_temp >= 1100) return 1;

    return 0;
}

/*********************************************　主要业务逻辑　*********************************************/
static void water_have_boil(char* scene_name)
{
    log_debug("水开场景%s触发了，水开了", scene_name);
    g_water_is_boil = true;
    g_water_boil_tick = g_ca_tick;
    g_water_boil_temp = g_last_tick_temp;
    log_debug("水开温度：%d", g_water_boil_temp);
    g_cover_type_when_water_boil = g_pot_cover_type;
}

/*********************************************　FUTURE　*********************************************/
// WATER_BOIL_SCENE_10 中的3
static void future_water_boil_scene10()
{
    // 如果10秒后10个平均温度大于等于88度,就认为开了
    if (get_latest_avg_temp_of_10() >= 880 && g_buildin_avg_arr10[g_buildin_signal_average_temp_of_10->valid_size - 1] <= 1100)
    {
        water_have_boil(VAR_NAME(WATER_BOIL_SCENE_10));
        water_boil_scenes += 1<<WATER_BOIL_SCENE_10;
    }
}

static void func_water_boil()  // 非信号回调函数，app每次运行都会执行
{
    log_debug("%s", __func__);
    if (access_judge()) return;

    int i;
    temp_tendency_info_t *p;
    enum WATER_BOIL_SCENE si;
    char scenes_str[(SCENE_END - SCENE_START) * 2 + 10] = {0};
    for (si = SCENE_START; si < SCENE_END; si++)
    {
        if (water_boil_scenes & 1<<si)
        {
            char s[10];
            sprintf(s, "%d ", si);
            strcat(scenes_str, s);
        }
    }
    log_debug("当前触发的场景有：%s", scenes_str);

    //　WATER_BOIL_SCENE_2 判断第3点  g_ca_tick % 10 == 0是因为上升是按10个平均值计算的,速率那个时候才计算
    if (water_boil_scene2_exist_ff && g_ca_tick % 10 == 0 && g_pot_cover_type == COVER_GLASS && get_latest_avg_temp_of_10() >= 700 && g_buildin_avg_arr10[g_buildin_signal_average_temp_of_10->valid_size - 1] <= 1100)
    {
        if (g_buildin_snatch_rise_slope >= 1500)
            water_boil_scene2_slope_gt_1500_count++;
        if (water_boil_scene2_slope_gt_1500_count >= 3)
        {
            water_have_boil(VAR_NAME(WATER_BOIL_SCENE_2));
            return;
        }
    }

    // WATER_BOIL_SCENE_3_1_2
    if ((water_boil_scenes & 1<<WATER_BOIL_SCENE_3_1) && !(water_boil_scenes & 1<<WATER_BOIL_SCENE_3_1_2) && g_buildin_signal_average_temp_of_10->valid_size > 0 && g_buildin_avg_arr10[g_buildin_signal_average_temp_of_10->valid_size-1] > 900 && g_buildin_avg_arr10[g_buildin_signal_average_temp_of_10->valid_size-1] < 1100)
    {
        water_have_boil(VAR_NAME(WATER_BOIL_SCENE_3_1_2));
        water_boil_scenes += WATER_BOIL_SCENE_3_1_2;
        return;
    }

    // WATER_BOIL_SCENE_1_2 触发了后再延迟5秒触发  因为有时候水就刚刚开，有时候才微微开，延迟５秒每啥大问题  或者温度大于９５度也算开，有时候会慢慢开，开了，不用过５秒就开了，温度还很高
    if ((water_boil_scenes & 1<<WATER_BOIL_SCENE_1_2) && (g_ca_tick - water_boil_scenes1_2_trigger_tick > 5 * 4 || g_last_tick_temp > 950) && g_last_tick_temp <= 1100)
    {
        water_have_boil(VAR_NAME(WATER_BOIL_SCENE_1_2));
        return;
    }

    // WATER_BOIL_SCENE_3
    // if (water_boil_scene3_delay_start_tick != 0 && g_ca_tick - water_boil_scene3_delay_start_tick > 5 * 4)
    // {
    //     water_have_boil(VAR_NAME(WATER_BOIL_SCENE_3));
    //     return;
    // }

    // WATER_BOIL_SCENE_8
    // if (!is_water_boil_scene8_arr_init)  // 初始化数组每个值为0
    // {
    //     for (i=0; i<WATER_BOIL_SCENE8_ARR_SIZE; i++)
    //     {
    //         water_boil_scene8_arr[i] = 0;
    //     }
    //     is_water_boil_scene8_arr_init = true;
    // }
    // if (g_pot_cover_type == COVER_METAL && g_buildin_signal_average_temp_of_4->valid_size > 0 && !(water_boil_scenes & 1<<WATER_BOIL_SCENE_8) && g_buildin_avg_arr4[g_buildin_signal_average_temp_of_4->valid_size - 1] >= WATER_BOIL_SCENE8_MONITOR_FLOOR_TEMP && g_buildin_avg_arr4[g_buildin_signal_average_temp_of_4->valid_size - 1] <= WATER_BOIL_SCENE8_MONITOR_CEILING_TEMP)
    // {
    //     // 判断平稳节点  修改water_boil_scene8_extra_wait_time的值
    //     p = g_buildin_temp_gental_tendency_head;
    //     while (p != NULL)
    //     {
    //         unsigned int duration = p->tdc_end_tick - p->tdc_start_tick;
    //         if (duration >= 1 * 60 * 4 && duration < 2 * 60 * 4)
    //             water_boil_scene8_extra_wait_time = 20 * 4;
    //         else if (duration >= 2 * 60 * 4 && duration < 3 * 60 * 4)
    //             water_boil_scene8_extra_wait_time = 40 * 4;
    //         else if (duration >= 3 * 60 * 4)
    //             water_boil_scene8_extra_wait_time = 60 * 4;
    //         p = p->next;
    //     }

    //     for (i=0; i<WATER_BOIL_SCENE8_ARR_SIZE; i++)
    //     {
    //         if (abs(g_last_tick_temp - (WATER_BOIL_SCENE8_MONITOR_FLOOR_TEMP + i * 20)) <= 10 && (water_boil_scene8_last_monitor_temp_tick == 0 || (water_boil_scene8_last_monitor_temp_tick != 0 && g_ca_tick - water_boil_scene8_last_monitor_temp_tick <= 5 * 4)))
    //         {
    //             water_boil_scene8_arr[i]++;
    //             water_boil_scene8_last_monitor_temp_tick = g_ca_tick;
    //         }
    //         if ((g_last_tick_temp >= WATER_BOIL_SCENE8_MONITOR_FLOOR_TEMP && g_last_tick_temp <= 420 && water_boil_scene8_arr[i] >= WATER_BOIL_SCENE8_RANGE_38_TO_42_JUDGE_TIME + water_boil_scene8_extra_wait_time) || 
    //             (g_last_tick_temp >= 430 && g_last_tick_temp <= 460 && water_boil_scene8_arr[i] >= WATER_BOIL_SCENE8_RANGE_43_TO_46_JUDGE_TIME + water_boil_scene8_extra_wait_time) ||
    //             (g_last_tick_temp >= 470 && g_last_tick_temp <= WATER_BOIL_SCENE8_MONITOR_CEILING_TEMP && water_boil_scene8_arr[i] >= WATER_BOIL_SCENE8_RANGE_47_TO_57_JUDGE_TIME + water_boil_scene8_extra_wait_time))
    //         {
    //             water_have_boil(VAR_NAME(WATER_BOIL_SCENE_8));
    //             water_boil_scenes += 1<<WATER_BOIL_SCENE_8;
    //             return;
    //         }
    //     }
    // }

    // WATER_BOIL_SCENE_8_1
    // p = g_buildin_temp_gental_tendency_tail;
    // unsigned short pot_cover_scene8_1_gental_seconds_gt_25 = 0;
    // temp_tendency_info_t *node_when_is_5 = NULL;
    // if (!(water_boil_scenes & 1<<WATER_BOIL_SCENE_8_1))
    // {
    //      while (p != NULL)
    //     {
    //         if (p->tdc_end_tick - p->tdc_start_tick > 25 * 4 && g_last_tick_temp < 570)
    //         {
    //             pot_cover_scene8_1_gental_seconds_gt_25++;
    //             if (pot_cover_scene8_1_gental_seconds_gt_25 >= 5)
    //                 node_when_is_5 = p;
    //         }
    //         while (node_when_is_5 != NULL && node_when_is_5->next != NULL && node_when_is_5->next->next != NULL)
    //         {
    //             unsigned int next_node_ticks = node_when_is_5->next->tdc_end_tick - node_when_is_5->next->tdc_start_tick;
    //             unsigned int next2_node_ticks = node_when_is_5->next->next->tdc_end_tick - node_when_is_5->next->next->tdc_start_tick;
    //             if (next_node_ticks < 25 * 4 && next2_node_ticks < 25 * 4 && next2_node_ticks < next_node_ticks)
    //             {
    //                 water_have_boil(VAR_NAME(WATER_BOIL_SCENE_8_1));
    //                 water_boil_scenes += 1<<WATER_BOIL_SCENE_8_1;
    //                 return;
    //             }
    //             node_when_is_5 = node_when_is_5->next;
    //         }
    //         if (pot_cover_scene8_1_gental_seconds_gt_25 >= 5)
    //             break;

    //         p = p->prev;
    //     }   
    // }

    // WATER_BOIL_SCENE_12
    if (g_pot_cover_type == COVER_GLASS_POT && g_buildin_signal_average_temp_of_10->valid_size >=3)
    {
        unsigned int temp1 = g_buildin_avg_arr10[g_buildin_signal_average_temp_of_10->valid_size - 3];
        unsigned int temp2 = g_buildin_avg_arr10[g_buildin_signal_average_temp_of_10->valid_size - 2];
        unsigned int temp3 = g_buildin_avg_arr10[g_buildin_signal_average_temp_of_10->valid_size - 1];
        unsigned int temp4 = g_buildin_avg_arr10[g_buildin_signal_average_temp_of_10->valid_size];
        if (abs(temp1 - temp2) < 15 && abs(temp2 - temp3) < 15 && abs(temp3 - temp4) < 15)
             water_have_boil(VAR_NAME(WATER_BOIL_SCENE_12));
    }
}

static void water_boil_judge_by_temp_rise()
{
    log_debug("%s", __func__);
    if (access_judge()) return;

    temp_tendency_info_t *p = g_buildin_temp_rise_tendency_tail;
    // 速率扩大1000倍，减小误差
    unsigned int rise_slope_of_the_last_two_rise = 0;
    if (p != NULL && p->tdc_end_tick - last_rise_end_tick != 0)
        rise_slope_of_the_last_two_rise = 1000 * (p->tdc_end_avg_temp - last_rise_end_temp) / (p->tdc_end_tick - last_rise_end_tick);

    // WATER_BOIL_SCENE_1
    if (g_pot_cover_type == COVER_NONE && g_last_tick_temp >=850 && g_last_tick_temp <=900)
    {
        if (!(water_boil_scenes & 1<<WATER_BOIL_SCENE_1))  // 没出现过场景１才记录　因为要更新water_boil_scenes1_tick，所以不把这个条件放到外层的if里面  作为被依赖的场景应该都要这么写，然后记住该场景最后一次出现的时间
            water_boil_scenes += 1<<WATER_BOIL_SCENE_1;

        // 记录 water_boil_scenes1_ticks
        water_boil_scenes1_ticks = g_ca_tick;

        // 记录 80-85范围内　最新的上升速率water_boil_scenes1_temp_range_min_slope
        if (last_rise_end_temp !=0 && rise_slope_of_the_last_two_rise != 0)
        {
            water_boil_scenes1_temp_range_min_slope = rise_slope_of_the_last_two_rise;  // < water_boil_scenes1_temp_range_min_slope ? rise_slope_of_the_last_two_rise : water_boil_scenes1_temp_range_min_slope;
            log_debug("water_boil_scenes1_temp_range_min_slope %d", rise_slope_of_the_last_two_rise, water_boil_scenes1_temp_range_min_slope);
        }
    }

    // WATER_BOIL_SCENE_1_2
    p = g_buildin_temp_rise_tendency_tail;
    if (p != NULL && g_last_tick_temp > 900 && !(water_boil_scenes & 1<<WATER_BOIL_SCENE_1_2))
    {
        log_debug("slope %d   water_boil_scenes1_temp_range_min_slope %d", rise_slope_of_the_last_two_rise, water_boil_scenes1_temp_range_min_slope);
        if (rise_slope_of_the_last_two_rise > water_boil_scenes1_temp_range_min_slope)
        {
            // water_have_boil(VAR_NAME(WATER_BOIL_SCENE_1_2));
            // return;
            water_boil_scenes1_2_trigger_tick = g_ca_tick;
        }
    }

    // WATER_BOIL_SCENE_9  要排除跳升情况
    temp_tendency_info_t *jump_rise = g_buildin_temp_jump_rise_tendency_tail;
    // if (((g_pot_cover_type == COVER_GLASS && g_last_tick_temp > 700 && g_buildin_snatch_rise_slope > 3000) 
    //     || (g_pot_cover_type == COVER_NONE && g_last_tick_temp > 900 && g_buildin_snatch_rise_slope > 1500)
    //     || (g_pot_cover_type == COVER_GLASS && g_last_tick_temp > 800 && g_buildin_snatch_rise_slope > 2000))
    //     && (jump_rise == NULL || (jump_rise != NULL && g_ca_tick > jump_rise->tdc_end_tick)) && g_last_tick_temp < 1100)
    if (((g_last_tick_temp > 700 && g_last_tick_temp <= 800 && g_buildin_snatch_rise_slope > 3000) 
        || (g_last_tick_temp > 900 && g_last_tick_temp <= 1100 && g_buildin_snatch_rise_slope > 1500)
        || (g_last_tick_temp > 800 && g_last_tick_temp <= 900 &&  g_buildin_snatch_rise_slope > 2000))
        && (jump_rise == NULL || (jump_rise != NULL && g_ca_tick > jump_rise->tdc_end_tick)) && g_last_tick_temp < 1100)
    {
        water_have_boil(VAR_NAME(WATER_BOIL_SCENE_9));
        water_boil_scenes += 1<<WATER_BOIL_SCENE_9;
        return;
    }    

    // WATER_BOIL_SCENE_7　　排除跳升情况
    // if (g_pot_cover_type == COVER_GLASS && g_last_tick_temp > 700 && g_buildin_avg_arr10[g_buildin_signal_average_temp_of_10->valid_size - 1] - g_buildin_avg_arr10[g_buildin_signal_average_temp_of_10->valid_size - 2] >= 15)
    jump_rise = g_buildin_temp_jump_rise_tendency_tail;
    p = g_buildin_temp_rise_ff_tendency_tail;
    unsigned int ff_latest_node_slope = 0;
    if (p != NULL)
    {
        ff_latest_node_slope = 1000 * (p->tdc_end_avg_temp - p->tdc_start_avg_temp) / (p->tdc_end_tick - p->tdc_start_tick);
        log_debug("p->tdc_end_avg_temp %d, p->tdc_start_avg_temp:%d, p->tdc_end_tick %d, p->tdc_start_tick %d, ff_latest_node_slope:%d", p->tdc_end_avg_temp, p->tdc_start_avg_temp, p->tdc_end_tick, p->tdc_start_tick, ff_latest_node_slope);
    }
    // 本来是125，但是温度又增大了10倍，所以是1250
    if ((jump_rise == NULL || (jump_rise != NULL && g_ca_tick > jump_rise->tdc_end_tick)) && ff_latest_node_slope != 0 && g_pot_cover_type == COVER_GLASS && p->tdc_start_avg_temp >= 700 && p->tdc_start_avg_temp <= 1100 && ff_latest_node_slope >= 1250)
    {
        water_have_boil(VAR_NAME(WATER_BOIL_SCENE_7));
        return;
    }

    // WATER_BOIL_SCENE_11
    if (g_pot_cover_type == COVER_GLASS && g_buildin_signal_average_temp_of_10->valid_size >0 && g_buildin_avg_arr10[g_buildin_signal_average_temp_of_10->valid_size - 1] >= 500 && g_buildin_avg_arr10[g_buildin_signal_average_temp_of_10->valid_size - 1] <= 1100)
    {
        if (water_boil_scene11_gte_50_tick == 0 )
        {
            water_boil_scene11_gte_50_tick = g_ca_tick;
        }

        temp_tendency_info_t *gental = g_buildin_temp_gental_tendency_head;
        p = g_buildin_temp_rise_tendency_head;
        while (gental != NULL)
        {
            if (
                gental->tdc_start_tick > water_boil_scene11_gte_50_tick && gental->tdc_end_tick - gental->tdc_start_tick <= 8 * 4 &&
                (g_buildin_snatch_rise_slope >= 3000 || (g_last_tick_temp > 800 && g_buildin_snatch_rise_slope >= 2000) || (g_last_tick_temp > 900 && g_buildin_snatch_rise_slope >= 1500))
            )
            {
                log_debug("gental->tdc_end_tick: %d, gental->tdc_start_tick: %d", gental->tdc_end_tick, gental->tdc_start_tick);
                water_have_boil(VAR_NAME(WATER_BOIL_SCENE_11));
                return;
            }
            gental = gental->next;
        }
    }

    // 注意：这两个变量一定在这个函数最后面
    if (p != NULL)
    {
        last_rise_end_temp = p->tdc_end_avg_temp;
        last_rise_end_tick = p->tdc_end_tick;
    }
}

static void water_boil_judge_by_temp_gental()
{
    log_debug("%s", __func__);
    if (access_judge()) return;

    temp_tendency_info_t *p = g_buildin_temp_gental_tendency_tail;

    //　WATER_BOIL_SCENE_1_1
    if (g_pot_cover_type == COVER_NONE && (water_boil_scenes & 1<<WATER_BOIL_SCENE_1) && !(water_boil_scenes & 1<<WATER_BOIL_SCENE_1_1) && g_last_tick_temp >=850 && g_last_tick_temp <=1150 &&
        api_exist_gental_node_duration_and_range(amend_gental_judget_tick(15*4), 0, 920, 0, 0, 0) && water_boil_scenes1_ticks != 0 && g_ca_tick - water_boil_scenes1_ticks <= 60*4)
    {
        water_have_boil(VAR_NAME(WATER_BOIL_SCENE_1_1));
        return;
    }

    // WATER_BOIL_SCENE_3_2
    temp_tendency_info_t *gental = g_buildin_temp_gental_tendency_tail;
    temp_tendency_info_t *rise = g_buildin_temp_rise_tendency_tail;
    if ((water_boil_scenes & 1<<WATER_BOIL_SCENE_3) && g_last_tick_temp > 850 && rise != NULL && gental != NULL && (gental->tdc_start_tick < rise->tdc_start_tick || gental->tdc_start_tick > rise->tdc_end_tick) && gental->tdc_start_avg_temp < 1100)
    {
        water_have_boil(VAR_NAME(WATER_BOIL_SCENE_3_2));
        return;
    }

    // WATER_BOIL_SCENE_4
    p = g_buildin_temp_gental_tendency_tail;
    if (p != NULL && p->tdc_start_avg_temp >= 920 && p->tdc_start_avg_temp <= 1100 && p->tdc_end_tick - p->tdc_start_tick >= 15 * 4)
    {
        water_have_boil(VAR_NAME(WATER_BOIL_SCENE_4));
        return;
    }

    // WATER_BOIL_SCENE_7_1
    p = g_buildin_temp_gental_tendency_tail;
    if ((g_pot_cover_type == COVER_GLASS || g_pot_cover_type == COVER_METAL) && p != NULL && p->tdc_start_avg_temp > 850 && p->tdc_start_avg_temp < 1100 && p->tdc_end_tick - p->tdc_start_tick > 15 * 4)
    {
        water_have_boil(VAR_NAME(WATER_BOIL_SCENE_7_1));
        return;
    }

    // WATER_BOIL_SCENE_10 中的1 2
    if(!water_boil_scene10_is_register_future)
    {
        p = g_buildin_temp_gental_tendency_tail;
        if (!water_boil_scene10_exist_85_sum30)
        {
            int sum_ticks = 0;
             while (p != NULL)
            {
                sum_ticks +=  p->tdc_end_tick - p->tdc_start_tick;
                if (p->tdc_start_avg_temp >= 850 && sum_ticks > 30 * 4)
                {
                    water_boil_scene10_exist_85_sum30 = true;
                    break;
                }
                p = p->prev;
            }
        }
        if (water_boil_scene10_exist_85_sum30 && g_last_tick_temp >= 900)
            water_boil_scene10_temp_gte90_count++;
        if (water_boil_scene10_temp_gte90_count > 12)
        {
            water_boil_scene10_is_register_future = true;
            CA_FUTURE(10 * 4, future_water_boil_scene10, NULL);
        }
            
    }
}

static void water_boil_judge_by_temp_rise_ff()
{
    log_debug("%s", __func__);
    if (access_judge()) return;

    temp_tendency_info_t *p = g_buildin_temp_rise_ff_tendency_tail;

    // WATER_BOIL_SCENE_2  记录出现过了越来越快
    if (!water_boil_scene2_exist_ff && p != NULL && g_pot_cover_type == COVER_GLASS && g_last_tick_temp >= 500 && g_last_tick_temp <= 850)
    {
        // 速率扩大1000倍，减小误差．注意后续与这个速率之间的运算都要考虑这点
        // last_rise_ff_node_slope = 1000 * (p->tdc_end_avg_temp - p->tdc_start_avg_temp) / (p->tdc_end_tick - p->tdc_start_tick);
        water_boil_scene2_exist_ff = true;
    }

    // WATER_BOIL_SCENE_3_1_1
    p = g_buildin_temp_rise_ff_tendency_tail;
    if (p != NULL && (water_boil_scenes & 1<<WATER_BOIL_SCENE_3_1) && p->tdc_end_avg_temp > 850 && p->tdc_end_avg_temp < 1100)
    {
        water_have_boil(VAR_NAME(WATER_BOIL_SCENE_3_1_1));
        return;
    }

    // WATER_BOIL_SCENE_5
    if (g_last_tick_temp > 900 && g_last_tick_temp < 1100)
    {
        water_have_boil(VAR_NAME(WATER_BOIL_SCENE_5));
        return;
    }
}

// WATER_BOIL_SCENE_3
static void future_water_boil_scene3()
{
    if ((g_last_tick_temp >= 930 || g_buildin_avg_arr4[g_buildin_signal_average_temp_of_4->valid_size - 1] >= 900) && g_buildin_avg_arr4[g_buildin_signal_average_temp_of_4->valid_size - 1] <= 1100)
    {
        water_have_boil(VAR_NAME(WATER_BOIL_SCENE_3));
    }
}
static void water_boil_judge_by_temp_jump_rise()
{
    log_debug("%s", __func__);
    if (access_judge()) return;

    temp_tendency_info_t *p = g_buildin_temp_jump_rise_tendency_tail;

    // WATER_BOIL_SCENE_3
    if (p != NULL && p->tdc_start_avg_temp > 350 && p->tdc_start_avg_temp < 850)
    {
        if (p->tdc_end_tick > 900 && p->tdc_end_tick < 1000)  // 跳升后温度大于９0，小于１００度就直接水开了
        {
            // water_have_boil(VAR_NAME(WATER_BOIL_SCENE_3));
            // return;
            // water_boil_scene3_delay_start_tick = g_ca_tick;
            CA_FUTURE(5 * 4, future_water_boil_scene3, NULL)  // 跳升后5秒再判断要不要水开
        }
        if (p->tdc_end_avg_temp - p->tdc_start_avg_temp >= 60)
        {
            water_boil_scenes += 1<<WATER_BOIL_SCENE_3;
            water_boil_scenes3_1_jump_rise_node = p;
        }
    }

    // WATER_BOIL_SCENE_12
    // p = g_buildin_temp_jump_rise_tendency_tail;
    // if (!water_boil_scene12_is_get_2_jump_rise && g_pot_cover_type == COVER_GLASS_POT && get_latest_avg_temp_of_10() >= 850)
    // {
    //     if (p != NULL && p->prev != NULL)
    //     {
    //         water_boil_scene12_is_get_2_jump_rise = true;
    //         water_boil_scene12_2th_jump_rise_temp = p->tdc_end_avg_temp;  // 获取第二次跳升后的温度
    //         if (get_latest_avg_temp_of_10() > water_boil_scene12_2th_jump_rise_temp)
    //         {
    //             water_have_boil(VAR_NAME(WATER_BOIL_SCENE_12));
    //         }
    //     }
    // }
}

static void water_boil_judge_by_temp_jump_down()
{
    log_debug("%s", __func__);
    if (access_judge()) return;

    temp_tendency_info_t *p = g_buildin_temp_jump_down_tendency_tail;

    // WATER_BOIL_SCENE_3_1
    if (p != NULL && (water_boil_scenes & 1<<WATER_BOIL_SCENE_3) && abs(p->tdc_end_avg_temp - water_boil_scenes3_1_jump_rise_node->tdc_start_avg_temp) <= 30)
    {
        water_boil_scenes += 1<<WATER_BOIL_SCENE_3_1;
    }
}

static void future_update_water_boil_temp_by_jump_rise_cb()
{
    log_debug("%s", __func__);
    // g_update_water_boil_temp = g_buildin_avg_arr10[g_buildin_signal_average_temp_of_10->valid_size - 1];
    // 遍历数组g_buildin_avg_arr1,取最大的作为更新水开温度
    // unsigned short max = 0;
    // int i;
    // for (i=0; i<g_buildin_signal_average_temp_of_1->valid_size; i++)
    // {
    //     if (g_buildin_avg_arr1[i] > max)
    //         max = g_buildin_avg_arr1[i];
    // }
    // if (max > g_water_boil_temp)
    // {
    //     g_update_water_boil_temp = max;
    //     log_debug("更新水开温度：%d", g_update_water_boil_temp);
    //     g_update_water_boil_tick = g_ca_tick;
    // }
    // 取最大的温度不合理，会因为某个异常温度不正常

    // 就直接去最新的平均温度
    log_debug("更新水开温度：%d", g_update_water_boil_temp);
    g_update_water_boil_tick = get_latest_avg_temp_of_4();
}

// 通过监控跳升，来更新水开时的温度，因为盖盖时，打开盖子后，才是水开温度
static void update_water_boil_temp_by_temp_jum_rise()
{
    if (g_last_tick_temp >= 1100) return;

    // 在水开后的２０秒内，如果发生跳升，就把跳升后的温度设置为水开温度　　　开盖更新水开温度在pot_cover.c 中的函数update_cover_type　中更新
    temp_tendency_info_t *p = g_buildin_temp_jump_rise_tendency_tail;
    if (g_water_is_boil && p != NULL && p->tdc_end_tick - g_water_boil_tick < 20 * 4)
    {
        // g_update_water_boil_temp = p->tdc_end_avg_temp;
        // g_update_water_boil_tick = g_ca_tick;
        CA_FUTURE(3 * 4, future_update_water_boil_temp_by_jump_rise_cb, NULL);
    }
}
/*********************************************　主要业务逻辑　*********************************************/


void app_water_boil()
{
    log_debug("%s", __func__);
    g_water_boil_app_is_open = true;
    if (!g_water_boil_app_is_open)
        return;

    if (!have_add_signals)
    {
        water_boil_add_signals();
        have_add_signals = true;
    }
    if (!have_add_signals_worker)
    {
        ADD_USER_SIGNAL_WORKER(water_boil_signals_worker);
        have_add_signals_worker = true;
    }
    if (!have_connect)
    {
        CA_SIG_CONNECT(g_buildin_signal_temp_gental, water_boil_judge_by_temp_gental);
        CA_SIG_CONNECT(g_buildin_signal_temp_rise, water_boil_judge_by_temp_rise);
        CA_SIG_CONNECT(g_buildin_signal_temp_jump_rise, water_boil_judge_by_temp_jump_rise);
        CA_SIG_CONNECT(g_buildin_signal_temp_jump_down, water_boil_judge_by_temp_jump_down);
        CA_SIG_CONNECT(g_buildin_signal_temp_rise_ff, water_boil_judge_by_temp_rise_ff);
        CA_SIG_CONNECT(g_buildin_signal_temp_jump_rise, update_water_boil_temp_by_temp_jum_rise);
        have_connect = true;
    }

    func_water_boil();
}