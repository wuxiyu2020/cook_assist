#include <stdio.h>
#include "ca_apps.h"

#define log_module_name "cook_assistant"
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)
#define log_info(...)  LOGI(log_module_name, ##__VA_ARGS__)
#define log_warn(...)  LOGW(log_module_name, ##__VA_ARGS__)
#define log_error(...) LOGE(log_module_name, ##__VA_ARGS__)
#define log_fatal(...) LOGF(log_module_name, ##__VA_ARGS__)

static bool exist_tendency_node_duration_and_range(temp_tendency_info_t *p, unsigned int at_least_duration, unsigned int at_most_duration, unsigned short start_temp_range_floor, unsigned short start_temp_range_ceiling, unsigned short end_temp_range_floor, unsigned short end_temp_range_ceiling)
{                                                   
    while (p != NULL)
    {
        if (at_least_duration > 0        && p->tdc_end_tick - p->tdc_start_tick < at_least_duration) {p = p->next; continue;}
        if (at_most_duration > 0         && p->tdc_end_tick - p->tdc_start_tick > at_most_duration)  {p = p->next; continue;}
        if (start_temp_range_floor > 0   && p->tdc_start_avg_temp < start_temp_range_floor)          {p = p->next; continue;}
        if (start_temp_range_ceiling > 0 && p->tdc_start_avg_temp > start_temp_range_ceiling)        {p = p->next; continue;}
        if (end_temp_range_floor > 0     && p->tdc_end_avg_temp < end_temp_range_floor)              {p = p->next; continue;}
        if (end_temp_range_ceiling > 0   && p->tdc_end_avg_temp > end_temp_range_ceiling)            {p = p->next; continue;}
        return true;  // 走到这里说明条件都通过了
    }
    return false;
}

bool api_exist_gental_node_duration_and_range(unsigned int at_least_duration, unsigned int at_most_duration, unsigned short start_temp_range_floor, unsigned short start_temp_range_ceiling, unsigned short end_temp_range_floor, unsigned short end_temp_range_ceiling)
{
    log_debug("%s", __func__); 
    temp_tendency_info_t *p = g_buildin_temp_gental_tendency_head;
    return exist_tendency_node_duration_and_range(p, at_least_duration, at_most_duration, start_temp_range_floor, start_temp_range_ceiling, end_temp_range_floor, end_temp_range_ceiling);
}

bool api_exist_rise_node_duration_and_range(unsigned int at_least_duration, unsigned int at_most_duration, unsigned short start_temp_range_floor, unsigned short start_temp_range_ceiling, unsigned short end_temp_range_floor, unsigned short end_temp_range_ceiling)
{
    log_debug("%s", __func__);
    temp_tendency_info_t *p = g_buildin_temp_rise_tendency_head;
    return exist_tendency_node_duration_and_range(p, at_least_duration, at_most_duration, start_temp_range_floor, start_temp_range_ceiling, end_temp_range_floor, end_temp_range_ceiling);
}

bool api_exist_down_node_duration_and_range(unsigned int at_least_duration, unsigned int at_most_duration, unsigned short start_temp_range_floor, unsigned short start_temp_range_ceiling, unsigned short end_temp_range_floor, unsigned short end_temp_range_ceiling)
{
    log_debug("%s", __func__);
    temp_tendency_info_t *p = g_buildin_temp_down_tendency_head;
    return exist_tendency_node_duration_and_range(p, at_least_duration, at_most_duration, start_temp_range_floor, start_temp_range_ceiling, end_temp_range_floor, end_temp_range_ceiling);
}

bool api_exist_shake_node_duration_and_range(unsigned int at_least_duration, unsigned int at_most_duration, unsigned short start_temp_range_floor, unsigned short start_temp_range_ceiling, unsigned short end_temp_range_floor, unsigned short end_temp_range_ceiling)
{
    log_debug("%s", __func__);
    temp_tendency_info_t *p = g_buildin_temp_shake_tendency_head;
    return exist_tendency_node_duration_and_range(p, at_least_duration, at_most_duration, start_temp_range_floor, start_temp_range_ceiling, end_temp_range_floor, end_temp_range_ceiling);
}

bool api_exist_ise_ff_node_duration_and_range(unsigned int at_least_duration, unsigned int at_most_duration, unsigned short start_temp_range_floor, unsigned short start_temp_range_ceiling, unsigned short end_temp_range_floor, unsigned short end_temp_range_ceiling)
{
    log_debug("%s", __func__);
    temp_tendency_info_t *p = g_buildin_temp_rise_ff_tendency_head;
    return exist_tendency_node_duration_and_range(p, at_least_duration, at_most_duration, start_temp_range_floor, start_temp_range_ceiling, end_temp_range_floor, end_temp_range_ceiling);
}

unsigned int get_latest_avg_temp_of_10()
{
    if (g_buildin_signal_average_temp_of_10->valid_size > 0)
        return g_buildin_avg_arr10[g_buildin_signal_average_temp_of_10->valid_size - 1];
    else
        return 0;
}

unsigned int get_latest_avg_temp_of_4()
{
    if (g_buildin_signal_average_temp_of_4->valid_size > 0)
        return g_buildin_avg_arr4[g_buildin_signal_average_temp_of_4->valid_size - 1];
    else
        return 0;
}

bool api_is_cb_in_future(future_cb_type future_cb)
{
    future_cb_list_t *f = g_future_cb_list_head;
    while (f != NULL)
    {
        if (f->cb == future_cb)
            return true;
        f = f->next;
    }
    return false;
}


// 对外
void ca_api_register_fire_gear_cb(int (*cb)(const int gear, enum INPUT_DIR input_dir))  // 调节火力档位
{
    g_fire_gear_cb = cb;
}

void ca_api_register_close_fire_cb(int (*cb)(enum INPUT_DIR input_dir))  //关火回调
{
    g_close_fire_cb = cb;
}

void ca_api_register_hood_gear_cb(int (*cb)(const int gear))  //风机回调
{
    g_hood_gear_cb = cb;
}

void ca_api_register_fire_state_cb(int (*cb)(int fire_state, bool is_first_small))
{
    g_fire_state_cb = cb;
}

void ca_api_register_quick_cooking_info_cb(int (*cb)(cook_model_t *cook_model))
{
    g_quick_cooking_info_cb = cb;
}

void ca_api_register_hood_light_cb(int (*cb)(const int gear))
{
    g_hood_light_cb = cb;
}