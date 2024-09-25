#include <stdio.h>
#include <stdlib.h>
#include "../ca_apps.h"

#define log_module_name "pot_cover"
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)
#define log_info(...)  LOGI(log_module_name, ##__VA_ARGS__)
#define log_warn(...)  LOGW(log_module_name, ##__VA_ARGS__)
#define log_error(...) LOGE(log_module_name, ##__VA_ARGS__)
#define log_fatal(...) LOGF(log_module_name, ##__VA_ARGS__)

/*　全局变量　*/
enum POT_COVER_TYPE g_pot_cover_type = COVER_UNKONW;
enum POT_COVER_TYPE g_last_judge_cover_type = COVER_UNKONW;  // 记录上次判断出来的盖子类型
enum POT_COVER_TYPE g_cover_type_when_water_boil = COVER_UNKONW;  // 记录水开时盖子类型
bool g_pot_cover_app_is_open = false;

long pot_cover_scenes = 1;  // 一般不会超过６４种场景吧

/*　静态变量　*/
static bool have_add_signals = false;
static bool have_add_signals_worker = false;
static bool have_connect = false;
static bool is_update_boil_temp_by_open_cover = false;

// POT_COVER_SCENE_7
static bool pot_cover_scene7_exist_200ticks_node = false;

// POT_COVER_SCENE_9
static bool pot_cover_scene9_exist_jump_rise_at_start = false;  // 是否满足场景9条件1
static char pot_cover_scene9_jump_rise_abs_temp_num = 0;  // 满足场景9条件2中跳升温差个数
static char pot_cover_scene9_jump_down_abs_temp_num = 0;  // 满足场景9条件2中跳降温差个数

/*　重置相关信息　*/
void reset_pot_cover_value()
{
    log_debug("%s", __func__);
    g_pot_cover_type = COVER_UNKONW;
    g_last_judge_cover_type = COVER_UNKONW;
    g_cover_type_when_water_boil = COVER_UNKONW;
    g_pot_cover_app_is_open = false;
    have_add_signals = false;
    have_add_signals_worker = false;
    have_connect = false;
    pot_cover_scenes = 1;
    is_update_boil_temp_by_open_cover = false;
    pot_cover_scene7_exist_200ticks_node = false;
    pot_cover_scene9_exist_jump_rise_at_start = false;

    reset_pot_cover_signals();
}


static void future_update_water_boil_temp_by_open_cover_cb(future_cb_param_t *param)
{
    log_debug("%s", __func__);
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

static void update_cover_type(enum POT_COVER_TYPE judge_cover_type, char *pts)
{
    log_debug("%s", __func__);
    if (judge_cover_type == g_pot_cover_type)
        return;
    log_debug("触发盖子判断场景：%s, 判断为: %d", pts, judge_cover_type);
    g_last_judge_cover_type = g_pot_cover_type;
    g_pot_cover_type = judge_cover_type;

    // 水已经开了，之前盖了盖子，切换成无盖时，更新水开时的温度
    if (!is_update_boil_temp_by_open_cover && g_water_is_boil && g_last_judge_cover_type != COVER_UNKONW && g_last_judge_cover_type != COVER_NONE && g_pot_cover_type == COVER_NONE)
    {
        // g_update_water_boil_temp = g_last_tick_temp;
        // g_update_water_boil_tick = g_ca_tick;
        CA_FUTURE(3 * 4, future_update_water_boil_temp_by_open_cover_cb, NULL);
        is_update_boil_temp_by_open_cover = true;
    }
}

static void func_pot_cover()
{
    log_debug("%s", __func__);

    //  矫正盖子 玻璃盖和玻璃锅矫正为无盖　　　如果当前是玻璃盖，但是90度还没水开，就更新为无盖
    if (g_last_tick_temp > 900 && g_pot_cover_type != COVER_NONE && !g_water_is_boil)
    {
        update_cover_type(COVER_NONE, VAR_NAME(POT_COVER_SCENE_5));
    }
    // 金属盖矫正为玻璃盖
    if (g_last_tick_temp > 570 && g_pot_cover_type == COVER_METAL && !g_water_is_boil)
    {
        update_cover_type(COVER_GLASS, VAR_NAME(POT_COVER_SCENE_6));
    }

    // WATER_BOIL_SCENE_8
    static temp_tendency_info_t *p = NULL;
    static unsigned int total_ticks = 0;
    if (p != g_buildin_temp_gental_tendency_tail && g_buildin_temp_gental_tendency_tail != NULL)
    {
        p = g_buildin_temp_gental_tendency_tail;
        total_ticks += (p->tdc_end_tick - p->tdc_start_tick);
        if (total_ticks > 2 * 60 * 4)
        {
            update_cover_type(COVER_GLASS, VAR_NAME(POT_COVER_SCENE_8));
            p = NULL;
            total_ticks = 0;
        }
    }
}

// 出现平稳时各个场景的判断
static void judge_by_temp_gental()  
{
    log_debug("%s", __func__);
    temp_tendency_info_t *p = g_buildin_temp_gental_tendency_tail;

    //　POT_COVER_SCENE_1  !(pot_cover_scene_type & 1 << SCENE_1)：没出现过SCENE_1才判断　　需要优化的话，duration参数可以由３０改为其他的，越大越严格   删了条件:g_last_tick_temp > 0 && g_last_tick_temp < 500    api_exist_gental_node_duration_and_range(30*4, 0, 0, 500, 0, 500))
    if (!(pot_cover_scenes & 1<<POT_COVER_SCENE_1) && api_exist_gental_node_duration_and_range(30*4, 0, 0, 0, 0, 0))
    {
        pot_cover_scenes += 1<<POT_COVER_SCENE_1;
        update_cover_type(COVER_GLASS, VAR_NAME(POT_COVER_SCENE_1));
    }

    // POT_COVER_SCENE_1_1
    p = g_buildin_temp_gental_tendency_tail;
    unsigned short pot_cover_scene1_1_gental_seconds_gt_20 = 0;
    while (p != NULL)
    {
        if (p->tdc_end_tick - p->tdc_start_tick > 20 * 4 && p->tdc_start_avg_temp > 0 && p->tdc_start_avg_temp < 500 && g_last_tick_temp < 800)
            pot_cover_scene1_1_gental_seconds_gt_20++;
        if (pot_cover_scene1_1_gental_seconds_gt_20 >= 2)
        {
            update_cover_type(COVER_GLASS, VAR_NAME(POT_COVER_SCENE_1_1));
            break;
        }
        p = p->prev;
    }

    // POT_COVER_SCENE_7   需要在  POT_COVER_SCENE_1_1 后面执行,防止被POT_COVER_SCENE_1_1覆盖掉
    p = g_buildin_temp_gental_tendency_head;
    if (!pot_cover_scene7_exist_200ticks_node && get_latest_avg_temp_of_10() <= 800)
    {
        int gt120_count = 0;
        int gt40_and_lt48 = 0;
        while (p != NULL)
        {
            unsigned int duration = p->tdc_end_tick - p->tdc_start_tick;
            if (duration > 50 * 4)
            {
                pot_cover_scene7_exist_200ticks_node = true;
                break;
            }
            if (duration > 30 * 4)
            {
                gt120_count++;
                if (gt120_count > 1)
                    break;
            }
            if (duration > 10 * 4 && duration < 12 * 4)
                gt40_and_lt48++;

            p = p->next;
        }
        if (!pot_cover_scene7_exist_200ticks_node && gt120_count <= 1 && gt40_and_lt48 >= 3)
        {
            pot_cover_scenes += 1<<POT_COVER_SCENE_7;
            update_cover_type(COVER_NONE, VAR_NAME(POT_COVER_SCENE_7));
        }
        
    }

    // POT_COVER_SCENE_1_2
    // p = g_buildin_temp_gental_tendency_tail;
    // unsigned short pot_cover_scene1_2_gental_seconds_gt_25 = 0;
    // while (p != NULL)
    // {
    //     if (p->tdc_end_tick - p->tdc_start_tick > 25 * 4 && p->tdc_start_avg_temp > 0 && p->tdc_start_avg_temp < 500 && g_last_tick_temp < 570)
    //         pot_cover_scene1_2_gental_seconds_gt_25++;
    //     if (pot_cover_scene1_2_gental_seconds_gt_25 >= 3)
    //     {
    //         update_cover_type(COVER_METAL, VAR_NAME(POT_COVER_SCENE_1_2));
    //         break;
    //     }
    //     p = p->prev;
    // }    

    // POT_COVER_SCENE_1_2_1
    // p = g_buildin_temp_gental_tendency_head;
    // bool is_gental_lt_40 = false;
    // while (p != NULL)
    // {
    //     if (!is_gental_lt_40 && p->tdc_end_tick - p->tdc_start_tick > 25 * 4 && p->tdc_start_avg_temp < 400 && g_last_tick_temp < 570)
    //         is_gental_lt_40 = true;
    //     if (is_gental_lt_40 && (p->tdc_end_tick - p->tdc_start_tick > 25 * 4 && p->tdc_start_avg_temp >= 400))
    //     {
    //         update_cover_type(COVER_METAL, VAR_NAME(POT_COVER_SCENE_1_2_1));
    //         break;
    //     }
    //     p = p->next;
    // }
} // 如果兜不住的话，再用rise_ff判断盖盖子情况

// 出现上升时各个场景的判断
static void judge_by_temp_rise()  
{
    log_debug("%s", __func__);
    // POT_COVER_SCENE_2
    if (!(pot_cover_scenes & 1<<POT_COVER_SCENE_1) && !(pot_cover_scenes & 1<<POT_COVER_SCENE_2) && g_last_tick_temp > 0 && g_last_tick_temp < 500 && api_exist_rise_node_duration_and_range(30*4, 0, 0, 500, 0, 500))
    {
        pot_cover_scenes += 1<<POT_COVER_SCENE_2;
        update_cover_type(COVER_NONE, VAR_NAME(POT_COVER_SCENE_2));
    }
}

static void judge_by_temp_jump_rise()
{
    log_debug("%s", __func__);
    // POT_COVER_SCENE_9  条件1判断
    if (g_ca_tick <= 20 * 4)
    {
        pot_cover_scene9_exist_jump_rise_at_start = true;
    }

    // POT_COVER_SCENE_9  条件2的跳升判断
    temp_tendency_info_t *p = g_buildin_temp_jump_rise_tendency_tail;
    if (pot_cover_scene9_exist_jump_rise_at_start && g_temp_jump_rise_tendency_node_num > 3)
    {
        while (p != NULL)
        {
            if (abs(p->tdc_end_avg_temp - p->tdc_start_avg_temp) >= 60)
                pot_cover_scene9_jump_rise_abs_temp_num++;
            p = p->prev;
        }
        
    }
}

static void judge_by_temp_jump_down()
{
    log_debug("%s", __func__);
    // POT_COVER_SCENE_9  条件2的跳降判断
    temp_tendency_info_t *p = g_buildin_temp_jump_down_tendency_tail;
    if (pot_cover_scene9_exist_jump_rise_at_start && g_temp_jump_down_tendency_node_num > 3)
    {
        while (p != NULL)
        {
            if (abs(p->tdc_end_avg_temp - p->tdc_start_avg_temp) >= 60)
                pot_cover_scene9_jump_down_abs_temp_num++;
            p = p->prev;
        }
    }
    if (pot_cover_scene9_exist_jump_rise_at_start && pot_cover_scene9_jump_rise_abs_temp_num > 3 && pot_cover_scene9_jump_down_abs_temp_num > 3)
        update_cover_type(COVER_GLASS_POT, VAR_NAME(POT_COVER_SCENE_9));
}

// 通过盖盖子动作，判断是last_judge_cover_type
static void judge_by_close_cover()
{
    log_debug("%s", __func__);
    // 正常进行的话，进来这里，此时盖子类型是COVER_NONE
    update_cover_type(g_last_judge_cover_type, VAR_NAME(POT_COVER_SCENE_4));  // 这里默认开盖，再盖盖时原来的盖子
}

// 通过开盖判断没盖子
static void judge_by_open_cover()
{
    log_debug("%s", __func__);
    update_cover_type(COVER_NONE, VAR_NAME(POT_COVER_SCENE_3));
}


void app_pot_cover()
{
    log_debug("%s 当前盖子类型：%d, 上种类型%d", __func__, g_pot_cover_type, g_last_judge_cover_type);
    // 盖盖子可以作为基础业务，默认开启
    g_pot_cover_app_is_open = true;
    if (!g_pot_cover_app_is_open)  // 虽然但是，还是判断一下
        return;

    if (!have_add_signals)
    {
        pot_cover_add_signals();
        have_add_signals = true;
    }
    if (!have_add_signals_worker)
    {
        ADD_USER_SIGNAL_WORKER(pot_cover_signals_worker);
        have_add_signals_worker = true;
    }

    // 利用其他模块信号
    if (!have_connect)
    {
        CA_SIG_CONNECT(g_buildin_signal_temp_gental, judge_by_temp_gental);
        CA_SIG_CONNECT(g_buildin_signal_temp_rise, judge_by_temp_rise);
        CA_SIG_CONNECT(g_buildin_signal_temp_jump_down, judge_by_temp_jump_down);
        CA_SIG_CONNECT(g_buildin_signal_temp_jump_rise, judge_by_temp_jump_rise);
        CA_SIG_CONNECT(g_sig_pot_cover_open_cover, judge_by_open_cover);
        CA_SIG_CONNECT(g_sig_pot_cover_close_cover, judge_by_close_cover);
        have_connect = true;
    }
    func_pot_cover();
}