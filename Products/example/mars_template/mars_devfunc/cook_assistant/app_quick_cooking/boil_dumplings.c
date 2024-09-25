/*  煮饺子  
 *  水开后，小火，放饺子（温度下降／跳降）就大火，暂停计时，等温度开始上升，并且温度大于９０度时，才开始计时
 *　计时期间大小火更替
*/ 

#include "../ca_apps.h"

#define log_module_name "dumpling"
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)
#define log_info(...)  LOGI(log_module_name, ##__VA_ARGS__)
#define log_warn(...)  LOGW(log_module_name, ##__VA_ARGS__)
#define log_error(...) LOGE(log_module_name, ##__VA_ARGS__)
#define log_fatal(...) LOGF(log_module_name, ##__VA_ARGS__)

#define BIG_TO_SMALL_AT_LEAST_INTERVAL_TICKS    (90 * 4)  // 大火要调到小火，大火至少需要持续的时间
#define SMALL_TO_BIG_AT_LEAST_INTERVAL_TICKS    (20 * 4)  // 小火要调到大火，小火至少需要持续的时间
#define BIG_TO_SMALL_AT_MOST_INTERVAL_TICKS    (100 * 4)  // 大火要调到小火，大火至多能持续的时间
#define SMALL_TO_BIG_AT_MOST_INTERVAL_TICKS    (30 * 4)  // 小火要调到大火，小火至多能持续的时间
#define AFTER_RESTART_COUNT_DOWN_NO_SMALL_FIRE_TICKS   (30 * 4)  // 重置计时后至少多久不能小火

static temp_tendency_info_t *judge_by_temp_jump_down_node = NULL;  // 记录judge_by_temp_jump_down的跳降node　　如果后续５秒都在jump down之后温度＋３度之下才判定放东西了
static unsigned int judge_by_temp_jump_down_behind_ticks = 0;  // 记录后续温度的累计时间

static temp_tendency_info_t *last_temp_down_node = NULL;  // 记录上一次温度下降的node
static temp_tendency_info_t *last_temp_gental_node = NULL;  // 记录上一次温度平稳的node
static unsigned int restart_coun_down_tick = 0;  // 记录重置倒计时的时间点
static bool temp_have_down_after_boil = false;

void reset_boil_dumplings()
{
    last_temp_down_node = NULL;
    last_temp_gental_node = NULL;

    judge_by_temp_jump_down_node = NULL;
    judge_by_temp_jump_down_behind_ticks = 0;
    restart_coun_down_tick = 0;
    temp_have_down_after_boil = false;
}

// 回调函数中统一判断一些条件，检查是否需要继续判断下去
static int access_judge()
{
    if (!have_water_boil || g_quick_cooking_app_is_open == 0) return 1;
    if (g_last_tick_temp >= 1100) return 1;
    if (g_cook_model != NULL && g_cook_model->cook_type != BOIL_DUMPLINGS) return 1;

    return 0;
}

static void reset_and_lock_count_down()
{
    log_debug("%s", __func__);
    g_cook_model->total_tick = g_cook_model->seted_time * INPUT_DATA_HZ;
    g_cook_model->remaining_time = g_cook_model->seted_time;
    count_down_lock = true;
    temp_have_first_down = true;
    temp_have_first_down_tick = g_ca_tick;
    log_debug("调大火１");
    // g_ca_tick - g_water_boil_tick >= 2 * 4 用处是有时候水开了变小火会立马发生温度骤降，但这个时候不可能用户放了东西的，所以延时一下
    if (fire_state != FIRE_BIG && g_ca_tick - g_water_boil_tick >= 2 * 4)
    {
        ca_set_fire(FIRE_BIG, 1);
        last_turn_big_fire_tick = g_ca_tick;
    }
}

static void future_restart_count_by_gt88()
{
    if (count_down_lock && get_latest_avg_temp_of_4() >= 880 && g_buildin_avg_arr4[g_buildin_signal_average_temp_of_4->valid_size - 1] <= 1100)
    {
        count_down_reset();
        restart_coun_down_tick = g_ca_tick;
    }
}

void func_boil_dumplings()
{
    log_debug("%s", __func__);

    // 这里就不直接调用access_judge
    if (!have_water_boil || g_quick_cooking_app_is_open == 0) return;
    if (g_last_tick_temp >= 1100) return;

    // 如果水开了，在设置的时间范围内没检测到温度降低，就认为一直没放东西，就结束烹饪，并关火
    if (have_water_boil && !temp_have_first_down && g_ca_tick - g_water_boil_tick > g_ca_tick - g_water_boil_tick > g_cook_model->seted_time)
    {
        g_cook_model->cook_event = COOK_END;
        if (g_quick_cooking_info_cb != NULL) g_quick_cooking_info_cb(g_cook_model);
        quick_cooking_switch(0, 1, 1, 1);
        log_debug("水开了，在设置的时间范围内没检测到温度降低，结束烹饪并关火");
        ca_close_fire(1);
    }

    temp_tendency_info_t *p;
    // 这两个if设置大小火持续的最长时间不能超过多少
    if (have_water_boil && fire_state == FIRE_SMALL && last_turn_small_fire_tick != 0 && g_ca_tick - last_turn_small_fire_tick > SMALL_TO_BIG_AT_MOST_INTERVAL_TICKS)
    {
        ca_set_fire(FIRE_BIG, 1);
        last_turn_big_fire_tick = g_ca_tick;
    }
    //　假如快满足这个条件的时候，加了冷水，就不应该调小火了　　再加个温度简单限制一下，需要平均温度大于90度 
    if (have_water_boil && fire_state == FIRE_BIG && last_turn_big_fire_tick != 0 && g_ca_tick - last_turn_big_fire_tick > BIG_TO_SMALL_AT_MOST_INTERVAL_TICKS)
    {
        if (get_latest_avg_temp_of_10() > 900 && g_ca_tick - restart_coun_down_tick > AFTER_RESTART_COUNT_DOWN_NO_SMALL_FIRE_TICKS)
        {
            ca_set_fire(FIRE_SMALL, 1);
            last_turn_small_fire_tick = g_ca_tick;
        }
    }

    /* 跳降延迟判断 */
    // 通过judge_by_temp_jump_down函数中记录judge_by_temp_jump_down的跳降node　　如果后续５秒都在jump down之后温度＋３度之下才判定放东西了
    if (g_ca_tick - g_water_boil_tick >= 2 * 4 && judge_by_temp_jump_down_node != NULL && have_water_boil && g_last_tick_temp <= judge_by_temp_jump_down_node->tdc_end_avg_temp + 30)
    {
        log_debug("出现了跳降 g_last_tick_temp: %d, judge_by_temp_jump_down_node->tdc_end_avg_temp: %d, g_ca_tick: %d", g_last_tick_temp, judge_by_temp_jump_down_node->tdc_end_avg_temp, g_ca_tick);
        judge_by_temp_jump_down_behind_ticks++;
        if (!temp_have_first_down && judge_by_temp_jump_down_behind_ticks >= 5 * 4)
        {
            log_debug("if 1");
            reset_and_lock_count_down();
            judge_by_temp_jump_down_behind_ticks = 0;
            judge_by_temp_jump_down_node = NULL;
        }
        if (temp_have_first_down && fire_state == FIRE_BIG && judge_by_temp_jump_down_behind_ticks >= 3 * 4)
        {
            log_debug("if 2");
            last_turn_big_fire_tick = g_ca_tick;
            judge_by_temp_jump_down_behind_ticks = 0;
            judge_by_temp_jump_down_node = NULL;
        }
        if (temp_have_first_down && fire_state == FIRE_SMALL && judge_by_temp_jump_down_behind_ticks >= 3 * 4)
        {
            log_debug("if 3");
            ca_set_fire(FIRE_BIG, 1);
            // 这里记录下发生了温度下降，可能加水了，服务水再次开后，需要触发事件TEMP_HAVE_DOWN_THEN_WATER_BOIL  后来又不需要了
            temp_have_down_after_boil = true;
            last_turn_big_fire_tick = g_ca_tick;
            judge_by_temp_jump_down_behind_ticks = 0;
            judge_by_temp_jump_down_node = NULL;
        }   
    }

    // 保底＂水开后放东西大火＂，防止温降没及时判断出来，这里利用当前温度与水开时温度作对比
    // 加10秒限制，是防止水开后或者开盖后，温度异常导致立马就识别放了东西
    log_debug("水开温度：%d  更新温度：%d", g_water_boil_temp, g_update_water_boil_temp);
    if (have_water_boil && !temp_have_first_down && ((g_update_water_boil_tick == 0 && g_ca_tick - g_water_boil_tick >= 4 * 10) || (g_update_water_boil_tick != 0 && g_ca_tick - g_update_water_boil_tick >= 4 * 10)))
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
        log_debug("diff：%d ", diff);
        // g_water_boil_temp  g_update_water_boil_temp 任意一个比平均值大diff即可
        if (g_buildin_signal_average_temp_of_4->valid_size > 0 && ((g_water_boil_temp - g_buildin_avg_arr4[g_buildin_signal_average_temp_of_4->valid_size - 1] >= diff) || (g_update_water_boil_temp - g_buildin_avg_arr4[g_buildin_signal_average_temp_of_4->valid_size - 1] >= diff)))
        {   
            log_debug("diff: %d, avg_temp: %d", diff, g_buildin_avg_arr4[g_buildin_signal_average_temp_of_4->valid_size - 1]);
            // 分盖盖情况用g_update_water_boil_tick　　和　开盖情况用g_water_boil_tick
            if (((g_cover_type_when_water_boil == COVER_GLASS || g_cover_type_when_water_boil == COVER_METAL) && g_update_water_boil_tick !=0) || 
                (g_cover_type_when_water_boil == COVER_NONE))
            {
                log_debug("保底放食材大火");
                reset_and_lock_count_down();
            }
        }
    }

    // 保底重新开始倒计时,防止count_down_restart_by_temp_rise函数一直没触发  如果已经出现了水开后温度降低,距离水开温度超过60秒,分几种情况判断开始计时
    if (count_down_lock && temp_have_first_down && g_ca_tick - g_water_boil_tick > 60 * 4)
    {
        // 存在平稳节点,平稳开始温度大于等于92度
        p = g_buildin_temp_gental_tendency_head;
        if (p != NULL && p->tdc_start_avg_temp >= 920 && p->tdc_start_avg_temp <= 1100)
        {
            count_down_reset();
            restart_coun_down_tick = g_ca_tick;
        }
        // 单个温度出现大于90度时,10秒后如果还没重新开始计时,并且4个平均温度大于等于88度
        else if (g_last_tick_temp > 900)
        {
            CA_FUTURE(10 * 4, future_restart_count_by_gt88, NULL);
        }
    }
}

// 放了饺子后，出现一个新的温度上升node，继续计时
static void count_down_restart_by_temp_rise()
{
    log_debug("%s", __func__);
    if(access_judge()) return;

    if (count_down_lock && temp_have_first_down && get_latest_avg_temp_of_4() >= 960 && g_buildin_avg_arr4[g_buildin_signal_average_temp_of_4->valid_size - 1] <= 1100)
    {
        // 防止“由于放饺子后短时间温度波动异常，而导致提前重新开始计时”
        if (g_ca_tick - temp_have_first_down_tick > 20 * 4)
        {
            count_down_reset();
            restart_coun_down_tick = g_ca_tick;
        }
    }
}

static void judge_by_temp_gental()
{
    log_debug("%s", __func__);
    if(access_judge()) return;

    temp_tendency_info_t *p = g_buildin_temp_gental_tendency_tail;
    // !count_down_lock这个条件原来是g_last_tick_temp > 900，替换的原因是：计时恢复了说明放了饺子后，温度经过了上升，再达到平稳的，比直接用温度限制更好，有时候温度达不到９０度
    if (p != NULL && temp_have_first_down && have_water_boil && fire_state == FIRE_BIG && !count_down_lock && last_temp_gental_node != p && p->tdc_end_tick - p->tdc_start_tick > 20 * 4)
    {
        last_temp_gental_node = p;
        log_debug("水开后有下降过，温度出现平稳，变小火");
        if (fire_state != FIRE_SMALL && g_ca_tick - last_turn_big_fire_tick > BIG_TO_SMALL_AT_LEAST_INTERVAL_TICKS && g_ca_tick - restart_coun_down_tick > AFTER_RESTART_COUNT_DOWN_NO_SMALL_FIRE_TICKS)
        {
            ca_set_fire(FIRE_SMALL, 1);
            if (temp_have_down_after_boil)  // 第一次温度下降后，再次温度出现过下降又水开，就触发TEMP_HAVE_DOWN_THEN_WATER_BOIL
            {
                // 不要这个叫声了
                // g_cook_model->cook_event = TEMP_HAVE_DOWN_THEN_WATER_BOIL;
                // if (g_quick_cooking_info_cb != NULL) g_quick_cooking_info_cb(g_cook_model);  // 传递TEMP_HAVE_DOWN_THEN_WATER_BOIL事件
                // log_debug("传递TEMP_HAVE_DOWN_THEN_WATER_BOIL事件");
                temp_have_down_after_boil = false;
            }
            last_turn_small_fire_tick = g_ca_tick;
        }
    }
}

static void judge_by_temp_down()
{
    if(access_judge()) return;
    log_debug("%s", __func__);

    temp_tendency_info_t *p = g_buildin_temp_down_tendency_tail;

    // 水开后，还没有出现过温度降低，温度持续下降时，就重置并暂停倒计时，变大火
    if (p != NULL && !temp_have_first_down)
    {
        log_debug("水开后，还没有出现过温度降低，温度持续下降时，重置并暂停倒计时，变大火");
        reset_and_lock_count_down();
    }

    // 如果有下降过，单纯变大火
    if (p != NULL && last_temp_down_node != p && temp_have_first_down)
    {
        last_temp_down_node = p;
        log_debug("水开后有下降过，温度持续下降时，单纯变大火");
        if (fire_state != FIRE_BIG && g_ca_tick - last_turn_small_fire_tick > SMALL_TO_BIG_AT_LEAST_INTERVAL_TICKS)
        {
            ca_set_fire(FIRE_BIG, 1);
            // 这里记录下发生了温度下降，可能加水了，服务水再次开后，需要触发事件TEMP_HAVE_DOWN_THEN_WATER_BOIL
            temp_have_down_after_boil = true;
            last_turn_big_fire_tick = g_ca_tick;
        }
    }

    // 保底温度降低调大火
    if (fire_state == FIRE_SMALL && have_water_boil &&
        ((get_latest_avg_temp_of_4() <= 840) ||
         (g_buildin_signal_average_temp_of_10->valid_size > 0 && g_buildin_avg_arr4[g_buildin_signal_average_temp_of_10->valid_size - 1] <= 870))
        )
    {
        log_debug("保底温度降低调大火");
        ca_set_fire(FIRE_BIG, 1);
        temp_have_down_after_boil = true;
        last_turn_big_fire_tick = g_ca_tick;
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

    // 通过在这里记录跳降节点，在app主函数里面去判断未来的情况，再做相应操作　　　对应函数func_boil_dumplings中＂跳降延迟判断＂第１个if
    if (temp_have_first_down && jum_down != NULL && jum_down->tdc_end_avg_temp - jum_down->tdc_start_avg_temp <= -50 && g_last_tick_temp < 900)
    {
        log_debug("记录跳降node");
        judge_by_temp_jump_down_node = jum_down;
    }

    // 当还是大火时，刷新last_turn_big_fire_tick   对应函数func_boil_dumplings中＂跳降延迟判断＂第2个if
    if (fire_state == FIRE_BIG && temp_have_first_down && judge_by_temp_jump_down_node != NULL && judge_by_temp_jump_down_node != jum_down && jum_down != NULL && jum_down->tdc_end_avg_temp - jum_down->tdc_start_avg_temp < -50)
    {
        judge_by_temp_jump_down_node = jum_down;
    }
    // 是小火时，碰到跳降，就立马变大火   对应函数func_boil_dumplings中＂跳降延迟判断＂第3个if
    if (fire_state == FIRE_SMALL && temp_have_first_down && judge_by_temp_jump_down_node != NULL && judge_by_temp_jump_down_node != jum_down && jum_down != NULL && jum_down->tdc_end_avg_temp - jum_down->tdc_start_avg_temp < -50)
    {
        judge_by_temp_jump_down_node = jum_down;
    }
}

void boil_dumplings_connect()
{
    if (!boil_dumplings_have_connect)
    {
        CA_SIG_CONNECT(g_buildin_signal_temp_down, judge_by_temp_down);
        CA_SIG_CONNECT(g_buildin_signal_temp_jump_down, judge_by_temp_jump_down);

        CA_SIG_CONNECT(g_buildin_signal_temp_gental, judge_by_temp_gental);
        
        CA_SIG_CONNECT(g_buildin_signal_temp_rise, count_down_restart_by_temp_rise);

        boil_dumplings_have_connect = true;
    }
}