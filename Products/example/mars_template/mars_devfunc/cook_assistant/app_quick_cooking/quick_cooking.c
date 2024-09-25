#include "../ca_apps.h"
#include "../fsyd.h"

#define log_module_name "quick_cooking"
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)
#define log_info(...)  LOGI(log_module_name, ##__VA_ARGS__)
#define log_warn(...)  LOGW(log_module_name, ##__VA_ARGS__)
#define log_error(...) LOGE(log_module_name, ##__VA_ARGS__)
#define log_fatal(...) LOGF(log_module_name, ##__VA_ARGS__)

/*　全局变量　*/
unsigned short g_quick_cooking_app_is_open = 0;  // 0关　　１开
bool g_is_need_reset_on_off_value = true;  // 标记是否需要恢复通断阀状态，默认是需要的
bool have_water_boil = false;
bool temp_have_first_down = false;  // 水开之后是否第一次温度下降
unsigned int temp_have_first_down_tick = 0;  // 水开之后是否第一次温度下降时间
bool count_down_lock = false;  // 倒计时锁
cook_model_t *g_cook_model = NULL;
int (*g_fire_state_cb)(int fire_state, bool is_first_small);   // 可删除
int (*g_quick_cooking_info_cb)(cook_model_t *cook_model);

bool soft_boil_egg_have_connect = false;
bool blanching_have_connect = false;
bool boil_dumplings_have_connect = false;
unsigned int last_turn_big_fire_tick = 0;  // 上次变大火的时间点
unsigned int last_turn_small_fire_tick = 0;  // 上次变小火的时间点

/*　静态变量　*/
static bool have_add_signals = false;
static bool have_add_signals_worker = false;
static unsigned int temp_not_down_after_30_seconds_loop = 0;

// 这里的g_cook_func_handle和COOK_NAME要与COOK_TYPE类型一一对应
static cook_funcs g_cook_func_handle[] = {func_soft_boil_egg, func_boil_dumplings, func_blanching};
char *COOK_NAME[] = {"溏心蛋", "煮饺子", "焯水"};

void reset_quick_cooking_value()
{
    log_debug("%s", __func__);

    reset_on_off_value();  // 重置通断阀状态

    if (g_cook_model != NULL)
    {
        free(g_cook_model);
        g_cook_model = NULL;
    }
    fire_state = FIRE_BIG;
    have_add_signals = false;
    have_add_signals_worker = false;
    have_water_boil = false;
    g_quick_cooking_app_is_open = 0;
    temp_have_first_down = false;
    temp_have_first_down_tick = 0;
    count_down_lock = false;
    temp_not_down_after_30_seconds_loop = 0;

    soft_boil_egg_have_connect = false;
    blanching_have_connect = false;
    boil_dumplings_have_connect = false;
    last_turn_big_fire_tick = 0;
    last_turn_small_fire_tick = 0;
    g_is_need_reset_on_off_value = true;  // 需要放到reset_on_off_value()后面重置

    reset_soft_boil_egg();
    reset_boil_dumplings();
}

// 重新开始倒计时
void count_down_reset()
{
    log_debug("%s", __func__);
    count_down_lock = false;
    // 产品说不要这个蜂鸣了
    // g_cook_model->cook_event = TEMP_HAVE_DOWN_THEN_WATER_BOIL;
    // if (g_quick_cooking_info_cb != NULL) g_quick_cooking_info_cb(g_cook_model);  // 传递TEMP_HAVE_DOWN_THEN_WATER_BOIL事件
    // log_debug("重新开始倒计时传递TEMP_HAVE_DOWN_THEN_WATER_BOIL事件");
    g_cook_model->total_tick = g_cook_model->seted_time * INPUT_DATA_HZ;
    g_cook_model->remaining_time = g_cook_model->seted_time;
    // small_fire_time = 20 * 4;
    // big_fire_time = 2 * 60 * 4;
    count_down();
}

// 保底计时
int savety_count_down()
{
    log_debug("%s", __func__);
    log_debug("保底计时还剩 %d 秒", g_cook_model->safety_time);
    if (g_ca_tick % 4 == 0)
        g_cook_model->safety_time--;
    if (g_cook_model->safety_time <= 0)
    {
        log_debug("一键烹饪触发保底关火");
        ca_close_fire(1);
        g_cook_model->cook_event = COOK_END;
        if (g_quick_cooking_info_cb != NULL) g_quick_cooking_info_cb(g_cook_model);  // 传递COOK_END事件
        reset_quick_cooking_value();
        return 1; 
    }
    return 0;
}

//　烹饪倒计时
void count_down()
{
    log_debug("%s", __func__);
    if (count_down_lock || g_quick_cooking_app_is_open != 1)  // count_down_lock水开后检测到第一次温度下降才会是true，水升温快开时为false
        return;

    /* 这个逻辑放这里的原因：如果水开后检测到了温度下降，但是温度下降到水开，这段时间可能超过30秒，这个时候是不要触发事件TEMP_NOT_DOWN_AFTER_30_SECONDS_LOOP的，
    *  此时count_down_lock是true的，正好上面就return了
    */
    if (g_cook_model->cook_type == BOIL_DUMPLINGS && !temp_have_first_down)  // 煮饺子的话，水开后没检测到温度下降就每30秒蜂鸣
    {
        temp_not_down_after_30_seconds_loop++;
        if (temp_not_down_after_30_seconds_loop % (30 * 4) == 0)
        {
            g_cook_model->cook_event = TEMP_NOT_DOWN_AFTER_30_SECONDS_LOOP;
            log_debug("水开后%d秒没检测到温度下降");
            if (g_quick_cooking_info_cb != NULL) g_quick_cooking_info_cb(g_cook_model);  // 传递TEMP_NOT_DOWN_AFTER_30_SECONDS_LOOP事件
        }
        return;
    }

    if (g_cook_model->total_tick % 4 == 0)
    {
        log_debug("一键烹饪还剩 %d 秒关火", g_cook_model->remaining_time);
        g_cook_model->cook_event = COOK_EVENT_NONE;
        if (g_quick_cooking_info_cb != NULL)  g_quick_cooking_info_cb(g_cook_model);
        g_cook_model->remaining_time--;
    }

    // if (g_cook_model->cook_type == BOIL_DUMPLINGS)
    // {
    //     temp_not_down_after_30_seconds_loop++;
    //     if (temp_have_first_down && !count_down_lock)  // 煮饺子的话，判断下了饺子后水开再之前都不倒计时
    //     {
    //         g_cook_model->total_tick--;
    //     }
    // }
    // else
    // {
    //     g_cook_model->total_tick--;
    // }
    g_cook_model->total_tick--;

    log_debug("g_cook_model->total_tick %d", g_cook_model->total_tick);
    if (g_cook_model->total_tick == 0)
    {
        log_debug("一键烹饪关火了");
        ca_close_fire(1);
        g_cook_model->cook_event = COOK_END;
        if (g_quick_cooking_info_cb != NULL) g_quick_cooking_info_cb(g_cook_model);  // 传递COOK_END事件
        reset_quick_cooking_value();
        return; 
    }

    // log_debug("控制计时器small_fire_time：%d, big_fire_time: %d ", small_fire_time, big_fire_time);
    // if (fire_state == FIRE_SMALL && temp_have_first_down && small_fire_time > 0)  // 小火的时候才开启小火时间倒计时
    // {
    //     small_fire_time--;
    // }

    // if (fire_state == FIRE_BIG && temp_have_first_down && big_fire_time > 0)
    // {
    //     big_fire_time--;
    // }
}

static void enter_cooking()
{
    log_debug("%s", __func__);
    if (!have_water_boil)
    {
        if (savety_count_down())  // 如果一直没有水开或者达到目标温度，当保底倒计时结束时，就关火
        {
            return;  // 保底倒计时到了，就直接返回了不继续运行了
        }
    }

    if (g_water_is_boil && !have_water_boil)
    {
        have_water_boil = true;
        log_debug("水开小火");
        // if (g_fire_state_cb != NULL)  增加了烹饪事件后，这个回调就没必要了，可以删除
            // g_fire_state_cb(FIRE_SMALL, true);  // 水开小火的时候第二个参数才是true,告知通讯板需要蜂鸣
        ca_set_fire(FIRE_SMALL, 1);
        last_turn_small_fire_tick = g_ca_tick;
        g_cook_model->cook_event = WATER_FIRST_BOIL;
        if (g_quick_cooking_info_cb != NULL) g_quick_cooking_info_cb(g_cook_model);  // 传递WATER_FIRST_BOIL事件
    }

    if (have_water_boil)
    {
        // g_cook_func_handle[g_cook_model->cook_type - 1] 这里-1是因为COOK_TYPE枚举里面有COOK_TYPE_START占了第一个，而g_cook_func_handle是没有start的
        g_cook_func_handle[g_cook_model->cook_type - 1]();
        count_down();
    }
}

static int init_cook_model(int cook_type, unsigned short seted_time, unsigned short safety_time)
{
    log_debug("%s", __func__);

    if (g_cook_model == NULL)
    {
        g_cook_model = malloc(sizeof(cook_model_t));
        if (g_cook_model == NULL)
        {
            log_debug("初始化g_cook_model内存失败！");
            return 1;
        }
    }

    g_cook_model->cook_type = cook_type;
    g_cook_model->seted_time = seted_time;
    g_cook_model->safety_time = 15 * 60;  // safety_time  先默认15分钟，不管传进来的是多少
    g_cook_model->cook_event = COOK_EVENT_NONE;
    g_cook_model->total_tick = g_cook_model->seted_time * INPUT_DATA_HZ;
    g_cook_model->remaining_time = g_cook_model->seted_time;
        
    return 0;
}

/// @brief 通讯板回调函数，通讯板告知是否要开启关闭一键烹饪
/// @param switch_flag 1开启　0关闭
/// @param cook_type 一键菜谱类型  当switch_flag为0时,不会用到,传1即可
/// @param seted_time 设置的时间  当switch_flag为0时,不会用到,传1即可
/// @param safety_time 保底时间   当switch_flag为0时,不会用到,传1即可
/// @return 0: 正常处理   1: 错误的烹饪类型
int quick_cooking_switch(unsigned short switch_flag, int cook_type, unsigned short seted_time, unsigned short safety_time)
{
    log_debug("%s", __func__);

    g_quick_cooking_app_is_open = switch_flag;
    if (switch_flag == 0)
        reset_quick_cooking_value();
    else if (switch_flag == 1)
    {
        if (cook_type <= COOK_TYPE_START || cook_type >= COOK_TYPE_END)
        {
            log_warn("错误的烹饪类型：%d", cook_type);
            free(g_cook_model);
            g_cook_model = NULL;
            g_quick_cooking_app_is_open = 0;
            return 1;
        }
        return init_cook_model(cook_type, seted_time, safety_time);
    }
    return 0;
}

// 还没有et70机器时,用E70风速来代替不同模式启动    测试程序也用这个入口  对应app_quick_cooking中的调试入口  生产环境也可以不用删除
int quick_cooking_switch_for_test(unsigned short switch_flag)
{
    log_debug("%s", __func__);
    if (switch_flag == 0)
        quick_cooking_switch(0, 1, 1, 1);
    else if (switch_flag == 1)
    {
#ifdef PRODUCTION
        mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
        // if (mars_template_ctx->status.RAuxiliaryTemp == 150) return 1; // 用于E70测试  辅助控温触发用这个  风速触发
        if (!(mars_template_ctx->status.HoodSpeed == 1 || mars_template_ctx->status.HoodSpeed == 2 || mars_template_ctx->status.HoodSpeed == 3))
        {
            g_quick_cooking_app_is_open = 0;  // 风机档位错误,把开关置为0
            return 1;  // 风机档位触发
        }
        log_debug("代替标识：%d", mars_template_ctx->status.HoodSpeed);  // 辅助控温按键坏了，换风机档位  1低  2中  3高   HoodSpeed   RAuxiliaryTemp
        if (mars_template_ctx->status.HoodSpeed == 1)
            quick_cooking_switch(1, 1, 6 * 60, 15 * 60);  // 类型   设置时间  保底时间
        else if (mars_template_ctx->status.HoodSpeed == 2)
            quick_cooking_switch(1, 2, 5 * 60, 15 * 60);
        else if (mars_template_ctx->status.HoodSpeed == 3)
            quick_cooking_switch(1, 3, 3 * 60, 15 * 60);
#endif
#ifndef PRODUCTION
        // 跑测试程序时,写死模式和时间
        quick_cooking_switch(1, SOFT_BOILED_EGG, 6 * 60, 15 * 60);  // 类型   设置时间  保底时间
        // quick_cooking_switch(1, BOIL_DUMPLINGS,  5 * 60, 15 * 60);
        // quick_cooking_switch(1, BLANCHING,       3 * 60, 15 * 60);
#endif
    }
    return 0;
}

void app_quick_cooking()
{
    log_debug("%s", __func__);

#ifdef PRODUCTION
    // ET70上需要注释  生产环境一定要注释这部分
    // mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    // if (hood_speed != mars_template_ctx->status.HoodSpeed) 
    // {
    //     hood_speed = mars_template_ctx->status.HoodSpeed;
    //     quick_cooking_switch_for_test(1);
    // }
    // if (g_quick_cooking_app_is_open == 0)  // E70上调试用,因为这个存在的原因,下面检测到移锅,再放锅,经过这里,又会打开,因此E70上调试时,通过看日志是否退出过了
        // quick_cooking_switch_for_test(1);
#endif

    // 水开之后移开锅，就退出一键烹饪  去掉have_water_boil条件,原来是水开后移锅退出,现在改成只要移锅,就退出 目前这样没开时放上锅,又会继续的(只是E70)
    // get_latest_avg_temp_of_10() > 1200 目前三种模式都是水模式，先用150度来判断，后期有油的再想办法
    if (get_latest_avg_temp_of_10() > 1200 && g_cook_model != NULL && g_quick_cooking_info_cb != NULL)
    {
        log_debug("检测到移锅,退出一键烹饪.g_quick_cooking_app_is_open :%d", g_quick_cooking_app_is_open);
        g_cook_model->cook_event = COOK_END;
        g_quick_cooking_info_cb(g_cook_model);  // 传递COOK_END事件

        state_handle_t *state_handle = get_input_handle(1);
        if (state_handle->pan_fire_switch)
            g_is_need_reset_on_off_value = false;  // 如果此时移锅小火功能开关是打开的，就不恢复通断阀
        quick_cooking_switch(0, 1, 1, 1);
    }

    if (g_quick_cooking_app_is_open == 0) return;
    if (g_cook_model == NULL) return;
    if (g_cook_model->total_tick <= 0 ) return;  // 倒计时结束就不下去了

    // if (!have_add_signals)  // 暂时没有自己的信号
    // {
    //     quick_cooking_add_signals();
    //     have_add_signals = true;
    // }
    // if (!have_add_signals_worker)
    // {
    //     ADD_USER_SIGNAL_WORKER(quick_cooking_signals_worker);
    //     have_add_signals_worker = true;
    // }

    if (g_cook_model->cook_type == SOFT_BOILED_EGG)
        soft_boil_egg_connect();
    if (g_cook_model->cook_type == BLANCHING)
        blanching_connect();
    if (g_cook_model->cook_type == BOIL_DUMPLINGS)
        boil_dumplings_connect();

    // COOK_NAME[g_cook_model->cook_type - 1] 这里-1是因为COOK_TYPE枚举里面有COOK_TYPE_START占了第一个，而COOK_NAME是没有start的
    log_debug("当前模式 %s 设置时间 %d  剩余时间%d", COOK_NAME[g_cook_model->cook_type - 1], g_cook_model->seted_time, g_cook_model->remaining_time);
    enter_cooking();
}