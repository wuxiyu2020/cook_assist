#include <stdio.h>
#include "../ca_apps.h"

#define log_module_name "pot_cover"
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)
#define log_info(...)  LOGI(log_module_name, ##__VA_ARGS__)
#define log_warn(...)  LOGW(log_module_name, ##__VA_ARGS__)
#define log_error(...) LOGE(log_module_name, ##__VA_ARGS__)
#define log_fatal(...) LOGF(log_module_name, ##__VA_ARGS__)


sig_open_cover_t *g_sig_pot_cover_open_cover = NULL;  // 打开盖子信号  命名规则　g:全局　sig:是个信号 pot_cover:app名称　open_cover:信号名称
sig_close_cover_t *g_sig_pot_cover_close_cover = NULL;

// is_open_cover
static temp_tendency_info_t *last_open_cover_jum_rise = NULL;

// is_close_cover
static temp_tendency_info_t *last_close_cover_jum_down = NULL;

void reset_pot_cover_signals()
{
    if (g_sig_pot_cover_open_cover != NULL)
    {
        free(g_sig_pot_cover_open_cover);
        g_sig_pot_cover_open_cover = NULL;
    }
    if (g_sig_pot_cover_close_cover != NULL)
    {
        free(g_sig_pot_cover_close_cover);
        g_sig_pot_cover_close_cover = NULL;
    }
    last_open_cover_jum_rise = NULL;
    last_close_cover_jum_down = NULL;
}

/************************************************************* 信号触发逻辑　*************************************************************/
static void future_is_open_cover(future_cb_param_t *param)
{
    if (g_buildin_signal_average_temp_of_10->valid_size > 0 && g_buildin_avg_arr10[g_buildin_signal_average_temp_of_10->valid_size -1] > last_open_cover_jum_rise->tdc_end_avg_temp - 30)
        CA_SIG_EMIT(g_sig_pot_cover_open_cover);
}
static bool is_open_cover(sig_open_cover_t *signal)
{
    log_debug("%s", __func__);
    if (g_pot_cover_type == COVER_UNKONW || g_pot_cover_type == COVER_NONE)  // 不知道什么盖子或者没盖盖，就直接返回false
        return false;

    temp_tendency_info_t *p = g_buildin_temp_jump_rise_tendency_tail;
    if (p != NULL && g_buildin_signal_temp_jump_rise != NULL && p != last_open_cover_jum_rise)
    {
        // 开盖是相对盖盖的,如果没有盖盖,就直接判断,如果有盖盖,就得要求此次跳升时间在盖盖跳降的后面
        if (last_close_cover_jum_down == NULL || (last_close_cover_jum_down != NULL && p->tdc_start_tick > last_close_cover_jum_down->tdc_end_tick))
        {
            if (p->tdc_end_avg_temp - p->tdc_start_avg_temp > 80)
            {
                log_debug("判断开盖了");
                last_open_cover_jum_rise = p;
                CA_FUTURE(5 * 4, future_is_open_cover, NULL);  // 由于第三个参数是后来加的,这里的逻辑就不改了,传NULL
            }
        }
    }
    return false;
    // 问题：锅内与锅盖温度差不多时，开盖是不会跳升的　　待解决这种场景
}

static void future_is_close_cover(future_cb_param_t *param)
{
    if (g_buildin_signal_average_temp_of_10->valid_size > 0 && g_buildin_avg_arr10[g_buildin_signal_average_temp_of_10->valid_size -1] < last_close_cover_jum_down->tdc_end_avg_temp + 30)
        CA_SIG_EMIT(g_sig_pot_cover_close_cover);
}
static bool is_close_cover(sig_close_cover_t *signal)
{
    log_debug("%s", __func__);
    if (g_pot_cover_type != COVER_NONE)  // 只有开盖才往下执行
        return false;

    temp_tendency_info_t *p = g_buildin_temp_jump_down_tendency_tail;
    if (p != NULL && g_buildin_signal_temp_jump_down != NULL && p != last_close_cover_jum_down)
    {
        // 盖盖是相对开盖的,如果没有开盖,就直接判断,如果有开盖,就得要求此次跳降时间在开盖跳升的后面
        if (last_open_cover_jum_rise == NULL || (last_open_cover_jum_rise != NULL && p->tdc_start_tick > last_open_cover_jum_rise->tdc_end_tick))
        {
            if ( p->tdc_end_avg_temp - p->tdc_start_avg_temp < -80)
            {
                log_debug("判断盖盖了");
                last_close_cover_jum_down = p;
                CA_FUTURE(5 * 4, future_is_close_cover, NULL);  // 由于第三个参数是后来加的,这里的逻辑就不改了,传NULL
            }
        }
    }
    return false;
    // 问题：跳降也可能是加水
}
/************************************************************* 信号触发逻辑　*************************************************************/
// 遍历信号链表 针对信号做相应计算
int pot_cover_signals_worker(ca_signal_list_t *p)  // 返回int类型,0：说明信号类型属于该函数所在的业务模块；1：说明不是该模块的信号
{
    log_debug("%s", __func__);
    switch (p->type)
    {
        case SIG_POT_COVER_OPEN_COVER:
            if (is_open_cover(p->sig))
                CA_SIG_EMIT(p->sig);
            return 0;
        case SIG_POT_COVER_CLOSE_COVER:
            if (is_close_cover(p->sig))
                CA_SIG_EMIT(p->sig)
            return 0;
        default:
            return 1;
    }
}


/************************************************************* stat信号定义　*************************************************************/
static void pot_cover_add_signal_open_cover()  // 命名规则　app名称_add_signal_信号名称
{
    log_debug("%s", __func__);
    if (g_sig_pot_cover_open_cover == NULL)
    {
        g_sig_pot_cover_open_cover = malloc(sizeof(sig_open_cover_t));
        if (g_sig_pot_cover_open_cover == NULL)
        {
            log_error("g_sig_pot_cover_open_cover信号初始化失败");
            return;
        }
        g_sig_pot_cover_open_cover->type = SIG_POT_COVER_OPEN_COVER;
        CA_ADD_SIGNAL(g_sig_pot_cover_open_cover);
    }
}

static void pot_cover_add_signal_close_cover()
{
    log_debug("%s", __func__);
    if (g_sig_pot_cover_close_cover == NULL)
    {
        g_sig_pot_cover_close_cover = malloc(sizeof(sig_close_cover_t));
        if (g_sig_pot_cover_close_cover == NULL)
        {
            log_error("g_sig_pot_cover_close_cover信号初始化失败");
            return;
        }
        g_sig_pot_cover_close_cover->type = SIG_POT_COVER_CLOSE_COVER;

        CA_ADD_SIGNAL(g_sig_pot_cover_close_cover);
    }
}
/************************************************************* 信号定义end　*************************************************************/

void pot_cover_add_signals()
{
    log_debug("%s", __func__);
    if (!g_pot_cover_app_is_open)
        return;
    
    pot_cover_add_signal_open_cover();
    pot_cover_add_signal_close_cover();
}