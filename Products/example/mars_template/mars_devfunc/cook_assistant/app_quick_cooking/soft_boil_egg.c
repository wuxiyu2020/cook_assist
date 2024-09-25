/*  溏心蛋
 *  与焯水逻辑基本一致
 *　简单的大小火交替
*/ 

#include "../ca_apps.h"

#define log_module_name "soft_boil_egg"
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)
#define log_info(...)  LOGI(log_module_name, ##__VA_ARGS__)
#define log_warn(...)  LOGW(log_module_name, ##__VA_ARGS__)
#define log_error(...) LOGE(log_module_name, ##__VA_ARGS__)
#define log_fatal(...) LOGF(log_module_name, ##__VA_ARGS__)

#define BIG_TO_SMALL_AT_LEAST_INTERVAL_TICKS    (10 * 4)  // 大火要调到小火，大火至少需要持续的时间
// #define SMALL_TO_BIG_AT_LEAST_INTERVAL_TICKS    (20 * 4)  // 小火要调到大火，小火至少需要持续的时间
#define BIG_TO_SMALL_AT_MOST_INTERVAL_TICKS    (20 * 4)  // 大火要调到小火，大火至多能持续的时间
// #define SMALL_TO_BIG_AT_MOST_INTERVAL_TICKS    (30 * 4)  // 小火要调到大火，小火至多能持续的时间
#define AFTER_RESTART_COUNT_DOWN_NO_SMALL_FIRE_TICKS   (30 * 4)  // 重置计时后至少多久不能小火

static unsigned short last_jump_down_end_temp = 0;  // 记录上次跳降后的温度
static unsigned int judge_by_temp_jump_down_behind_ticks = 0;  // 记录后续温度的累计时间

static temp_tendency_info_t *last_temp_down_node = NULL;  // 记录上一次温度下降的node
static temp_tendency_info_t *last_temp_gental_node = NULL;  // 记录上一次温度平稳的node

void reset_soft_boil_egg()
{
    last_temp_down_node = NULL;
    last_temp_gental_node = NULL;

    judge_by_temp_jump_down_behind_ticks = 0;
    last_jump_down_end_temp = 0;
}

// 回调函数中统一判断一些条件，检查是否需要继续判断下去
static int access_judge()
{
    if (!have_water_boil || g_quick_cooking_app_is_open == 0) return 1;
    if (g_last_tick_temp >= 1100) return 1;
    if (g_cook_model != NULL && g_cook_model->cook_type != SOFT_BOILED_EGG) return 1;  // 防止中途变了其他模式，还来运行原来模式的逻辑

    return 0;
}

void func_soft_boil_egg()
{
    log_debug("%s", __func__);

    // 这里就不直接调用access_judge
    if (!have_water_boil || g_quick_cooking_app_is_open == 0) return;
    if (g_last_tick_temp >= 1100) return;

    // 这两个if设置大小火持续的最长时间不能超过多少  焯水和煮鸡蛋不用要求小火最长时间,可以一直小火
    // if (have_water_boil && fire_state == FIRE_SMALL && last_turn_small_fire_tick != 0 && g_ca_tick - last_turn_small_fire_tick > SMALL_TO_BIG_AT_MOST_INTERVAL_TICKS)
    // {
    //     ca_set_fire(FIRE_BIG, 1);
    //     last_turn_big_fire_tick = g_ca_tick;
    // }
    //　假如快满足这个条件的时候，加了冷水，就不应该调小火了　　再加个温度简单限制一下，需要平均温度大于90度    需要控制大火最长时间,防止鸡蛋肉过熟
    if (have_water_boil && fire_state == FIRE_BIG && last_turn_big_fire_tick != 0 && g_ca_tick - last_turn_big_fire_tick > BIG_TO_SMALL_AT_MOST_INTERVAL_TICKS)
    {
        if (get_latest_avg_temp_of_10() > 900)
        {
            ca_set_fire(FIRE_SMALL, 1);
            last_turn_small_fire_tick = g_ca_tick;
        }
    }

    log_debug("水开温度：%d  更新温度：%d", g_water_boil_temp, g_update_water_boil_temp);
    // 保底变大火
    // (last_turn_small_fire_tick != 0 && g_ca_tick - last_turn_small_fire_tick > 20 * 4) 防止小火后马上就大火
    log_debug("last_turn_small_fire_tick %d, g_ca_tick: %d, last_turn_small_fire_tick: %d", last_turn_small_fire_tick, g_ca_tick, last_turn_small_fire_tick);
    if (have_water_boil && fire_state != FIRE_BIG && g_ca_tick - last_turn_small_fire_tick > 20 * 4)
    {
        // 这里需要对水开时的温度数值，来定不同的差值，不然温度高时，小火后降得快，容易误触；温度低时，又很难判断出来 g_update_water_boil_temp为0就用g_water_boil_temp判断,不为0就用g_update_water_boil_temp
        int diff;
        if ((g_update_water_boil_temp == 0 && 1000 <= g_water_boil_temp && g_water_boil_temp <= 1100) || (g_update_water_boil_temp != 0 && 1000 <= g_update_water_boil_temp && g_update_water_boil_temp <= 1100))
            diff = 100;
        else if ((g_update_water_boil_temp == 0 && 930 <= g_water_boil_temp && g_water_boil_temp <= 1000) || (g_update_water_boil_temp != 0 && 930 <= g_update_water_boil_temp && g_update_water_boil_temp <= 1000))
            diff = 80;
        else if ((g_update_water_boil_temp == 0 && 880 <= g_water_boil_temp && g_water_boil_temp < 930) || (g_update_water_boil_temp != 0 && 880 <= g_update_water_boil_temp && g_update_water_boil_temp < 930))
            diff = 50;
        else
            diff = 30;
        // g_water_boil_temp  g_update_water_boil_temp 任意一个比平均值大diff即可
        if (g_buildin_signal_average_temp_of_4->valid_size > 0 && ((g_water_boil_temp - g_buildin_avg_arr4[g_buildin_signal_average_temp_of_4->valid_size - 1] >= diff) || (g_update_water_boil_temp - g_buildin_avg_arr4[g_buildin_signal_average_temp_of_4->valid_size - 1] >= diff)))
        {
            log_debug("diff: %d, avg_temp: %d", diff, g_buildin_avg_arr4[g_buildin_signal_average_temp_of_4->valid_size - 1]);
            // 分盖盖情况用g_update_water_boil_tick　　和　开盖情况用g_water_boil_tick
            if (((g_cover_type_when_water_boil == COVER_GLASS || g_cover_type_when_water_boil == COVER_METAL) && g_update_water_boil_tick !=0) || 
                g_cover_type_when_water_boil == COVER_NONE || g_cover_type_when_water_boil == COVER_GLASS_POT)
            {
                log_debug("保底调大火");
                ca_set_fire(FIRE_BIG, 1);
                last_turn_big_fire_tick = g_ca_tick;
            }
        }
    }
}

static void judge_by_temp_gental()
{
    log_debug("%s", __func__);
    if(access_judge()) return;

    temp_tendency_info_t *p = g_buildin_temp_gental_tendency_tail;
    // 与煮饺子相比少了条件:temp_have_first_down  !count_down_lock,因为少了!count_down_lock,所以需要加上温度判断条件  88度是因为考虑到焯水浮沫,溏心蛋温度低点也没关系
    if (get_latest_avg_temp_of_10() >= 880 &&
        p != NULL && have_water_boil && fire_state == FIRE_BIG && last_temp_gental_node != p && p->tdc_end_tick - p->tdc_start_tick > 20 * 4
        )
    {
        last_temp_gental_node = p;  // 记住这个gental node,因为节点更新也会触发, 防止一个节点重复使用
        log_debug("温度出现平稳，变小火");
        if (fire_state != FIRE_SMALL && g_ca_tick - last_turn_big_fire_tick > BIG_TO_SMALL_AT_LEAST_INTERVAL_TICKS)
        {
            ca_set_fire(FIRE_SMALL, 1);
            last_turn_small_fire_tick = g_ca_tick;
        }
    }
}

static void judge_by_temp_down()
{
    log_debug("%s", __func__);
    if(access_judge()) return;
    
    temp_tendency_info_t *p = g_buildin_temp_down_tendency_tail;

    if (p != NULL && last_temp_down_node != p && temp_have_first_down)
    {
        last_temp_down_node = p;
        log_debug("温度持续下降时，变大火");  // 并且平均温度降到85度下
        if (fire_state != FIRE_BIG && get_latest_avg_temp_of_10() < 850)
        {
            ca_set_fire(FIRE_BIG, 1);
            last_turn_big_fire_tick = g_ca_tick;
        }
    }
}

static void future_turn_big_fire_by_jump_down(future_cb_param_t *param)
{
    // 距离跳降5秒后,温度在跳降后温度+3度一下,就认为真的跳降了,调大火
    if (get_latest_avg_temp_of_10() < param->temp + 30)
    {
        log_debug("5秒后判断需要调大火");
        ca_set_fire(FIRE_BIG, 1);
    }
}
static void judge_by_temp_jump_down()
{
    if(access_judge()) return;
    log_debug("%s", __func__);

    temp_tendency_info_t *jum_down = g_buildin_temp_jump_down_tendency_tail;

    if (jum_down != NULL)
    {
        log_debug("jum_down->tdc_end_avg_temp %d jum_down->tdc_start_avg_temp %d", jum_down->tdc_end_avg_temp , jum_down->tdc_start_avg_temp);
    }
    
    // 对跳降进行修正 有时候在正常情况下,会出现单个异常温度  比如 920 910 920 910 920 920 920 960 920    突然出现一个96导致跳升/跳降,但这种跳升跳降每意义.
    // 通过跳降后温度,与每4个计算的平均温度对比,如果温度在abs<=2度内,就不认为跳降
    if (jum_down != NULL && abs(jum_down->tdc_end_avg_temp - g_buildin_avg_arr4[g_buildin_signal_average_temp_of_4->valid_size - 1]) <= 20)
        return;
    
    if (have_water_boil && !api_is_cb_in_future(future_turn_big_fire_by_jump_down) && jum_down->tdc_end_avg_temp - jum_down->tdc_start_avg_temp < -80)
    {
        log_debug("跳降了,5秒后判断是否调大火");
        future_cb_param_t *param = malloc(sizeof(future_cb_param_t));
        param->temp = jum_down->tdc_end_avg_temp;
        CA_FUTURE(5 * 4, future_turn_big_fire_by_jump_down, param);
    }
}


void soft_boil_egg_connect()
{
    if (!soft_boil_egg_have_connect)
    {
        CA_SIG_CONNECT(g_buildin_signal_temp_down, judge_by_temp_down);
        CA_SIG_CONNECT(g_buildin_signal_temp_jump_down, judge_by_temp_jump_down);

        CA_SIG_CONNECT(g_buildin_signal_temp_gental, judge_by_temp_gental);
        
        soft_boil_egg_have_connect = true;
    }
    
}