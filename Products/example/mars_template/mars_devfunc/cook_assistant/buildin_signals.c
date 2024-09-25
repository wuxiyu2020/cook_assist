#include <string.h>
#include <stdio.h>
#include "buildin_signals.h"

#define log_module_name "cook_assistant"
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)
#define log_info(...)  LOGI(log_module_name, ##__VA_ARGS__)
#define log_warn(...)  LOGW(log_module_name, ##__VA_ARGS__)
#define log_error(...) LOGE(log_module_name, ##__VA_ARGS__)
#define log_fatal(...) LOGF(log_module_name, ##__VA_ARGS__)

/* *************************buildin 信号************************* */
// 每1个温度算一次平均值，存20个
unsigned short g_buildin_avg_arr1[BUILDIN_AVG_ARR_SEIZ_1];
sig_average_temp_of_n_t *g_buildin_signal_average_temp_of_1 = NULL;

// 每10个温度算一次平均值，存10个
unsigned short g_buildin_avg_arr10[BUILDIN_AVG_ARR_SEIZ_10];
sig_average_temp_of_n_t *g_buildin_signal_average_temp_of_10 = NULL;

// 每4个温度算一次平均值，存10个
unsigned short g_buildin_avg_arr4[BUILDIN_AVG_ARR_SEIZ_4];
sig_average_temp_of_n_t *g_buildin_signal_average_temp_of_4 = NULL;

// // 用每4个温度计算的平均值数组，判断温度趋势在上升
// sig_temp_rise_tendency_t *g_buildin_signal_temp_rise = NULL;
// average_compare_rule_t buildin_avg_arr4_compare_temp_rise_rules_arr[BUILDIN_AVG_ARR4_COMPARE_TEMP_RISE_RULE_SIZE];
// sig_average_compare_t *g_buildin_signal_avg_arr4_compare_temp_rise = NULL;
// static unsigned short last_rise_start_temp = 0;
// static unsigned short last_rise_start_tick = 0;
// static unsigned short last_rise_end_temp = 0;
// static unsigned short last_rise_end_tick = 0;
// int g_buildin_snatch_rise_slope = 0;  // 单个节点瞬时上升速率
// 用每10个温度计算的平均值数组，判断温度趋势在上升
sig_temp_rise_tendency_t *g_buildin_signal_temp_rise = NULL;
average_compare_rule_t buildin_avg_arr10_compare_temp_rise_rules_arr[BUILDIN_AVG_ARR10_COMPARE_TEMP_RISE_RULE_SIZE];
sig_average_compare_t *g_buildin_signal_avg_arr10_compare_temp_rise = NULL;
static unsigned short last_rise_start_temp = 0;
static unsigned short last_rise_start_tick = 0;
static unsigned short last_rise_end_temp = 0;
static unsigned short last_rise_end_tick = 0;
int g_buildin_snatch_rise_slope = 0;  // 单个节点瞬时上升速率

// 用每4个温度计算的平均值数组，判断温度趋势在下降
sig_temp_down_tendency_t *g_buildin_signal_temp_down = NULL;
average_compare_rule_t buildin_avg_arr4_compare_temp_down_rules_arr[BUILDIN_AVG_ARR4_COMPARE_TEMP_DOWN_RULE_SIZE];
sig_average_compare_t *g_buildin_signal_avg_arr4_compare_temp_down = NULL;
static unsigned short last_down_start_temp = 0;
static unsigned short last_down_start_tick = 0;
static unsigned short last_down_end_temp = 0;
static unsigned short last_down_end_tick = 0;

// 用每4个温度计算的平均值数组，判断温度上升越来越快
sig_rise_faster_and_faster_t *g_buildin_signal_temp_rise_ff = NULL;

// 用每1个温度计算的平均值数组，判断温度是趋势是平稳
sig_temp_gental_tendency_t *g_buildin_signal_temp_gental = NULL;
average_compare_rule_t buildin_avg_arr1_compare_temp_gental_rules_arr[BUILDIN_AVG_ARR1_COMPARE_TEMP_GENTAL_RULE_SIZE];
sig_average_compare_t *g_buildin_signal_avg_arr1_compare_temp_gental = NULL;

// 用每1个温度计算的平均值数组，判断温度跳升
sig_temp_jump_rise_tendency_t *g_buildin_signal_temp_jump_rise = NULL;
average_compare_rule_t buildin_avg_arr1_compare_temp_jump_rise_rules_arr[BUILDIN_AVG_ARR1_COMPARE_TEMP_JUMP_RISE_RULE_SIZE];
sig_average_compare_t *g_buildin_signal_avg_arr1_compare_temp_jump_rise = NULL;

// 用每1个温度计算的平均值数组，判断温度跳降
sig_temp_jump_down_tendency_t *g_buildin_signal_temp_jump_down = NULL;
average_compare_rule_t buildin_avg_arr1_compare_temp_jump_down_rules_arr[BUILDIN_AVG_ARR1_COMPARE_TEMP_JUMP_DOWN_RULE_SIZE];
sig_average_compare_t *g_buildin_signal_avg_arr1_compare_temp_jump_down = NULL;

// 用跳升跳降判断大幅波动  用每1个温度计算的平均值数组，判断温度大幅波动
sig_temp_shake_tendency_t *g_buildin_signal_temp_shake = NULL;
average_compare_rule_t buildin_avg_arr1_compare_temp_shake_rules_arr[BUILDIN_AVG_ARR1_COMPARE_TEMP_SHAKE_RULE_SIZE];
sig_average_compare_t *g_buildin_signal_avg_arr1_compare_temp_shake = NULL;
/* *************************buildin 信号************************* */

static bool have_add_signals = false;

// 重置一些变量，避免影响下次烹饪
void reset_buildin_signals()
{
    log_debug("%s", __func__);
    have_add_signals = false;
    last_rise_start_temp = 0;
    last_rise_start_tick = 0;
    last_rise_end_temp = 0;
    last_rise_end_tick = 0;
    g_buildin_snatch_rise_slope = 0;
    last_down_start_temp = 0;
    last_down_start_tick = 0;
    last_down_end_temp = 0;
    last_down_end_tick = 0;

    if (g_buildin_signal_average_temp_of_1 != NULL)
    {
        free(g_buildin_signal_average_temp_of_1);
        g_buildin_signal_average_temp_of_1 = NULL;
    }
    if (g_buildin_signal_average_temp_of_10 != NULL)
    {
        free(g_buildin_signal_average_temp_of_10);
        g_buildin_signal_average_temp_of_10 = NULL;
    }
    if (g_buildin_signal_average_temp_of_4 != NULL)
    {
        free(g_buildin_signal_average_temp_of_4);
        g_buildin_signal_average_temp_of_4 = NULL;
    }

    if (g_buildin_signal_temp_rise != NULL)
    {
        free(g_buildin_signal_temp_rise);
        g_buildin_signal_temp_rise = NULL;
    }
    if (g_buildin_signal_avg_arr10_compare_temp_rise != NULL)
    {
        free(g_buildin_signal_avg_arr10_compare_temp_rise);
        g_buildin_signal_avg_arr10_compare_temp_rise = NULL;
    }

    if (g_buildin_signal_temp_down != NULL)
    {
        free(g_buildin_signal_temp_down);
        g_buildin_signal_temp_down = NULL;
    }
    if (g_buildin_signal_avg_arr4_compare_temp_down != NULL)
    {
        free(g_buildin_signal_avg_arr4_compare_temp_down);
        g_buildin_signal_avg_arr4_compare_temp_down = NULL;
    }

    if (g_buildin_signal_temp_rise_ff != NULL)
    {
        free(g_buildin_signal_temp_rise_ff);
        g_buildin_signal_temp_rise_ff = NULL;
    }

    if (g_buildin_signal_temp_gental != NULL)
    {
        free(g_buildin_signal_temp_gental);
        g_buildin_signal_temp_gental = NULL;
    }
    if (g_buildin_signal_avg_arr1_compare_temp_gental != NULL)
    {
        free(g_buildin_signal_avg_arr1_compare_temp_gental);
        g_buildin_signal_avg_arr1_compare_temp_gental = NULL;
    }

    if (g_buildin_signal_temp_jump_rise != NULL)
    {
        free(g_buildin_signal_temp_jump_rise);
        g_buildin_signal_temp_jump_rise = NULL;
    }
    if (g_buildin_signal_avg_arr1_compare_temp_jump_rise != NULL)
    {
        free(g_buildin_signal_avg_arr1_compare_temp_jump_rise);
        g_buildin_signal_avg_arr1_compare_temp_jump_rise = NULL;
    }

    if (g_buildin_signal_temp_jump_down != NULL)
    {
        free(g_buildin_signal_temp_jump_down);
        g_buildin_signal_temp_jump_down = NULL;
    }
    if (g_buildin_signal_avg_arr1_compare_temp_jump_down != NULL)
    {
        free(g_buildin_signal_avg_arr1_compare_temp_jump_down);
        g_buildin_signal_avg_arr1_compare_temp_jump_down = NULL;
    }

    if (g_buildin_signal_temp_shake != NULL)
    {
        free(g_buildin_signal_temp_shake);
        g_buildin_signal_temp_shake = NULL;
    }
    if (g_buildin_signal_avg_arr1_compare_temp_shake != NULL)
    {
        free(g_buildin_signal_avg_arr1_compare_temp_shake);
        g_buildin_signal_avg_arr1_compare_temp_shake = NULL;
    }
}

/* *************************call_back 函数************************* */
static void print_add_avg_arr1()  // buildin_avg_arr1数组每来一个新的都会打印一次
{
    int i;
    char log_str[BUILDIN_AVG_ARR_SEIZ_1 * 4 + 20] = {0};
    for (i=0; i<g_buildin_signal_average_temp_of_1->valid_size; i++)
    {
        char s[10];
        sprintf(s, "%d ", g_buildin_signal_average_temp_of_1->avg_arr[i]);
        strcat(log_str, s);
    }
    log_debug("每1个温度计算的平均数数组：%s", log_str);
}

static void print_add_avg_arr10()  // buildin_avg_arr10数组每来一个新的都会打印一次
{
    int i;
    char log_str[BUILDIN_AVG_ARR_SEIZ_10 * 4 + 20] = {0};
    for (i=0; i<g_buildin_signal_average_temp_of_10->valid_size; i++)
    {
        char s[10];
        sprintf(s, "%d ", g_buildin_signal_average_temp_of_10->avg_arr[i]);
        strcat(log_str, s);
    }
    log_debug("每10个温度计算的平均数数组：%s", log_str);
}

static void print_add_avg_arr4()
{
    int i;
    char log_str[BUILDIN_AVG_ARR_SEIZ_4 * 4 + 20] = {0};
    for (i=0; i<g_buildin_signal_average_temp_of_4->valid_size; i++)
    {
        char s[10];
        sprintf(s, "%d ", g_buildin_signal_average_temp_of_4->avg_arr[i]);
        strcat(log_str, s);
    }
    log_debug("每4个温度计算的平均数数组：%s", log_str);
}

static void buildin_signal_temp_gental_cb()
{
    log_debug("%s", __func__);
    temp_tendency_info_t *p = g_buildin_temp_gental_tendency_head;
    while (p != NULL )
    {
        log_debug("最新平稳趋势：在%d附近平稳持续了%d(%d-%d)个ticks", p->tdc_start_avg_temp, p->tdc_end_tick - p->tdc_start_tick + 1, p->tdc_start_tick, p->tdc_end_tick);
        p = p->next;
    }
}

static void buildin_signal_temp_rise_cb()
{
    log_debug("%s", __func__);
    temp_tendency_info_t *p = g_buildin_temp_rise_tendency_head;
    while (p != NULL )
    {
        log_debug("最新上升趋势：平均温度从%d到%d花费了%d(%d-%d)个ticks", p->tdc_start_avg_temp, p->tdc_end_avg_temp, p->tdc_end_tick - p->tdc_start_tick + 1, p->tdc_start_tick, p->tdc_end_tick);
        p = p->next;
    }

    p = g_buildin_temp_rise_tendency_tail;

    if (p != NULL)
    {
        // p->tdc_end_avg_temp > last_rise_end_temp 这个条件保证斜率时正的,负数做除法会得到不可预料的值.而且这是在上升的情景中,负斜率也没用
        // last_rise_start_temp == p->tdc_start_avg_temp && p->tdc_end_tick - last_rise_end_tick != 0 要求在同一个上升节点上，更新了end温度后来算斜率
        if (p->tdc_end_avg_temp > last_rise_end_temp && last_rise_end_temp != 0 && last_rise_start_temp == p->tdc_start_avg_temp && p->tdc_end_tick - last_rise_end_tick != 0)
        {
            g_buildin_snatch_rise_slope = 1000 * (p->tdc_end_avg_temp - last_rise_end_temp) / (p->tdc_end_tick - last_rise_end_tick);
            log_debug("短时上升速率(扩大1000倍)：%d", g_buildin_snatch_rise_slope);
        }
        else
        {
            g_buildin_snatch_rise_slope = 0;
        }
        last_rise_start_temp = p->tdc_start_avg_temp;
        last_rise_start_tick = p->tdc_start_tick;
        last_rise_end_temp = p->tdc_end_avg_temp;
        last_rise_end_tick = p->tdc_end_tick;
    }
}

static void buildin_signal_temp_down_cb()
{
    log_debug("%s", __func__);
    temp_tendency_info_t *p = g_buildin_temp_down_tendency_head;
    while (p != NULL )
    {
        log_debug("最新下降趋势：平均温度从%d到%d花费了%d(%d-%d)个ticks", p->tdc_start_avg_temp, p->tdc_end_avg_temp, p->tdc_end_tick - p->tdc_start_tick + 1, p->tdc_start_tick, p->tdc_end_tick);
        p = p->next;
    }

    p = g_buildin_temp_down_tendency_tail;
    if (p != NULL)
    {
        if (p->tdc_end_avg_temp < last_down_end_temp && last_down_end_temp != 0 && last_down_start_temp == p->tdc_start_avg_temp && p->tdc_end_tick - last_down_end_tick != 0)
        {
            // 负数做除法有问题，把减数和被减数换个位置，日志里面加个负号
            int slope = 1000 * (last_down_end_temp - p->tdc_end_avg_temp) / (p->tdc_end_tick - last_down_end_tick);
            log_debug("短时下降速率(扩大1000倍)：-%d", slope);
        }
        last_down_start_temp = p->tdc_start_avg_temp;
        last_down_start_tick = p->tdc_start_tick;
        last_down_end_temp = p->tdc_end_avg_temp;
        last_down_end_tick = p->tdc_end_tick;
    }
}

static void buildin_signal_temp_jump_rise_cb()
{
    log_debug("%s", __func__);
    temp_tendency_info_t *p = g_buildin_temp_jump_rise_tendency_head;
    while (p != NULL )
    {
        log_debug("跳升趋势：从%d到%d花费了%d(%d-%d)个ticks", p->tdc_start_avg_temp, p->tdc_end_avg_temp, p->tdc_end_tick - p->tdc_start_tick + 1, p->tdc_start_tick, p->tdc_end_tick);
        p = p->next;
    }
}

static void buildin_signal_temp_jump_down_cb()
{
    log_debug("%s", __func__);
    temp_tendency_info_t *p = g_buildin_temp_jump_down_tendency_head;
    while (p != NULL )
    {
        log_debug("跳降趋势：从%d到%d花费了%d(%d-%d)个ticks", p->tdc_start_avg_temp, p->tdc_end_avg_temp, p->tdc_end_tick - p->tdc_start_tick + 1, p->tdc_start_tick, p->tdc_end_tick);
        p = p->next;
    }
}

static void buildin_signal_temp_shake_cb()
{
    log_debug("%s", __func__);
    temp_tendency_info_t *p = g_buildin_temp_shake_tendency_head;
    while (p != NULL )
    {
        log_debug("大幅波动趋势：从%d到%d花费了%d(%d-%d)个ticks", p->tdc_start_avg_temp, p->tdc_end_avg_temp, p->tdc_end_tick - p->tdc_start_tick + 1, p->tdc_start_tick, p->tdc_end_tick);
        p = p->next;
    }
}

static void buildin_signal_temp_rise_ff_cb()
{
    log_debug("%s", __func__);
    temp_tendency_info_t *p = g_buildin_temp_rise_ff_tendency_head;
    while (p != NULL )
    {
        log_debug("上升越来越快趋势：从%d到%d花费了%d(%d-%d)个ticks", p->tdc_start_avg_temp, p->tdc_end_avg_temp, p->tdc_end_tick - p->tdc_start_tick + 1, p->tdc_start_tick, p->tdc_end_tick);
        p = p->next;
    }
}
/* *************************call_back 函数************************* */


/***********************************************************
*                       信号初始化　注册                      *
************************************************************/
// 每1个温度计算平均值
static void buildin_add_signal_avg_arr1()
{
    // 每隔1个温度算一次平均值，存10个
    if (g_buildin_signal_average_temp_of_1 == NULL)
    {
        g_buildin_signal_average_temp_of_1 = malloc(sizeof(sig_average_temp_of_n_t));
        memset(g_buildin_signal_average_temp_of_1, 0, sizeof(sig_average_temp_of_n_t));
        g_buildin_signal_average_temp_of_1->interval = BUILDIN_AVG_INTERVAL_1;
        g_buildin_signal_average_temp_of_1->arr_size = BUILDIN_AVG_ARR_SEIZ_1;
        g_buildin_signal_average_temp_of_1->valid_size = 0;
        g_buildin_signal_average_temp_of_1->avg_arr = g_buildin_avg_arr1;
        g_buildin_signal_average_temp_of_1->type = SIG_AVERAGE_TEMP_OF_N;
        g_buildin_signal_average_temp_of_1->sum = 0;

        CA_ADD_SIGNAL(g_buildin_signal_average_temp_of_1);
        CA_SIG_CONNECT(g_buildin_signal_average_temp_of_1, print_add_avg_arr1);
    }
}

// 每10个温度计算平均值
static void buildin_add_signal_avg_arr10()
{
    // 每隔10个温度算一次平均值，存5个
    if (g_buildin_signal_average_temp_of_10 == NULL)
    {
        g_buildin_signal_average_temp_of_10 = malloc(sizeof(sig_average_temp_of_n_t));
        memset(g_buildin_signal_average_temp_of_10, 0, sizeof(sig_average_temp_of_n_t));
        g_buildin_signal_average_temp_of_10->interval = BUILDIN_AVG_INTERVAL_10;
        g_buildin_signal_average_temp_of_10->arr_size = BUILDIN_AVG_ARR_SEIZ_10;
        g_buildin_signal_average_temp_of_10->valid_size = 0;
        g_buildin_signal_average_temp_of_10->avg_arr = g_buildin_avg_arr10;
        g_buildin_signal_average_temp_of_10->type = SIG_AVERAGE_TEMP_OF_N;
        g_buildin_signal_average_temp_of_10->sum = 0;

        CA_ADD_SIGNAL(g_buildin_signal_average_temp_of_10);
        CA_SIG_CONNECT(g_buildin_signal_average_temp_of_10, print_add_avg_arr10);
    }
}

// 每4个温度计算平均值
static void buildin_add_signal_avg_arr4()
{
    if (g_buildin_signal_average_temp_of_4 == NULL)
    {
        g_buildin_signal_average_temp_of_4 = malloc(sizeof(sig_average_temp_of_n_t));
        memset(g_buildin_signal_average_temp_of_4, 0, sizeof(sig_average_temp_of_n_t));
        g_buildin_signal_average_temp_of_4->interval = BUILDIN_AVG_INTERVAL_4;
        g_buildin_signal_average_temp_of_4->arr_size = BUILDIN_AVG_ARR_SEIZ_4;
        g_buildin_signal_average_temp_of_4->valid_size = 0;
        g_buildin_signal_average_temp_of_4->avg_arr = g_buildin_avg_arr4;
        g_buildin_signal_average_temp_of_4->type = SIG_AVERAGE_TEMP_OF_N;
        g_buildin_signal_average_temp_of_4->sum = 0;

        CA_ADD_SIGNAL(g_buildin_signal_average_temp_of_4);
        CA_SIG_CONNECT(g_buildin_signal_average_temp_of_4, print_add_avg_arr4);
    }
}

/* ************************* start平稳趋势　************************* */
// 作为　g_buildin_signal_temp_gental 的依赖信号
static void buildin_add_signal_avg_arr1_compare_dependency_of_temp_gental()
{
    log_debug("%s", __func__);
    if (g_buildin_signal_avg_arr1_compare_temp_gental == NULL)
    {
        g_buildin_signal_avg_arr1_compare_temp_gental = malloc(sizeof(sig_average_compare_t));
        memset(g_buildin_signal_avg_arr1_compare_temp_gental, 0, sizeof(sig_average_compare_t));
        g_buildin_signal_avg_arr1_compare_temp_gental->sig_average_temp_of_n = g_buildin_signal_average_temp_of_1;
        g_buildin_signal_avg_arr1_compare_temp_gental->average_compare_rules_arr_size = BUILDIN_AVG_ARR1_COMPARE_TEMP_GENTAL_RULE_SIZE;
        g_buildin_signal_avg_arr1_compare_temp_gental->type = SIG_AVERAGE_COMPARE;
        g_buildin_signal_avg_arr1_compare_temp_gental->rule_relation = AND;

        // 平稳就需要用绝对值了  要绝对平稳的话，就等于
        buildin_avg_arr1_compare_temp_gental_rules_arr[0].compare_type = COMPARE_LTE;
        buildin_avg_arr1_compare_temp_gental_rules_arr[0].calculate_type = CALC_ABS;
        buildin_avg_arr1_compare_temp_gental_rules_arr[0].minuend_index = 19;
        buildin_avg_arr1_compare_temp_gental_rules_arr[0].subtrahend_index = 0;
        buildin_avg_arr1_compare_temp_gental_rules_arr[0].compare_target = 20;
        
        buildin_avg_arr1_compare_temp_gental_rules_arr[1].compare_type = COMPARE_LTE; 
        buildin_avg_arr1_compare_temp_gental_rules_arr[1].calculate_type = CALC_ABS;
        buildin_avg_arr1_compare_temp_gental_rules_arr[1].minuend_index = 18; 
        buildin_avg_arr1_compare_temp_gental_rules_arr[1].subtrahend_index = 1; 
        buildin_avg_arr1_compare_temp_gental_rules_arr[1].compare_target = 20;
        
        buildin_avg_arr1_compare_temp_gental_rules_arr[2].compare_type = COMPARE_LTE;
        buildin_avg_arr1_compare_temp_gental_rules_arr[2].calculate_type = CALC_ABS;
        buildin_avg_arr1_compare_temp_gental_rules_arr[2].minuend_index = 17;
        buildin_avg_arr1_compare_temp_gental_rules_arr[2].subtrahend_index = 2;
        buildin_avg_arr1_compare_temp_gental_rules_arr[2].compare_target = 20;
        
        buildin_avg_arr1_compare_temp_gental_rules_arr[3].compare_type = COMPARE_LTE;
        buildin_avg_arr1_compare_temp_gental_rules_arr[3].calculate_type = CALC_ABS;
        buildin_avg_arr1_compare_temp_gental_rules_arr[3].minuend_index = 16;
        buildin_avg_arr1_compare_temp_gental_rules_arr[3].subtrahend_index = 3;
        buildin_avg_arr1_compare_temp_gental_rules_arr[3].compare_target = 20;
        
        buildin_avg_arr1_compare_temp_gental_rules_arr[4].compare_type = COMPARE_LTE;
        buildin_avg_arr1_compare_temp_gental_rules_arr[4].calculate_type = CALC_ABS;
        buildin_avg_arr1_compare_temp_gental_rules_arr[4].minuend_index = 15;
        buildin_avg_arr1_compare_temp_gental_rules_arr[4].subtrahend_index = 4;
        buildin_avg_arr1_compare_temp_gental_rules_arr[4].compare_target = 20;
        
        buildin_avg_arr1_compare_temp_gental_rules_arr[5].compare_type = COMPARE_LTE;
        buildin_avg_arr1_compare_temp_gental_rules_arr[5].calculate_type = CALC_ABS;
        buildin_avg_arr1_compare_temp_gental_rules_arr[5].minuend_index = 10;
        buildin_avg_arr1_compare_temp_gental_rules_arr[5].subtrahend_index = 9;
        buildin_avg_arr1_compare_temp_gental_rules_arr[5].compare_target = 20;

        g_buildin_signal_avg_arr1_compare_temp_gental->average_compare_rules_arr = buildin_avg_arr1_compare_temp_gental_rules_arr;
        
        // CA_ADD_SIGNAL(g_buildin_signal_avg_arr1_compare_temp_gental); 仅仅作为依赖信号的话，可以不用ADD
    }
}

// 利用对比信号　g_buildin_signal_avg_arr1_compare_temp_gental　来识别烹饪过程中出现的平稳区段
static void buildin_add_signal_temp_tendency_gental()
{
    log_debug("%s", __func__);

    if (g_buildin_signal_temp_gental == NULL)
    {
        g_buildin_signal_temp_gental = malloc(sizeof(sig_temp_gental_tendency_t));
        memset(g_buildin_signal_temp_gental, 0, sizeof(sig_temp_gental_tendency_t));
        g_buildin_signal_temp_gental->sig_average_compare = g_buildin_signal_avg_arr1_compare_temp_gental;
        g_buildin_signal_temp_gental->type = SIG_TEMP_TENDENCY_GENTAL;
        g_buildin_signal_temp_gental->at_least_duration_ticks = 10 * 4;  // 至少平稳10秒  这里可以设置小点，具体的业务场景需要加长，可以在具体场景里通过具体的时长判断
        g_buildin_signal_temp_gental->is_set_range = false;  // 不设置范围
        g_buildin_signal_temp_gental->gental_range_floor = 0; g_buildin_signal_temp_gental->gental_range_ceiling = 0;  // 虽然不设置范围，但最好要给他们设置值，并且都是0
        g_buildin_signal_temp_gental->at_most_diff_of_tdc_start_avg_temp_and_tdc_end_avg_temp = 10;  // 趋势开始结束差值不超过２度　abs(e-s) <= 20

        CA_ADD_SIGNAL(g_buildin_signal_temp_gental);
        CA_SIG_CONNECT(g_buildin_signal_temp_gental, buildin_signal_temp_gental_cb);
    }
}

// 利用对比信号　g_buildin_signal_avg_arr1_compare_temp_gental　来识别烹饪过程中出现的平稳区段
// 并且平稳出现在20--50度之间
// static void buildin_add_signal_temp_tendency_gental()
// {
//     log_debug("%s", __func__);

//     if (g_buildin_signal_temp_gental == NULL)
//     {
//         g_buildin_signal_temp_gental = malloc(sizeof(sig_temp_gental_tendency_t));
//         memset(g_buildin_signal_temp_gental, 0, sizeof(sig_temp_gental_tendency_t));
//         g_buildin_signal_temp_gental->sig_average_compare = g_buildin_signal_avg_arr1_compare_temp_gental;
//         g_buildin_signal_temp_gental->type = SIG_TEMP_TENDENCY_GENTAL;
//         g_buildin_signal_temp_gental->at_least_duration_ticks = 4 * 30;  // 至少平稳30秒
//         g_buildin_signal_temp_gental->is_set_range = true;  // 设置范围
//         g_buildin_signal_temp_gental->gental_range_floor = 200;
//         g_buildin_signal_temp_gental->gental_range_ceiling = 500;
//         g_buildin_signal_temp_gental->diff_of_tdc_start_avg_temp_and_tdc_end_avg_temp = 20;  // 趋势开始结束差值不超过２度　abs(e-s) <= 20

//         CA_ADD_SIGNAL(g_buildin_signal_temp_gental);
//         CA_SIG_CONNECT(g_buildin_signal_temp_gental, buildin_signal_temp_gental_cb);
//     }
// }
/* ************************* 平稳趋势end　************************* */

/* ************************* start上升趋势　************************* */
static void buildin_add_signal_avg_arr4_compare_dependency_of_temp_rise()
{
    log_debug("%s", __func__);
    if (g_buildin_signal_avg_arr10_compare_temp_rise == NULL)
    {
        g_buildin_signal_avg_arr10_compare_temp_rise = malloc(sizeof(sig_average_compare_t));
        memset(g_buildin_signal_avg_arr10_compare_temp_rise, 0, sizeof(sig_average_compare_t));
        g_buildin_signal_avg_arr10_compare_temp_rise->sig_average_temp_of_n = g_buildin_signal_average_temp_of_10;
        g_buildin_signal_avg_arr10_compare_temp_rise->average_compare_rules_arr_size = BUILDIN_AVG_ARR10_COMPARE_TEMP_RISE_RULE_SIZE;
        g_buildin_signal_avg_arr10_compare_temp_rise->type = SIG_AVERAGE_COMPARE;
        g_buildin_signal_avg_arr10_compare_temp_rise->rule_relation = AND;

        // compare_target为-30,是因为存在允许出现后面出现小的情况,但最后3个规则保底了要变大
        buildin_avg_arr10_compare_temp_rise_rules_arr[0].compare_type = COMPARE_GTE;
        // buildin_avg_arr10_compare_temp_rise_rules_arr[0].calculate_type = CALC_ABS;
        buildin_avg_arr10_compare_temp_rise_rules_arr[0].minuend_index = 9;
        buildin_avg_arr10_compare_temp_rise_rules_arr[0].subtrahend_index = 7;
        buildin_avg_arr10_compare_temp_rise_rules_arr[0].compare_target = -30;

        buildin_avg_arr10_compare_temp_rise_rules_arr[1].compare_type = COMPARE_GTE;
        // buildin_avg_arr10_compare_temp_rise_rules_arr[1].calculate_type = CALC_ABS;
        buildin_avg_arr10_compare_temp_rise_rules_arr[1].minuend_index = 8; 
        buildin_avg_arr10_compare_temp_rise_rules_arr[1].subtrahend_index = 6; 
        buildin_avg_arr10_compare_temp_rise_rules_arr[1].compare_target = -30;
        
        buildin_avg_arr10_compare_temp_rise_rules_arr[2].compare_type = COMPARE_GTE;
        // buildin_avg_arr10_compare_temp_rise_rules_arr[2].calculate_type = CALC_ABS;
        buildin_avg_arr10_compare_temp_rise_rules_arr[2].minuend_index = 7; 
        buildin_avg_arr10_compare_temp_rise_rules_arr[2].subtrahend_index = 5; 
        buildin_avg_arr10_compare_temp_rise_rules_arr[2].compare_target = -30;

        buildin_avg_arr10_compare_temp_rise_rules_arr[3].compare_type = COMPARE_GTE;
        // buildin_avg_arr10_compare_temp_rise_rules_arr[3].calculate_type = CALC_ABS;
        buildin_avg_arr10_compare_temp_rise_rules_arr[3].minuend_index = 6; 
        buildin_avg_arr10_compare_temp_rise_rules_arr[3].subtrahend_index = 4; 
        buildin_avg_arr10_compare_temp_rise_rules_arr[3].compare_target = -30;
        
        buildin_avg_arr10_compare_temp_rise_rules_arr[4].compare_type = COMPARE_GTE;
        // buildin_avg_arr10_compare_temp_rise_rules_arr[4].calculate_type = CALC_ABS;
        buildin_avg_arr10_compare_temp_rise_rules_arr[4].minuend_index = 5; 
        buildin_avg_arr10_compare_temp_rise_rules_arr[4].subtrahend_index = 3; 
        buildin_avg_arr10_compare_temp_rise_rules_arr[4].compare_target = -30;

        buildin_avg_arr10_compare_temp_rise_rules_arr[5].compare_type = COMPARE_GTE;
        // buildin_avg_arr10_compare_temp_rise_rules_arr[5].calculate_type = CALC_ABS;
        buildin_avg_arr10_compare_temp_rise_rules_arr[5].minuend_index = 4;
        buildin_avg_arr10_compare_temp_rise_rules_arr[5].subtrahend_index = 2;
        buildin_avg_arr10_compare_temp_rise_rules_arr[5].compare_target = -30;
        
        buildin_avg_arr10_compare_temp_rise_rules_arr[6].compare_type = COMPARE_GTE;
        // buildin_avg_arr10_compare_temp_rise_rules_arr[6].calculate_type = CALC_ABS;
        buildin_avg_arr10_compare_temp_rise_rules_arr[6].minuend_index = 3;
        buildin_avg_arr10_compare_temp_rise_rules_arr[6].subtrahend_index = 1;
        buildin_avg_arr10_compare_temp_rise_rules_arr[6].compare_target = -30;

        buildin_avg_arr10_compare_temp_rise_rules_arr[7].compare_type = COMPARE_GTE;
        // buildin_avg_arr10_compare_temp_rise_rules_arr[7].calculate_type = CALC_ABS;
        buildin_avg_arr10_compare_temp_rise_rules_arr[7].minuend_index = 2;
        buildin_avg_arr10_compare_temp_rise_rules_arr[7].subtrahend_index = 0;
        buildin_avg_arr10_compare_temp_rise_rules_arr[7].compare_target = -30;

        buildin_avg_arr10_compare_temp_rise_rules_arr[8].compare_type = COMPARE_GT;
        buildin_avg_arr10_compare_temp_rise_rules_arr[8].minuend_index = 9;
        buildin_avg_arr10_compare_temp_rise_rules_arr[8].subtrahend_index = 2;
        buildin_avg_arr10_compare_temp_rise_rules_arr[8].compare_target = 10;

        buildin_avg_arr10_compare_temp_rise_rules_arr[9].compare_type = COMPARE_GT;
        buildin_avg_arr10_compare_temp_rise_rules_arr[9].minuend_index = 9;
        buildin_avg_arr10_compare_temp_rise_rules_arr[9].subtrahend_index = 1;
        buildin_avg_arr10_compare_temp_rise_rules_arr[9].compare_target = 10;

        buildin_avg_arr10_compare_temp_rise_rules_arr[10].compare_type = COMPARE_GT; 
        buildin_avg_arr10_compare_temp_rise_rules_arr[10].minuend_index = 9;
        buildin_avg_arr10_compare_temp_rise_rules_arr[10].subtrahend_index = 0;
        buildin_avg_arr10_compare_temp_rise_rules_arr[10].compare_target = 20;

        g_buildin_signal_avg_arr10_compare_temp_rise->average_compare_rules_arr = buildin_avg_arr10_compare_temp_rise_rules_arr;
        
        // CA_ADD_SIGNAL(g_buildin_signal_avg_arr1_compare_temp_gental); 仅仅作为依赖信号的话，可以不用ADD
    }
}

static void buildin_add_signal_temp_tendency_rise()
{
    log_debug("%s", __func__);

    if (g_buildin_signal_temp_rise == NULL)
    {
        g_buildin_signal_temp_rise = malloc(sizeof(sig_temp_rise_tendency_t));
        memset(g_buildin_signal_temp_rise, 0, sizeof(sig_temp_rise_tendency_t));
        g_buildin_signal_temp_rise->sig_average_compare = g_buildin_signal_avg_arr10_compare_temp_rise;
        g_buildin_signal_temp_rise->type = SIG_TEMP_TENDENCY_RISE;
        g_buildin_signal_temp_rise->is_set_range = false;  // 不设置范围
        g_buildin_signal_temp_rise->rise_range_floor = 0; 
        g_buildin_signal_temp_rise->rise_range_ceiling = 0;
        g_buildin_signal_temp_rise->at_leaset_rise = -50;  // -50兼容一些异常的情况
        g_buildin_signal_temp_rise->at_most_rise = 50;
        g_buildin_signal_temp_rise->at_least_duration_ticks =10 * 4;

        CA_ADD_SIGNAL(g_buildin_signal_temp_rise);
        CA_SIG_CONNECT(g_buildin_signal_temp_rise, buildin_signal_temp_rise_cb);
    }
}
/* ************************* 上升趋势end　************************* */


/* ************************* start下降趋势　************************* */
static void buildin_add_signal_avg_arr4_compare_dependency_of_temp_down()
{
    log_debug("%s", __func__);
    if (g_buildin_signal_avg_arr4_compare_temp_down == NULL)
    {
        g_buildin_signal_avg_arr4_compare_temp_down = malloc(sizeof(sig_average_compare_t));
        memset(g_buildin_signal_avg_arr4_compare_temp_down, 0, sizeof(sig_average_compare_t));
        g_buildin_signal_avg_arr4_compare_temp_down->sig_average_temp_of_n = g_buildin_signal_average_temp_of_4;
        g_buildin_signal_avg_arr4_compare_temp_down->average_compare_rules_arr_size = BUILDIN_AVG_ARR4_COMPARE_TEMP_DOWN_RULE_SIZE;
        g_buildin_signal_avg_arr4_compare_temp_down->type = SIG_AVERAGE_COMPARE;
        g_buildin_signal_avg_arr4_compare_temp_down->rule_relation = AND;

        buildin_avg_arr4_compare_temp_down_rules_arr[0].compare_type = COMPARE_LTE;
        buildin_avg_arr4_compare_temp_down_rules_arr[0].minuend_index = 9;
        buildin_avg_arr4_compare_temp_down_rules_arr[0].subtrahend_index = 7;
        buildin_avg_arr4_compare_temp_down_rules_arr[0].compare_target = 30;

        buildin_avg_arr4_compare_temp_down_rules_arr[1].compare_type = COMPARE_LTE;
        buildin_avg_arr4_compare_temp_down_rules_arr[1].minuend_index = 8; 
        buildin_avg_arr4_compare_temp_down_rules_arr[1].subtrahend_index = 6; 
        buildin_avg_arr4_compare_temp_down_rules_arr[1].compare_target = 30;
        
        buildin_avg_arr4_compare_temp_down_rules_arr[2].compare_type = COMPARE_LTE;
        buildin_avg_arr4_compare_temp_down_rules_arr[2].minuend_index = 7; 
        buildin_avg_arr4_compare_temp_down_rules_arr[2].subtrahend_index = 5; 
        buildin_avg_arr4_compare_temp_down_rules_arr[2].compare_target = 30;

        buildin_avg_arr4_compare_temp_down_rules_arr[3].compare_type = COMPARE_LTE;
        buildin_avg_arr4_compare_temp_down_rules_arr[3].minuend_index = 6; 
        buildin_avg_arr4_compare_temp_down_rules_arr[3].subtrahend_index = 4; 
        buildin_avg_arr4_compare_temp_down_rules_arr[3].compare_target = 30;
        
        buildin_avg_arr4_compare_temp_down_rules_arr[4].compare_type = COMPARE_LTE;
        buildin_avg_arr4_compare_temp_down_rules_arr[4].minuend_index = 5; 
        buildin_avg_arr4_compare_temp_down_rules_arr[4].subtrahend_index = 3; 
        buildin_avg_arr4_compare_temp_down_rules_arr[4].compare_target = 30;

        buildin_avg_arr4_compare_temp_down_rules_arr[5].compare_type = COMPARE_LTE;
        buildin_avg_arr4_compare_temp_down_rules_arr[5].minuend_index = 4;
        buildin_avg_arr4_compare_temp_down_rules_arr[5].subtrahend_index = 2;
        buildin_avg_arr4_compare_temp_down_rules_arr[5].compare_target = 30;

        buildin_avg_arr4_compare_temp_down_rules_arr[6].compare_type = COMPARE_LTE;
        buildin_avg_arr4_compare_temp_down_rules_arr[6].minuend_index = 3;
        buildin_avg_arr4_compare_temp_down_rules_arr[6].subtrahend_index = 1;
        buildin_avg_arr4_compare_temp_down_rules_arr[6].compare_target = 30;

        buildin_avg_arr4_compare_temp_down_rules_arr[7].compare_type = COMPARE_LTE;
        buildin_avg_arr4_compare_temp_down_rules_arr[7].minuend_index = 2;
        buildin_avg_arr4_compare_temp_down_rules_arr[7].subtrahend_index = 0;
        buildin_avg_arr4_compare_temp_down_rules_arr[7].compare_target = 30;

        buildin_avg_arr4_compare_temp_down_rules_arr[8].compare_type = COMPARE_LT;
        buildin_avg_arr4_compare_temp_down_rules_arr[8].minuend_index = 9;
        buildin_avg_arr4_compare_temp_down_rules_arr[8].subtrahend_index = 2;
        buildin_avg_arr4_compare_temp_down_rules_arr[8].compare_target = -10;

        buildin_avg_arr4_compare_temp_down_rules_arr[9].compare_type = COMPARE_LT;
        buildin_avg_arr4_compare_temp_down_rules_arr[9].minuend_index = 9;
        buildin_avg_arr4_compare_temp_down_rules_arr[9].subtrahend_index = 1;
        buildin_avg_arr4_compare_temp_down_rules_arr[9].compare_target = -10;
        
        buildin_avg_arr4_compare_temp_down_rules_arr[10].compare_type = COMPARE_LT;
        buildin_avg_arr4_compare_temp_down_rules_arr[10].minuend_index = 9;
        buildin_avg_arr4_compare_temp_down_rules_arr[10].subtrahend_index = 0;
        buildin_avg_arr4_compare_temp_down_rules_arr[10].compare_target = -20;

        g_buildin_signal_avg_arr4_compare_temp_down->average_compare_rules_arr = buildin_avg_arr4_compare_temp_down_rules_arr;

        // CA_ADD_SIGNAL(g_buildin_signal_avg_arr1_compare_temp_gental); 仅仅作为依赖信号的话，可以不用ADD 但需要执行一下这个函数，初始化变量
    }
}

static void buildin_add_signal_temp_tendency_down()
{
    log_debug("%s", __func__);

    if (g_buildin_signal_temp_down == NULL)
    {
        g_buildin_signal_temp_down = malloc(sizeof(sig_temp_down_tendency_t));
        memset(g_buildin_signal_temp_down, 0, sizeof(sig_temp_down_tendency_t));
        g_buildin_signal_temp_down->sig_average_compare = g_buildin_signal_avg_arr4_compare_temp_down;
        g_buildin_signal_temp_down->type = SIG_TEMP_TENDENCY_DOWN;
        g_buildin_signal_temp_down->is_set_range = false;  // 不设置范围
        g_buildin_signal_temp_down->down_range_floor = 0; 
        g_buildin_signal_temp_down->down_range_ceiling = 0;
        g_buildin_signal_temp_down->at_leaset_down = 30;
        g_buildin_signal_temp_down->at_most_down = 50;
        g_buildin_signal_temp_down->at_least_duration_ticks = 10 * 4;

        CA_ADD_SIGNAL(g_buildin_signal_temp_down);
        CA_SIG_CONNECT(g_buildin_signal_temp_down, buildin_signal_temp_down_cb);
    }
}
/* ************************* 下降趋势end　************************* */

/* ************************* start跳升现象　************************* */
static void buildin_add_signal_avg_arr1_compare_dependency_of_temp_jump_rise()
{
    log_debug("%s", __func__);
    if (g_buildin_signal_avg_arr1_compare_temp_jump_rise == NULL)
    {
        g_buildin_signal_avg_arr1_compare_temp_jump_rise = malloc(sizeof(sig_average_compare_t));
        memset(g_buildin_signal_avg_arr1_compare_temp_jump_rise, 0, sizeof(sig_average_compare_t));
        g_buildin_signal_avg_arr1_compare_temp_jump_rise->sig_average_temp_of_n = g_buildin_signal_average_temp_of_1;
        g_buildin_signal_avg_arr1_compare_temp_jump_rise->average_compare_rules_arr_size = BUILDIN_AVG_ARR1_COMPARE_TEMP_JUMP_RISE_RULE_SIZE;
        g_buildin_signal_avg_arr1_compare_temp_jump_rise->type = SIG_AVERAGE_COMPARE;
        g_buildin_signal_avg_arr1_compare_temp_jump_rise->rule_relation = OR;

        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[0].compare_type = COMPARE_GT;
        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[0].minuend_index = 19;
        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[0].subtrahend_index = 18;
        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[0].compare_target = 0;

        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[1].compare_type = COMPARE_GT;
        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[1].minuend_index = 19; 
        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[1].subtrahend_index = 17; 
        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[1].compare_target = 0;
        
        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[2].compare_type = COMPARE_GT;
        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[2].minuend_index = 19; 
        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[2].subtrahend_index = 16; 
        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[2].compare_target = 0;

        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[3].compare_type = COMPARE_GT;
        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[3].minuend_index = 19;
        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[3].subtrahend_index = 15;
        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[3].compare_target = 0;

        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[4].compare_type = COMPARE_GT;
        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[4].minuend_index = 19;
        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[4].subtrahend_index = 14;
        buildin_avg_arr1_compare_temp_jump_rise_rules_arr[4].compare_target = 0;

        g_buildin_signal_avg_arr1_compare_temp_jump_rise->average_compare_rules_arr = buildin_avg_arr1_compare_temp_jump_rise_rules_arr;

        // CA_ADD_SIGNAL(g_buildin_signal_avg_arr1_compare_temp_gental); 仅仅作为依赖信号的话，可以不用ADD 但需要执行一下这个函数，初始化变量
    }
}

static void buildin_add_signal_temp_jump_rise()
{
    log_debug("%s", __func__);

    if (g_buildin_signal_temp_jump_rise == NULL)
    {
        g_buildin_signal_temp_jump_rise = malloc(sizeof(sig_temp_jump_rise_tendency_t));
        memset(g_buildin_signal_temp_jump_rise, 0, sizeof(sig_temp_jump_rise_tendency_t));
        g_buildin_signal_temp_jump_rise->sig_average_compare = g_buildin_signal_avg_arr1_compare_temp_jump_rise;
        g_buildin_signal_temp_jump_rise->type = SIG_TEMP_TENDENCY_JUMP_RISE;
        g_buildin_signal_temp_jump_rise->is_set_range = false;  // 不设置范围
        g_buildin_signal_temp_jump_rise->jump_rise_range_floor = 0; 
        g_buildin_signal_temp_jump_rise->jump_rise_range_ceiling = 0;
        g_buildin_signal_temp_jump_rise->at_leaset_jump_rise = 40;
        g_buildin_signal_temp_jump_rise->at_most_jump_rise = 3000;

        CA_ADD_SIGNAL(g_buildin_signal_temp_jump_rise);
        CA_SIG_CONNECT(g_buildin_signal_temp_jump_rise, buildin_signal_temp_jump_rise_cb);
    }
}
/* ************************* 跳升现象end　************************* */


/* ************************* start跳降现象　************************* */
static void buildin_add_signal_avg_arr1_compare_dependency_of_temp_jump_down()
{
    log_debug("%s", __func__);
    if (g_buildin_signal_avg_arr1_compare_temp_jump_down == NULL)
    {
        g_buildin_signal_avg_arr1_compare_temp_jump_down = malloc(sizeof(sig_average_compare_t));
        memset(g_buildin_signal_avg_arr1_compare_temp_jump_down, 0, sizeof(sig_average_compare_t));
        g_buildin_signal_avg_arr1_compare_temp_jump_down->sig_average_temp_of_n = g_buildin_signal_average_temp_of_1;
        g_buildin_signal_avg_arr1_compare_temp_jump_down->average_compare_rules_arr_size = BUILDIN_AVG_ARR1_COMPARE_TEMP_JUMP_DOWN_RULE_SIZE;
        g_buildin_signal_avg_arr1_compare_temp_jump_down->type = SIG_AVERAGE_COMPARE;
        g_buildin_signal_avg_arr1_compare_temp_jump_down->rule_relation = OR;

        buildin_avg_arr1_compare_temp_jump_down_rules_arr[0].compare_type = COMPARE_LT;
        buildin_avg_arr1_compare_temp_jump_down_rules_arr[0].minuend_index = 19;
        buildin_avg_arr1_compare_temp_jump_down_rules_arr[0].subtrahend_index = 18;
        buildin_avg_arr1_compare_temp_jump_down_rules_arr[0].compare_target = 0;

        buildin_avg_arr1_compare_temp_jump_down_rules_arr[1].compare_type = COMPARE_LT;
        buildin_avg_arr1_compare_temp_jump_down_rules_arr[1].minuend_index = 19; 
        buildin_avg_arr1_compare_temp_jump_down_rules_arr[1].subtrahend_index = 17; 
        buildin_avg_arr1_compare_temp_jump_down_rules_arr[1].compare_target = 0;
        
        buildin_avg_arr1_compare_temp_jump_down_rules_arr[2].compare_type = COMPARE_LT;
        buildin_avg_arr1_compare_temp_jump_down_rules_arr[2].minuend_index = 19; 
        buildin_avg_arr1_compare_temp_jump_down_rules_arr[2].subtrahend_index = 16; 
        buildin_avg_arr1_compare_temp_jump_down_rules_arr[2].compare_target = 0;

        buildin_avg_arr1_compare_temp_jump_down_rules_arr[3].compare_type = COMPARE_LT;
        buildin_avg_arr1_compare_temp_jump_down_rules_arr[3].minuend_index = 19;
        buildin_avg_arr1_compare_temp_jump_down_rules_arr[3].subtrahend_index = 15;
        buildin_avg_arr1_compare_temp_jump_down_rules_arr[3].compare_target = 0;

        buildin_avg_arr1_compare_temp_jump_down_rules_arr[4].compare_type = COMPARE_LT;
        buildin_avg_arr1_compare_temp_jump_down_rules_arr[4].minuend_index = 19;
        buildin_avg_arr1_compare_temp_jump_down_rules_arr[4].subtrahend_index = 14;
        buildin_avg_arr1_compare_temp_jump_down_rules_arr[4].compare_target = 0;

        g_buildin_signal_avg_arr1_compare_temp_jump_down->average_compare_rules_arr = buildin_avg_arr1_compare_temp_jump_down_rules_arr;

        // CA_ADD_SIGNAL(g_buildin_signal_avg_arr1_compare_temp_gental); 仅仅作为依赖信号的话，可以不用ADD 但需要执行一下这个函数，初始化变量
    }
}

static void buildin_add_signal_temp_jump_down()
{
    log_debug("%s", __func__);

    if (g_buildin_signal_temp_jump_down == NULL)
    {
        g_buildin_signal_temp_jump_down = malloc(sizeof(sig_temp_jump_down_tendency_t));
        memset(g_buildin_signal_temp_jump_down, 0, sizeof(sig_temp_jump_down_tendency_t));
        g_buildin_signal_temp_jump_down->sig_average_compare = g_buildin_signal_avg_arr1_compare_temp_jump_down;
        g_buildin_signal_temp_jump_down->type = SIG_TEMP_TENDENCY_JUMP_DOWN;
        g_buildin_signal_temp_jump_down->is_set_range = false;  // 不设置范围
        g_buildin_signal_temp_jump_down->jump_down_range_floor = 0; 
        g_buildin_signal_temp_jump_down->jump_down_range_ceiling = 0;
        g_buildin_signal_temp_jump_down->at_leaset_jump_down = 40;
        g_buildin_signal_temp_jump_down->at_most_jump_down = 3000;

        CA_ADD_SIGNAL(g_buildin_signal_temp_jump_down);
        CA_SIG_CONNECT(g_buildin_signal_temp_jump_down, buildin_signal_temp_jump_down_cb);
    }
}
/* ************************* 跳降现象end　************************* */


/* ************************* start大幅波动现象　************************* */
// static void buildin_add_signal_temp_shake()
// {
//     log_debug("%s", __func__);

//     if (g_buildin_signal_temp_shake == NULL)
//     {
//         g_buildin_signal_temp_shake = malloc(sizeof(sig_temp_shake_tendency_t));
//         memset(g_buildin_signal_temp_shake, 0, sizeof(sig_temp_shake_tendency_t));
//         g_buildin_signal_temp_shake->type = SIG_TEMP_TENDENCY_SHAKE;
//         g_buildin_signal_temp_shake->at_least_duration_ticks = 30 * 4;
//         g_buildin_signal_temp_shake->at_least_shake_times_in_duration_ticks = 3;
//         g_buildin_signal_temp_shake->at_most_jump_gap_ticks = 15 * 4;  // 上升下降之间最多间隔Ｎ秒

//         CA_ADD_SIGNAL(g_buildin_signal_temp_shake);
//         CA_SIG_CONNECT(g_buildin_signal_temp_shake, buildin_signal_temp_shake_cb);
//     }
// }
static void buildin_add_signal_avg_arr1_compare_dependency_of_temp_shake()
{
    log_debug("%s", __func__);
    if (g_buildin_signal_avg_arr1_compare_temp_shake == NULL)
    {
        g_buildin_signal_avg_arr1_compare_temp_shake = malloc(sizeof(sig_average_compare_t));
        memset(g_buildin_signal_avg_arr1_compare_temp_shake, 0, sizeof(sig_average_compare_t));
        g_buildin_signal_avg_arr1_compare_temp_shake->sig_average_temp_of_n = g_buildin_signal_average_temp_of_1;
        g_buildin_signal_avg_arr1_compare_temp_shake->average_compare_rules_arr_size = BUILDIN_AVG_ARR1_COMPARE_TEMP_SHAKE_RULE_SIZE;
        g_buildin_signal_avg_arr1_compare_temp_shake->type = SIG_AVERAGE_COMPARE;
        g_buildin_signal_avg_arr1_compare_temp_shake->rule_relation = OR;

        buildin_avg_arr1_compare_temp_shake_rules_arr[0].compare_type = COMPARE_GT;
        buildin_avg_arr1_compare_temp_shake_rules_arr[0].calculate_type = CALC_ABS;
        buildin_avg_arr1_compare_temp_shake_rules_arr[0].minuend_index = 10;
        buildin_avg_arr1_compare_temp_shake_rules_arr[0].subtrahend_index = 9;
        buildin_avg_arr1_compare_temp_shake_rules_arr[0].compare_target = 30;

        buildin_avg_arr1_compare_temp_shake_rules_arr[1].compare_type = COMPARE_GT;
        buildin_avg_arr1_compare_temp_shake_rules_arr[1].calculate_type = CALC_ABS;
        buildin_avg_arr1_compare_temp_shake_rules_arr[1].minuend_index = 10; 
        buildin_avg_arr1_compare_temp_shake_rules_arr[1].subtrahend_index = 8; 
        buildin_avg_arr1_compare_temp_shake_rules_arr[1].compare_target = 30;

        buildin_avg_arr1_compare_temp_shake_rules_arr[2].compare_type = COMPARE_GT;
        buildin_avg_arr1_compare_temp_shake_rules_arr[2].calculate_type = CALC_ABS;
        buildin_avg_arr1_compare_temp_shake_rules_arr[2].minuend_index = 10; 
        buildin_avg_arr1_compare_temp_shake_rules_arr[2].subtrahend_index = 7; 
        buildin_avg_arr1_compare_temp_shake_rules_arr[2].compare_target = 30;

        buildin_avg_arr1_compare_temp_shake_rules_arr[3].compare_type = COMPARE_GT;
        buildin_avg_arr1_compare_temp_shake_rules_arr[3].calculate_type = CALC_ABS;
        buildin_avg_arr1_compare_temp_shake_rules_arr[3].minuend_index = 10; 
        buildin_avg_arr1_compare_temp_shake_rules_arr[3].subtrahend_index = 6; 
        buildin_avg_arr1_compare_temp_shake_rules_arr[3].compare_target = 30;

        buildin_avg_arr1_compare_temp_shake_rules_arr[4].compare_type = COMPARE_GT;
        buildin_avg_arr1_compare_temp_shake_rules_arr[4].calculate_type = CALC_ABS;
        buildin_avg_arr1_compare_temp_shake_rules_arr[4].minuend_index = 10; 
        buildin_avg_arr1_compare_temp_shake_rules_arr[4].subtrahend_index = 5; 
        buildin_avg_arr1_compare_temp_shake_rules_arr[4].compare_target = 30;

        buildin_avg_arr1_compare_temp_shake_rules_arr[5].compare_type = COMPARE_GT;
        buildin_avg_arr1_compare_temp_shake_rules_arr[5].calculate_type = CALC_ABS;
        buildin_avg_arr1_compare_temp_shake_rules_arr[5].minuend_index = 10; 
        buildin_avg_arr1_compare_temp_shake_rules_arr[5].subtrahend_index = 11; 
        buildin_avg_arr1_compare_temp_shake_rules_arr[5].compare_target = 30;
        
        buildin_avg_arr1_compare_temp_shake_rules_arr[6].compare_type = COMPARE_GT;
        buildin_avg_arr1_compare_temp_shake_rules_arr[6].calculate_type = CALC_ABS;
        buildin_avg_arr1_compare_temp_shake_rules_arr[6].minuend_index = 10; 
        buildin_avg_arr1_compare_temp_shake_rules_arr[6].subtrahend_index = 12; 
        buildin_avg_arr1_compare_temp_shake_rules_arr[6].compare_target = 30;

        buildin_avg_arr1_compare_temp_shake_rules_arr[7].compare_type = COMPARE_GT;
        buildin_avg_arr1_compare_temp_shake_rules_arr[7].calculate_type = CALC_ABS;
        buildin_avg_arr1_compare_temp_shake_rules_arr[7].minuend_index = 10; 
        buildin_avg_arr1_compare_temp_shake_rules_arr[7].subtrahend_index = 13; 
        buildin_avg_arr1_compare_temp_shake_rules_arr[7].compare_target = 30;

        buildin_avg_arr1_compare_temp_shake_rules_arr[8].compare_type = COMPARE_GT;
        buildin_avg_arr1_compare_temp_shake_rules_arr[8].calculate_type = CALC_ABS;
        buildin_avg_arr1_compare_temp_shake_rules_arr[8].minuend_index = 10; 
        buildin_avg_arr1_compare_temp_shake_rules_arr[8].subtrahend_index = 14; 
        buildin_avg_arr1_compare_temp_shake_rules_arr[8].compare_target = 30;

        buildin_avg_arr1_compare_temp_shake_rules_arr[9].compare_type = COMPARE_GT;
        buildin_avg_arr1_compare_temp_shake_rules_arr[9].calculate_type = CALC_ABS;
        buildin_avg_arr1_compare_temp_shake_rules_arr[9].minuend_index = 10; 
        buildin_avg_arr1_compare_temp_shake_rules_arr[9].subtrahend_index = 15; 
        buildin_avg_arr1_compare_temp_shake_rules_arr[9].compare_target = 30;

        g_buildin_signal_avg_arr1_compare_temp_shake->average_compare_rules_arr = buildin_avg_arr1_compare_temp_shake_rules_arr;

        // CA_ADD_SIGNAL(g_buildin_signal_avg_arr1_compare_temp_gental); 仅仅作为依赖信号的话，可以不用ADD 但需要执行一下这个函数，初始化变量
    }
}

static void buildin_add_signal_temp_shake()
{
    log_debug("%s", __func__);

    if (g_buildin_signal_temp_shake == NULL)
    {
        g_buildin_signal_temp_shake = malloc(sizeof(sig_temp_shake_tendency_t));
        memset(g_buildin_signal_temp_shake, 0, sizeof(sig_temp_shake_tendency_t));
        g_buildin_signal_temp_shake->sig_average_compare = g_buildin_signal_avg_arr1_compare_temp_shake;
        g_buildin_signal_temp_shake->type = SIG_TEMP_TENDENCY_SHAKE;
        g_buildin_signal_temp_shake->is_set_range = false;  // 不设置范围
        g_buildin_signal_temp_shake->jump_down_range_floor = 0; 
        g_buildin_signal_temp_shake->jump_down_range_ceiling = 0;
        g_buildin_signal_temp_shake->at_least_duration_ticks = 10 * 4;
        g_buildin_signal_temp_shake->at_least_temp_direction_change_times = 5;
        g_buildin_signal_temp_shake->at_least_diff_of_max_min = 50;

        CA_ADD_SIGNAL(g_buildin_signal_temp_shake);
        CA_SIG_CONNECT(g_buildin_signal_temp_shake, buildin_signal_temp_shake_cb);
    }
}
/* ************************* 大幅波动现象end　************************* */


/* ************************* start上升越来越快　************************* */
static void buildin_add_signal_temp_rise_ff_avg_arr4()
{
    log_debug("%s", __func__);

    if (g_buildin_signal_temp_rise_ff == NULL)
    {
        g_buildin_signal_temp_rise_ff = malloc(sizeof(sig_rise_faster_and_faster_t));
        memset(g_buildin_signal_temp_rise_ff, 0, sizeof(sig_rise_faster_and_faster_t));
        g_buildin_signal_temp_rise_ff->sig_average_temp_of_n = g_buildin_signal_average_temp_of_4;  // 用每４个计算平均数数组来判断
        g_buildin_signal_temp_rise_ff->type = SIG_TEMP_TENDENCY_RISE_FF;
        g_buildin_signal_temp_rise_ff->is_set_range = false;  // 不设置范围
        g_buildin_signal_temp_rise_ff->faster_and_faster_range_floor = 0; 
        g_buildin_signal_temp_rise_ff->faster_and_faster_range_ceiling = 0;
        g_buildin_signal_temp_rise_ff->rise_step = 30;
        g_buildin_signal_temp_rise_ff->at_most_single_step_cost = 30 * 4;  // 前面的平稳at_least_duration_ticks是这个值
        g_buildin_signal_temp_rise_ff->cost_compate_times = 3;

        CA_ADD_SIGNAL(g_buildin_signal_temp_rise_ff);
        CA_SIG_CONNECT(g_buildin_signal_temp_rise_ff, buildin_signal_temp_rise_ff_cb);
    }
}
/* ************************* 上升越来越快end　************************* */


static void buildin_add_signals()
{
    log_debug("%s", __func__);
    // 注意定义添加信号时，必须一个一个函数来，否则宏展开会报错（目前设计是这样的，因为如果连续在一个函数里面展开，宏里面定义节点变量会出现重复定义。而且模块化信号利于调试维护）

    buildin_add_signal_avg_arr1();
    buildin_add_signal_avg_arr1_compare_dependency_of_temp_gental();  // 存在信号依赖关系时，一定要在被依赖的后面添加
    buildin_add_signal_temp_tendency_gental();

    buildin_add_signal_avg_arr1_compare_dependency_of_temp_jump_rise();
    buildin_add_signal_temp_jump_rise();

    buildin_add_signal_avg_arr1_compare_dependency_of_temp_jump_down();
    buildin_add_signal_temp_jump_down();

    buildin_add_signal_avg_arr1_compare_dependency_of_temp_shake();
    buildin_add_signal_temp_shake();


    buildin_add_signal_avg_arr4();  // 注意：信号注册完了，需要到这里来调用相关函数，不然没效果
    // buildin_add_signal_avg_arr4_compare_dependency_of_temp_rise();
    // buildin_add_signal_temp_tendency_rise();

    buildin_add_signal_avg_arr4_compare_dependency_of_temp_down();
    buildin_add_signal_temp_tendency_down();

    buildin_add_signal_temp_rise_ff_avg_arr4();


    buildin_add_signal_avg_arr10();
    buildin_add_signal_avg_arr4_compare_dependency_of_temp_rise();
    buildin_add_signal_temp_tendency_rise();
}

//初始化信号链表的函数入口
void buildin_app()
{
    if (!have_add_signals)
    {
        buildin_add_signals();
        have_add_signals = true;
    }
    
}