#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "ca_signals_lib.h"
// #include "ring_buffer.h"
#include "ca_apps.h"

#define log_module_name "cook_assistant"
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)
#define log_info(...)  LOGI(log_module_name, ##__VA_ARGS__)
#define log_warn(...)  LOGW(log_module_name, ##__VA_ARGS__)
#define log_error(...) LOGE(log_module_name, ##__VA_ARGS__)
#define log_fatal(...) LOGF(log_module_name, ##__VA_ARGS__)

/* *************************全局变量************************* */
ca_signal_list_t *g_signal_list_head = NULL;  // 存放所有信号的链表头节点
signal_cb_list_t *g_signal_cb_list_head = NULL;  // 存放所有信号与回调函数对应关系的链表头节点
user_signals_worker_list_t *g_user_signals_worker_head = NULL;  // 存放业务模块中信号分发函数
temp_tendency_info_t *g_buildin_temp_gental_tendency_head = NULL;  // 内置的　存放所有平稳温度趋势段信息的链表头节点
unsigned short g_temp_gental_tendency_node_num = 0;
temp_tendency_info_t *g_buildin_temp_gental_tendency_tail = NULL;  // 内置的　存放所有平稳温度趋势段信息的链表尾节点
temp_tendency_info_t *g_buildin_temp_rise_tendency_head = NULL;
unsigned short g_temp_rise_tendency_node_num = 0;
temp_tendency_info_t *g_buildin_temp_rise_tendency_tail = NULL;
temp_tendency_info_t *g_buildin_temp_down_tendency_head = NULL;
unsigned short g_temp_down_tendency_node_num = 0;
temp_tendency_info_t *g_buildin_temp_down_tendency_tail = NULL;
temp_tendency_info_t *g_buildin_temp_jump_rise_tendency_head = NULL;
unsigned short g_temp_jump_rise_tendency_node_num = 0;
temp_tendency_info_t *g_buildin_temp_jump_rise_tendency_tail = NULL;
temp_tendency_info_t *g_buildin_temp_jump_down_tendency_head = NULL;
unsigned short g_temp_jump_down_tendency_node_num = 0;
temp_tendency_info_t *g_buildin_temp_jump_down_tendency_tail = NULL;
temp_tendency_info_t *g_buildin_temp_shake_tendency_head = NULL;
unsigned short g_temp_shake_tendency_node_num = 0;
temp_tendency_info_t *g_buildin_temp_shake_tendency_tail = NULL;
temp_tendency_info_t *g_buildin_temp_rise_ff_tendency_head = NULL;
unsigned short g_temp_rise_ff_tendency_node_num = 0;
temp_tendency_info_t *g_buildin_temp_rise_ff_tendency_tail = NULL;
future_cb_list_t *g_future_cb_list_head = NULL;
static unsigned short rise_ff_last_reach_step_avg_temp = 0;  // 上升越来越快判断过程中记录上次上升一个step时的平均温度，首次是平均值数组第一个
static unsigned int rise_ff_last_reach_step_tick;  // 上升越来越快判断过程中记录上次上升一个step时的tick


unsigned short g_first_tick_temp;  // 开灶且进入一件烹饪模式后的第一个温度
unsigned short g_last_tick_temp;   // 最新的一个温度
unsigned int g_ca_tick;  // 目前进来的温度个数，每进来一个温度就+1
/* *************************全局变量************************* */

// 温度区间  g_temp_cells占了18.4k内存，注释掉，本来也没用到
// temp_cell_t g_temp_cells[TEMP_CELL_SIZE];
// unsigned int g_temp_cell_valid_size = 0;

// 温度链表
// temp_list_t *g_temp_list_head;
// temp_list_t *g_temp_list_tail;
// unsigned int g_temp_list_valid_siez = 0;

static bool load_signals_flag = false;
static bool load_connections_flag = false;


// 重置所有信息  只有关灶才触发这个
void reset_signal_module()
{
    log_debug("%s", __func__);
    int i;
    g_first_tick_temp = 0;
    g_ca_tick = 0;
    g_last_tick_temp = 0;
    // g_temp_list_valid_siez = 0;

    load_signals_flag = false;
    load_connections_flag = false;

    // 温度区间相关信息重置
    // for (i = 0; i < TEMP_CELL_SIZE; i++)
    // {
    //     g_temp_cells[i] = (temp_cell_t){0};
    // }

    // g_user_signals_worker_head
    user_signals_worker_list_t *uswl_p = g_user_signals_worker_head;
    while (uswl_p != NULL)
    {
        g_user_signals_worker_head = uswl_p->next;
        free(uswl_p);
        uswl_p = g_user_signals_worker_head;
    }

    // 所有信号链表
    ca_signal_list_t *sl_p = g_signal_list_head;
    while (sl_p != NULL)
    {
        g_signal_list_head = sl_p->next;
        free(sl_p);
        sl_p = g_signal_list_head;
    }

    // 所有信号回调对应关系链表
    signal_cb_list_t *scl_p = g_signal_cb_list_head;
    while (scl_p != NULL)
    {
        g_signal_cb_list_head = scl_p->next;
        free(scl_p);
        scl_p = g_signal_cb_list_head;
    }

    future_cb_list_t *fcl_p;
    fcl_p = g_future_cb_list_head;
    while (fcl_p != NULL)
    {
        g_future_cb_list_head = fcl_p->next;
        free(fcl_p->cb_param);  // 先释放参数指针
        free(fcl_p);
        fcl_p = g_future_cb_list_head;
    }

    temp_tendency_info_t *tti_p;
    tti_p = g_buildin_temp_gental_tendency_head;
    while (tti_p != NULL)
    {
        g_buildin_temp_gental_tendency_head = tti_p->next;
        free(tti_p);
        tti_p = g_buildin_temp_gental_tendency_head;
    }
    g_buildin_temp_gental_tendency_tail = NULL;
    g_temp_gental_tendency_node_num = 0;

    tti_p = g_buildin_temp_rise_tendency_head;
    while (tti_p != NULL)
    {
        g_buildin_temp_rise_tendency_head = tti_p->next;
        free(tti_p);
        tti_p = g_buildin_temp_rise_tendency_head;
    }
    g_buildin_temp_rise_tendency_tail = NULL;
    g_temp_rise_tendency_node_num = 0;

    tti_p = g_buildin_temp_down_tendency_head;
    while (tti_p != NULL)
    {
        g_buildin_temp_down_tendency_head = tti_p->next;
        free(tti_p);
        tti_p = g_buildin_temp_down_tendency_head;
    }
    g_buildin_temp_down_tendency_tail = NULL;
    g_temp_down_tendency_node_num = 0;

    tti_p = g_buildin_temp_jump_rise_tendency_head;
    while (tti_p != NULL)
    {
        g_buildin_temp_jump_rise_tendency_head = tti_p->next;
        free(tti_p);
        tti_p = g_buildin_temp_jump_rise_tendency_head;
    }
    g_buildin_temp_jump_rise_tendency_tail = NULL;
    g_temp_jump_rise_tendency_node_num = 0;

    tti_p = g_buildin_temp_jump_down_tendency_head;
    while (tti_p != NULL)
    {
        g_buildin_temp_jump_down_tendency_head = tti_p->next;
        free(tti_p);
        tti_p = g_buildin_temp_jump_down_tendency_head;
    }
    g_buildin_temp_jump_down_tendency_tail = NULL;
    g_temp_jump_down_tendency_node_num = 0;

    tti_p = g_buildin_temp_shake_tendency_head;
    while (tti_p != NULL)
    {
        g_buildin_temp_shake_tendency_head = tti_p->next;
        free(tti_p);
        tti_p = g_buildin_temp_shake_tendency_head;
    }
    g_buildin_temp_shake_tendency_tail = NULL;
    g_temp_shake_tendency_node_num =0;

    tti_p = g_buildin_temp_rise_ff_tendency_head;
    while (tti_p != NULL)
    {
        g_buildin_temp_rise_ff_tendency_head = tti_p->next;
        free(tti_p);
        tti_p = g_buildin_temp_rise_ff_tendency_head;
    }
    g_buildin_temp_rise_ff_tendency_tail = NULL;
    g_temp_rise_ff_tendency_node_num = 0;

    reset_buildin_signals();

    extern app_reset_func reset_app_funcs[RESET_APP_FUNC_SIZE];
    // reset 各个app
    for (i=0; i<RESET_APP_FUNC_SIZE; i++)
        reset_app_funcs[i]();

/*
    // 销毁所有温度节点 应该要在 g_temp_cells 后面
    while (g_temp_list_head != NULL)
    {
        if (g_temp_list_head->next == NULL)
        {
            free(g_temp_list_head->prev);
            g_temp_list_head->prev = NULL;
            break;
        }
        else
        {
            g_temp_list_head = g_temp_list_head->next;
            free(g_temp_list_head->prev);
            g_temp_list_head->prev = NULL;
        }
    }
*/
/*
    // 销毁所有注册的状态信号
    signal_temp_state_t *sts_next;
    while (g_signal_temp_state_head != NULL)
    {
        if (g_signal_temp_state_head->next != NULL)
        {
            sts_next = g_signal_temp_state_head;
            free(g_signal_temp_state_head);
            g_signal_temp_state_head = sts_next;
        }
        free(g_signal_temp_state_head);
        g_signal_temp_state_head = NULL;
        break;
    }
*/
}

/*
static init_temp_cells()
{
    log_debug("%s", __func__);
    if (g_temp_cell_valid_size == TEMP_CELL_SIZE)
        return;

    int i, j;
    for (i = 0; i < TEMP_CELL_SIZE; i++)   
    {
        log_debug("计算start %d  end  %d", i * TEMP_INTERVAL, (i + 1) * TEMP_INTERVAL - 1);
        g_temp_cells[i].temp_start = i * TEMP_INTERVAL;
        g_temp_cells[i].temp_end = (i + 1) * TEMP_INTERVAL - 1;
        g_temp_cell_valid_size++;

        for (j = 0; j < TEMP_INTERVAL / 10; j ++)
        {
            g_temp_cells[i].temp_num_dict[j].temp = g_temp_cells[i].temp_start + j * 10;  // 温度放大了10倍
            g_temp_cells[i].temp_num_dict[j].num = 0;
        }
    }
}
*/

/*
// 创建新的温度区间节点
static int create_new_temp_list_node(temp_cell_t *cell,  unsigned short t)
{
    log_debug("%s", __func__);
    temp_list_t *node = malloc(sizeof(temp_list_t));
    if (node == NULL)
    {
        log_debug("新建temp_list_node失败") ;
        return 1;
    }
    memset(node, 0, sizeof(temp_list_t));
    cell->temp_list_size++;
    node->belong_cell = cell;
    node->first_in_temp = t;
    node->first_in_tick = g_ca_tick;
    node->last_in_temp = t;
    node->last_in_tick = g_ca_tick;
    node->ticks = 1;

    if (g_temp_list_head == NULL)
        g_temp_list_head = node;
    if (g_temp_list_tail == NULL)
        g_temp_list_tail = node;
    else
    {
        g_temp_list_tail->next = node;
        node->next = NULL;
        node->prev = g_temp_list_tail;
        g_temp_list_tail = node;
    }

    if (cell->cell_temp_list_tail == NULL)
    {
        cell->cell_temp_list_tail = node;
        node->cell_prev = NULL;
        node->cell_next = NULL;
    }
    else
    {
        cell->cell_temp_list_tail->next = node;
        node->cell_prev = cell->cell_temp_list_tail;
        node->cell_next = NULL;
        cell->cell_temp_list_tail = node;
    }  
    
    g_temp_list_valid_siez++;
    return 0;
}
*/

/*
static int add_temp_to_node(unsigned short t)
{
    log_debug("%s", __func__);
    int i;
    for (i = 0; i < TEMP_CELL_SIZE; i++)
    {
        if (g_temp_cells[i].temp_start <= t && t <= g_temp_cells[i].temp_end)
        {
            g_temp_cells[i].cell_temp_num++;
            int j;
            for (j = 0; j < TEMP_INTERVAL / 10; j ++)
            {
                if (t == g_temp_cells[i].temp_num_dict[j].temp)
                    g_temp_cells[i].temp_num_dict[j].num++;
            }
        }
    }
    return 0;
}
*/

// 一一按信号要求算好平均值　　　求平均值信号处理逻辑不提供对外api，不准随便插入一个温度进来参与计算
static void average_temp_of_n_worker(sig_average_temp_of_n_t *signal)
{
    log_debug("%s", __func__);

    if (g_ca_tick % signal->interval != 0)
    {
        signal->sum += g_last_tick_temp;
    }
    else
    {
        signal->sum += g_last_tick_temp;  // 加上取模 等于0 时的温度
        if (signal->valid_size < signal->arr_size)  // p->avg_arr还没满的时候
        {
            signal->avg_arr[signal->valid_size] = signal->sum / signal->interval;
            signal->valid_size++;
        }
        else  // p->avg_arr满了，需要每个值往前移一个位置  最后一个位置放最新的avg
        {
            int i;
            for (i=0; i<signal->arr_size - 1; i++)
            {
                signal->avg_arr[i] = signal->avg_arr[i+1];
            }
            signal->avg_arr[signal->arr_size-1] = signal->sum / signal->interval;
        }
        signal->sum = 0;  // 算完一轮要置0
        CA_SIG_EMIT(signal);  // 平均值数组中每来一个新的就触发信号
    }
}

// 平均值比较具体的实现逻辑  判断信号对应的平均值数组是否满足相应比较规则　　　可以从外部任何时候传一个sig_average_compare_t类型的信号，来判断当前平均值数组是否满足相应规则，有利于同时调试多种规则，加快调试效率
bool api_is_average_compare_meet_rule(sig_average_compare_t *signal)
{
    log_debug("%s", __func__);
    int i;
    unsigned short minuend_index = 0;
    unsigned short subtrahend_index = 0;
    short compare_target = 0;
    enum COMPARE compare_type = 0;
    enum CALCULATE calculate_type = 0;
    enum RULE_REL rule_relation = 0;
    int or_meet_rule_index = -1;
    
    for (i=0; i<signal->average_compare_rules_arr_size; i++)  // 遍历数组对应的所有比较规则
    {
        minuend_index = signal->average_compare_rules_arr[i].minuend_index;
        subtrahend_index = signal->average_compare_rules_arr[i].subtrahend_index;
        compare_target = signal->average_compare_rules_arr[i].compare_target;
        compare_type = signal->average_compare_rules_arr[i].compare_type;
        calculate_type = signal->average_compare_rules_arr[i].calculate_type;
        rule_relation = signal->rule_relation;

        // 如果平均值的个数没满就不比较，直接返回false
        if (signal->sig_average_temp_of_n->valid_size < signal->sig_average_temp_of_n->arr_size)
            return false;

        // 根据compare_type 来分类比较 总是被减数-减数 只要出现一个不符合规则就直接返回false
        short diff = signal->sig_average_temp_of_n->avg_arr[minuend_index] - signal->sig_average_temp_of_n->avg_arr[subtrahend_index];
        if (calculate_type == CALC_ABS)  // 如果是做绝对值，就先取绝对值在判断
            diff = abs(diff);
        
        if (rule_relation == AND)
        {
            if ((compare_type == COMPARE_GTE && diff < compare_target) || (compare_type == COMPARE_LTE && diff > compare_target) || (compare_type == COMPARE_GT && diff <= compare_target) || (compare_type == COMPARE_LT && diff >= compare_target) || (compare_type == COMPARE_E && diff != compare_target))
            {
                log_debug("不满足比较规则：第%d个-第%d个%s(%d-%d)，期望差值%s%d，实际值为：%d", minuend_index, subtrahend_index, CA_ABS(calculate_type), signal->sig_average_temp_of_n->avg_arr[minuend_index], signal->sig_average_temp_of_n->avg_arr[subtrahend_index], CA_SYMBOL(compare_type), compare_target, diff);
                return false;
            }
        }
        else if(rule_relation == OR)
        {
            if ((compare_type == COMPARE_GTE && diff >= compare_target) || (compare_type == COMPARE_LTE && diff <= compare_target) || (compare_type == COMPARE_GT && diff > compare_target) || (compare_type == COMPARE_LT && diff < compare_target) || (compare_type == COMPARE_E && diff == compare_target))
            {
                or_meet_rule_index = i;  // or的时候，只需要满足任意一个即可，记住满足的index，方便后面打日志
                break;
            }
        }
    }
    if (rule_relation == OR && or_meet_rule_index == -1)  // or_meet_rule_index值没变的话，就说明一个比较规则都没满足
    {
        log_debug("不满足任意一个比较规则");
        return false;
    }

    // 日志打印
    if (rule_relation == AND)
    {
        char log_str[250] = {0};
        for (i=0; i<signal->average_compare_rules_arr_size; i++)
        {
            minuend_index = signal->average_compare_rules_arr[i].minuend_index;
            subtrahend_index = signal->average_compare_rules_arr[i].subtrahend_index;
            compare_target = signal->average_compare_rules_arr[i].compare_target;
            compare_type = signal->average_compare_rules_arr[i].compare_type;
            calculate_type = signal->average_compare_rules_arr[i].calculate_type;

            char s[50];
            sprintf(s, "%dth-%dth%s(%d-%d)%s%d ", minuend_index, subtrahend_index, CA_ABS(calculate_type), signal->sig_average_temp_of_n->avg_arr[minuend_index], signal->sig_average_temp_of_n->avg_arr[subtrahend_index], CA_SYMBOL(compare_type), compare_target);
            strcat(log_str, s);
        }
        log_debug("AND:%d rules all triggered：%s", signal->average_compare_rules_arr_size, log_str);
    }
    else if (rule_relation == OR)
    {
        minuend_index = signal->average_compare_rules_arr[or_meet_rule_index].minuend_index;
        subtrahend_index = signal->average_compare_rules_arr[or_meet_rule_index].subtrahend_index;
        compare_target = signal->average_compare_rules_arr[or_meet_rule_index].compare_target;
        compare_type = signal->average_compare_rules_arr[or_meet_rule_index].compare_type;
        calculate_type = signal->average_compare_rules_arr[or_meet_rule_index].calculate_type;
        char s[50];
        sprintf(s, "%dth-%dth%s(%d-%d)%s%d ", minuend_index, subtrahend_index, CA_ABS(calculate_type), signal->sig_average_temp_of_n->avg_arr[minuend_index], signal->sig_average_temp_of_n->avg_arr[subtrahend_index], CA_SYMBOL(compare_type), compare_target);
        log_debug("OR:%dth rule triggered：%s", or_meet_rule_index, s);
    }

    return true;  // 能走到这里说明该数组满足所有比较规则了
}

// signals_workers 内部调用的，需要配合emit用
static void emit_average_compare(sig_average_compare_t *signal)
{
    log_debug("%s", __func__);
    if (api_is_average_compare_meet_rule(signal))
        CA_SIG_EMIT(signal);
}

bool api_is_gental_tendency(sig_temp_gental_tendency_t *signal)
{
    if (api_is_average_compare_meet_rule(signal->sig_average_compare))
    {
        unsigned short *avg_arr = signal->sig_average_compare->sig_average_temp_of_n->avg_arr;
        unsigned short arr_valid_size = signal->sig_average_compare->sig_average_temp_of_n->valid_size;
        unsigned short interval = signal->sig_average_compare->sig_average_temp_of_n->interval;
        unsigned short arr_size = signal->sig_average_compare->sig_average_temp_of_n->arr_size;

        // 如果设置了范围，需要平均数组的第一个数值要大于等于floor，最后一个要小于等于ceiling，不满足的话，就直接返回false
        if (signal->is_set_range && (avg_arr[0] < signal->gental_range_floor || avg_arr[arr_valid_size - 1] > signal->gental_range_ceiling))
        {
            log_warn("超过了设置的温度范围");
            return false;
        }
        // 检查g_buildin_temp_tendency里面是否已经有了平稳趋势段
        if (g_buildin_temp_gental_tendency_head == NULL)
        {
            temp_tendency_info_t *latest_gental_tendency_info_node;
            latest_gental_tendency_info_node = malloc(sizeof(temp_tendency_info_t));
            if (latest_gental_tendency_info_node == NULL)
            {
                log_error("latest_gental_tendency_info_node指针获取失败");
                return false;
            }
            g_temp_gental_tendency_node_num++;
            g_buildin_temp_gental_tendency_head = latest_gental_tendency_info_node;
            g_buildin_temp_gental_tendency_tail = latest_gental_tendency_info_node;
            latest_gental_tendency_info_node->prev = NULL;
            latest_gental_tendency_info_node->next = NULL;

            latest_gental_tendency_info_node->tendency = TDC_GENTAL;
            latest_gental_tendency_info_node->tdc_start_avg_temp = avg_arr[0];  // 记录某段平缓开始时的平均温度
            latest_gental_tendency_info_node->tdc_end_avg_temp = avg_arr[arr_valid_size - 1];  // 记录某段平稳最后的温度
            latest_gental_tendency_info_node->tdc_start_tick = g_ca_tick - interval * arr_size + 1;
            latest_gental_tendency_info_node->tdc_end_tick = g_ca_tick;
            if (latest_gental_tendency_info_node->tdc_end_tick - latest_gental_tendency_info_node->tdc_start_tick + 1 >= signal->at_least_duration_ticks)
                return true;
        }
        else
        {
            // 从趋势链表后面开始往前找，找到最近一个平稳趋势段，判断是不是需要接在后面更新tdc_end_avg_temp　tdc_end_tick，还是新增一个平稳趋势段
            temp_tendency_info_t *p = g_buildin_temp_gental_tendency_tail;
            // while (p != NULL)  注释原因：各种趋势分开放了，不用判断了
            // {
            //     if (p->tendency == TDC_GENTAL)
            //         break;
            //     p = p->prev;
            // }
            // 满足首尾先差值条件，更新最近趋势的温度和时间
            if (abs(avg_arr[arr_valid_size - 1] - p->tdc_start_avg_temp) <= signal->at_most_diff_of_tdc_start_avg_temp_and_tdc_end_avg_temp)
            {
                p->tdc_end_avg_temp = avg_arr[arr_valid_size - 1];
                p->tdc_end_tick = g_ca_tick;
                // 满足至少持续的时间要求才触发信号
                if (p->tdc_end_tick - p->tdc_start_tick + 1 >= signal->at_least_duration_ticks)
                    return true;
            }
            //　不满足就新增趋势节点
            /*只有趋势段收尾tick相差　interval * arr_size　才触发信号．本来单段是一定满足这个条件的，但是新增一个趋势段，就不一定了.
            * 新增一个段时，最后一个平均值打破了原本的平稳，不满足at_most_diff_of_tdc_start_avg_temp_and_tdc_end_avg_temp条件，需要等到这个最后平均值
            * 跑到平均值数组的第一个时，才能开始算新的平稳趋势
            */ 
            else if (g_ca_tick - p->tdc_end_tick >= interval * arr_size)
            {
                temp_tendency_info_t *latest_gental_tendency_info_node;
                latest_gental_tendency_info_node = malloc(sizeof(temp_tendency_info_t));
                if (latest_gental_tendency_info_node == NULL)
                {
                    log_error("latest_gental_tendency_info_node指针获取失败");
                    return false;
                }
                g_temp_gental_tendency_node_num++;
                g_buildin_temp_gental_tendency_tail->next = latest_gental_tendency_info_node;
                latest_gental_tendency_info_node->prev = g_buildin_temp_gental_tendency_tail;
                latest_gental_tendency_info_node->next = NULL;
                g_buildin_temp_gental_tendency_tail = latest_gental_tendency_info_node;

                latest_gental_tendency_info_node->tendency = TDC_GENTAL;
                latest_gental_tendency_info_node->tdc_start_avg_temp = avg_arr[0];
                latest_gental_tendency_info_node->tdc_end_avg_temp = avg_arr[arr_valid_size - 1];
                latest_gental_tendency_info_node->tdc_start_tick = g_ca_tick -  interval * arr_size + 1;
                latest_gental_tendency_info_node->tdc_end_tick = g_ca_tick;

                p = latest_gental_tendency_info_node;  // 新增了node，把p指向新增的
                if (p->tdc_end_tick - p->tdc_start_tick + 1 >= signal->at_least_duration_ticks)
                    return true;
            }
        }
    }
    return false;
}

static void emit_gental_tendency(sig_temp_gental_tendency_t *signal)
{
    log_debug("%s", __func__);
    if (api_is_gental_tendency(signal))
        CA_SIG_EMIT(signal);
}

bool api_is_rise_tendency(sig_temp_rise_tendency_t *signal)
{
    log_debug("%s", __func__);
    unsigned short *avg_arr = signal->sig_average_compare->sig_average_temp_of_n->avg_arr;
    unsigned short arr_valid_size = signal->sig_average_compare->sig_average_temp_of_n->valid_size;
    unsigned short interval = signal->sig_average_compare->sig_average_temp_of_n->interval;
    unsigned short arr_size = signal->sig_average_compare->sig_average_temp_of_n->arr_size;

    if (api_is_average_compare_meet_rule(signal->sig_average_compare))
    {
        if (signal->is_set_range && (avg_arr[0] > signal->rise_range_ceiling || avg_arr[arr_valid_size - 1] < signal->rise_range_floor))
        {
            log_warn("超过了设置的温度范围");
            return false;
        }

        temp_tendency_info_t *latest_rise_tendency_info_node;
        if (g_buildin_temp_rise_tendency_head == NULL)
        {   
            // 最后一个平均值需要比其他的都大　第一个趋势节点严格一点
            if (avg_arr[arr_valid_size -1] > avg_arr[0] && avg_arr[arr_valid_size - 1] > avg_arr[1] && avg_arr[arr_valid_size - 1] > avg_arr[2] && avg_arr[arr_valid_size - 1] > avg_arr[3] && avg_arr[arr_valid_size - 1] > avg_arr[4] \
              && avg_arr[arr_valid_size -1] > avg_arr[5] && avg_arr[arr_valid_size - 1] > avg_arr[6] && avg_arr[arr_valid_size - 1] > avg_arr[7] && avg_arr[arr_valid_size - 1] > avg_arr[8])
            {
                log_debug("rise g_tick0: %d  %d", g_ca_tick, avg_arr[0]);
                latest_rise_tendency_info_node = malloc(sizeof(temp_tendency_info_t));
                if (latest_rise_tendency_info_node == NULL)
                {
                    log_error("latest_rise_tendency_info_node指针获取失败");
                    return false;
                }
                g_temp_rise_tendency_node_num++;
                g_buildin_temp_rise_tendency_head = latest_rise_tendency_info_node;
                g_buildin_temp_rise_tendency_tail = latest_rise_tendency_info_node;
                latest_rise_tendency_info_node->prev = NULL;
                latest_rise_tendency_info_node->next = NULL;

                latest_rise_tendency_info_node->tendency = TDC_RISE;
                latest_rise_tendency_info_node->tdc_start_avg_temp = avg_arr[0];
                latest_rise_tendency_info_node->tdc_end_avg_temp = avg_arr[arr_valid_size - 1];
                latest_rise_tendency_info_node->tdc_start_tick = g_ca_tick -  interval * arr_size + 1;
                latest_rise_tendency_info_node->tdc_end_tick = g_ca_tick;
            }
        }
        else
        {
            temp_tendency_info_t *p = g_buildin_temp_rise_tendency_tail;

            bool is_create_new_node = false;
            // 温度上升幅度在定义规则之内，并且时间在一轮计算周期之内，就更新最新平均温度和最新时间
            if (avg_arr[arr_valid_size - 1] - p->tdc_end_avg_temp >= signal->at_leaset_rise && avg_arr[arr_valid_size - 1] - p->tdc_end_avg_temp <= signal->at_most_rise && g_ca_tick - p->tdc_end_tick <= interval * arr_size)
            {
                log_debug("rise: update temp tick");
                p->tdc_end_avg_temp = avg_arr[arr_valid_size - 1];
                p->tdc_end_tick = g_ca_tick;
            }
            //　最新的两个平均值差值在定义规则之外，并且时间大于一个计算周期，就新增趋势节点
            else if ((avg_arr[arr_valid_size - 1] - p->tdc_end_avg_temp < signal->at_leaset_rise|| avg_arr[arr_valid_size - 1] - p->tdc_end_avg_temp > signal->at_leaset_rise) && g_ca_tick - p->tdc_end_tick >= arr_size * interval)
            {
                latest_rise_tendency_info_node = malloc(sizeof(temp_tendency_info_t));
                if (latest_rise_tendency_info_node == NULL)
                {
                    log_error("latest_rise_tendency_info_node指针获取失败");
                    return false;
                }
                g_temp_rise_tendency_node_num++;
                g_buildin_temp_rise_tendency_tail->next = latest_rise_tendency_info_node;
                latest_rise_tendency_info_node->prev = g_buildin_temp_rise_tendency_tail;
                latest_rise_tendency_info_node->next = NULL;
                g_buildin_temp_rise_tendency_tail = latest_rise_tendency_info_node;

                latest_rise_tendency_info_node->tendency = TDC_RISE;
                latest_rise_tendency_info_node->tdc_start_avg_temp = avg_arr[0];
                latest_rise_tendency_info_node->tdc_end_avg_temp = avg_arr[arr_valid_size - 1];
                latest_rise_tendency_info_node->tdc_start_tick = g_ca_tick - interval * arr_size + 1;
                latest_rise_tendency_info_node->tdc_end_tick = g_ca_tick;
                is_create_new_node = true;
            }       
            //　如果刚刚新增过了趋势节点，并且（上一个节点首尾温度差小于定义的最小要上升的温度，或者维持上升的时间比定义的小），就把上一个节点删掉
            if (is_create_new_node && (p->tdc_end_avg_temp - p->tdc_start_avg_temp < signal->at_leaset_rise || p->tdc_end_tick - p->tdc_start_tick < signal->at_least_duration_ticks))  // 这里的ｐ是g_buildin_temp_rise_tendency_tail的prev了
            {
                log_debug("rise g_ca_tick: %d  %d", g_ca_tick, p->tdc_start_avg_temp);
                if (p == g_buildin_temp_rise_tendency_head)
                {
                    g_buildin_temp_rise_tendency_head = p->next;
                    free(g_buildin_temp_rise_tendency_head->prev);
                }
                else
                {
                    p->prev->next = g_buildin_temp_rise_tendency_tail;
                    g_buildin_temp_rise_tendency_tail->prev = p->prev;
                    g_buildin_temp_rise_tendency_tail->next = NULL;
                    free(p);
                }
                g_temp_rise_tendency_node_num--;
            }
        }

        if (g_buildin_temp_rise_tendency_tail != NULL)
        {
            // 尾节点的首尾温度差至少大于定义的最小温升，并且时间至少为定义的时，才触发信号回调
            if (g_buildin_temp_rise_tendency_tail->tdc_end_avg_temp - g_buildin_temp_rise_tendency_tail->tdc_start_avg_temp >= signal->at_leaset_rise &&
                g_buildin_temp_rise_tendency_tail->tdc_end_tick - g_buildin_temp_rise_tendency_tail->tdc_start_tick + 1 >= signal->at_least_duration_ticks)
            {
                //　再加个限制条件，如果tdc_end_tick与当前tick不一致，说明经过上面没有更新，也没新增，就返回false.　相反，一致的话，才返回true
                if (g_ca_tick == g_buildin_temp_rise_tendency_tail->tdc_end_tick)
                    return true;
            }
        }
    }

    return false;
}

static void emit_rise_tendency(sig_temp_rise_tendency_t *signal)
{
    log_debug("%s", __func__);
    if (g_ca_tick % signal->sig_average_compare->sig_average_temp_of_n->interval == 0 && api_is_rise_tendency(signal))
        CA_SIG_EMIT(signal);
}

bool api_is_down_tendency(sig_temp_down_tendency_t *signal)
{
    log_debug("%s", __func__);
    unsigned short *avg_arr = signal->sig_average_compare->sig_average_temp_of_n->avg_arr;
    unsigned short arr_valid_size = signal->sig_average_compare->sig_average_temp_of_n->valid_size;
    unsigned short interval = signal->sig_average_compare->sig_average_temp_of_n->interval;
    unsigned short arr_size = signal->sig_average_compare->sig_average_temp_of_n->arr_size;

    if (api_is_average_compare_meet_rule(signal->sig_average_compare))
    {
        if (signal->is_set_range && (avg_arr[0] < signal->down_range_floor || avg_arr[arr_valid_size - 1] > signal->down_range_ceiling))
        {
            log_warn("超过了设置的温度范围");
            return false;
        }

        temp_tendency_info_t *latest_down_tendency_info_node;
        if (g_buildin_temp_down_tendency_head == NULL)
        {   
            if (avg_arr[arr_valid_size -1] < avg_arr[0] && avg_arr[arr_valid_size - 1] < avg_arr[1] && avg_arr[arr_valid_size - 1] < avg_arr[2] && avg_arr[arr_valid_size - 1] < avg_arr[3] && avg_arr[arr_valid_size - 1] < avg_arr[4] \
              && avg_arr[arr_valid_size -1] < avg_arr[5] && avg_arr[arr_valid_size - 1] < avg_arr[6] && avg_arr[arr_valid_size - 1] < avg_arr[7] && avg_arr[arr_valid_size - 1] < avg_arr[8])
            {
                log_debug("down g_tick0: %d  %d", g_ca_tick, avg_arr[0]);
                latest_down_tendency_info_node = malloc(sizeof(temp_tendency_info_t));
                if (latest_down_tendency_info_node == NULL)
                {
                    log_error("latest_down_tendency_info_node指针获取失败");
                    return false;
                }
                g_temp_down_tendency_node_num++;
                g_buildin_temp_down_tendency_head = latest_down_tendency_info_node;
                g_buildin_temp_down_tendency_tail = latest_down_tendency_info_node;
                latest_down_tendency_info_node->prev = NULL;
                latest_down_tendency_info_node->next = NULL;

                latest_down_tendency_info_node->tendency = TDC_DOWN;
                latest_down_tendency_info_node->tdc_start_avg_temp = avg_arr[0];
                latest_down_tendency_info_node->tdc_end_avg_temp = avg_arr[arr_valid_size - 1];
                latest_down_tendency_info_node->tdc_start_tick = g_ca_tick - arr_size * interval + 1;
                latest_down_tendency_info_node->tdc_end_tick = g_ca_tick;
            }
        }
        else
        {
            temp_tendency_info_t *p = g_buildin_temp_down_tendency_tail;
            log_debug("down g_tick10: g_ca_tick%d  tdc_start_tick%d tdc_start_avg_temp%d tdc_end_tick%d tdc_end_avg_temp%d", g_ca_tick, p->tdc_start_tick, p->tdc_start_avg_temp, p->tdc_end_tick, p->tdc_end_avg_temp);
            
            bool is_create_new_node = false;
            if (p->tdc_end_avg_temp - avg_arr[arr_valid_size - 1] >= signal->at_leaset_down && p->tdc_end_avg_temp - avg_arr[arr_valid_size - 1] <= signal->at_most_down && g_ca_tick - p->tdc_end_tick <= interval * arr_size)
            {
                log_debug("down: update temp tick");
                p->tdc_end_avg_temp = avg_arr[arr_valid_size - 1];
                p->tdc_end_tick = g_ca_tick;
            }
            else if ((p->tdc_end_avg_temp - avg_arr[arr_valid_size - 1] < signal->at_leaset_down|| p->tdc_end_avg_temp - avg_arr[arr_valid_size - 1] > signal->at_leaset_down) && g_ca_tick - p->tdc_end_tick >= arr_size * interval)
            {
                log_debug("down g_tick3: %d  %d", g_ca_tick, p->tdc_start_avg_temp);
                latest_down_tendency_info_node = malloc(sizeof(sig_temp_down_tendency_t));
                if (latest_down_tendency_info_node == NULL)
                {
                    log_error("latest_down_tendency_info_node指针获取失败");
                    return false;
                }
                g_temp_down_tendency_node_num++;
                g_buildin_temp_down_tendency_tail->next = latest_down_tendency_info_node;
                latest_down_tendency_info_node->prev = g_buildin_temp_down_tendency_tail;
                latest_down_tendency_info_node->next = NULL;
                g_buildin_temp_down_tendency_tail = latest_down_tendency_info_node;

                latest_down_tendency_info_node->tendency = TDC_DOWN;
                latest_down_tendency_info_node->tdc_start_avg_temp = avg_arr[0];
                latest_down_tendency_info_node->tdc_end_avg_temp = avg_arr[arr_valid_size - 1];
                latest_down_tendency_info_node->tdc_start_tick = g_ca_tick - interval * arr_size + 1;
                latest_down_tendency_info_node->tdc_end_tick = g_ca_tick;
                is_create_new_node = true;
            }       

            if (is_create_new_node && (p->tdc_start_avg_temp - p->tdc_end_avg_temp  < signal->at_leaset_down || p->tdc_end_tick - p->tdc_start_tick < signal->at_least_duration_ticks))  // 这里的ｐ是g_buildin_temp_down_tendency_tail的prev了
            {
                log_debug("down g_ca_tick: %d  %d", g_ca_tick, p->tdc_start_avg_temp);
                if (p == g_buildin_temp_down_tendency_head)
                {
                    g_buildin_temp_down_tendency_head = p->next;
                    free(g_buildin_temp_down_tendency_head->prev);
                }
                else
                {
                    p->prev->next = g_buildin_temp_down_tendency_tail;
                    g_buildin_temp_down_tendency_tail->prev = p->prev;
                    g_buildin_temp_down_tendency_tail->next = NULL;
                    free(p);
                }
                g_temp_down_tendency_node_num--;
            }
        }

        if (g_buildin_temp_down_tendency_tail != NULL)
        {
            if (g_buildin_temp_down_tendency_tail->tdc_start_avg_temp - g_buildin_temp_down_tendency_tail->tdc_end_avg_temp >= signal->at_leaset_down &&
                g_buildin_temp_down_tendency_tail->tdc_end_tick - g_buildin_temp_down_tendency_tail->tdc_start_tick + 1 >= signal->at_least_duration_ticks)
            {
                if (g_ca_tick == g_buildin_temp_down_tendency_tail->tdc_end_tick)
                    return true;
            }
        }
    }

    return false;
}

static void emit_down_tendency(sig_temp_down_tendency_t *signal)
{
    log_debug("%s", __func__);
    if (g_ca_tick % signal->sig_average_compare->sig_average_temp_of_n->interval == 0 && api_is_down_tendency(signal))
        CA_SIG_EMIT(signal);
}

bool api_is_jump_rise_tendency(sig_temp_jump_rise_tendency_t *signal)
{
    log_debug("%s", __func__);
    unsigned short *avg_arr = signal->sig_average_compare->sig_average_temp_of_n->avg_arr;
    unsigned short arr_valid_size = signal->sig_average_compare->sig_average_temp_of_n->valid_size;
    unsigned short interval = signal->sig_average_compare->sig_average_temp_of_n->interval;
    int start_jump_temp = -1;
    int start_jump_tick = 0;
    int i;

    if (api_is_average_compare_meet_rule(signal->sig_average_compare))
    {
        if (signal->is_set_range && (avg_arr[0] > signal->jump_rise_range_ceiling || avg_arr[arr_valid_size - 1] < signal->jump_rise_range_floor))
        {
            log_warn("超过了设置的温度范围");
            return false;
        }

        temp_tendency_info_t *latest_jump_rise_tendency_info_node;
        int j = 1;  // 作为at_least_rise系数
        for (i=arr_valid_size - 2; i>=0; i--)
        {
            bool is_meet_in_4 = false;
            if (avg_arr[arr_valid_size - 1] - avg_arr[i]  <= signal->at_most_jump_rise && avg_arr[arr_valid_size - 1] - avg_arr[i] >= signal->at_leaset_jump_rise * j)
            {
                start_jump_temp = avg_arr[i];
                start_jump_tick = g_ca_tick - j * interval;
                if (j <= 4)
                    is_meet_in_4 = true;
            }

            // 加这个的原因是：比如最少跳升４度，但可能相邻几个都只是相差３度，导致上面的判断失效，因此不乘系数，扩大到１秒之内
            if (j <=4 && !is_meet_in_4 && avg_arr[arr_valid_size - 1] - avg_arr[i] >= signal->at_leaset_jump_rise)  // 距离最新温度一秒内的不需要乘系数
            {
                if (avg_arr[i] < start_jump_temp)  // 更小的时候才替换
                {
                    start_jump_temp = avg_arr[i];
                    start_jump_tick = g_ca_tick - j * interval;
                }
            }
            j++;  // 需要放到外面，因为不满足上面的if也是需要累加的
        }

        if (start_jump_temp != -1)
        {
            // 因为上面用了一秒内免乘系数的，所以会出现很多重叠的结果，这里把重叠的结果整合　　整合需要触发信号回调么？不需要
            if (g_buildin_temp_jump_rise_tendency_tail != NULL)  // 查看这次与前一次的记录是否有温度时间上的重复，有的话就更新node信息,不新增node
            {
                // 最新一次的开始跳变的时间在上一次的开始结束时间内，就更新
                if (start_jump_tick >= g_buildin_temp_jump_rise_tendency_tail->tdc_start_tick && start_jump_tick <= g_buildin_temp_jump_rise_tendency_tail->tdc_end_tick)
                {
                    // end值更大，更新end值
                    if (avg_arr[arr_valid_size - 1] > g_buildin_temp_jump_rise_tendency_tail->tdc_end_avg_temp)
                    {
                        g_buildin_temp_jump_rise_tendency_tail->tdc_end_avg_temp = avg_arr[arr_valid_size - 1];
                        g_buildin_temp_jump_rise_tendency_tail->tdc_end_tick = g_ca_tick;
                    }
                    // start值更小，更新start值
                    if (start_jump_temp < g_buildin_temp_jump_rise_tendency_tail->tdc_start_avg_temp)
                    {
                        g_buildin_temp_jump_rise_tendency_tail->tdc_start_avg_temp = start_jump_temp;
                        g_buildin_temp_jump_rise_tendency_tail->tdc_end_tick = start_jump_tick;
                    }

                    return false;  // 整合不需要触发信号回调
                }
            }

            latest_jump_rise_tendency_info_node = malloc(sizeof(temp_tendency_info_t));
            if (latest_jump_rise_tendency_info_node == NULL)
            {
                log_error("latest_jump_rise_tendency_info_node指针获取失败");
                return false;
            }
            g_temp_jump_rise_tendency_node_num++;

            if (g_buildin_temp_jump_rise_tendency_head == NULL)
            {
                g_buildin_temp_jump_rise_tendency_head = latest_jump_rise_tendency_info_node;
                latest_jump_rise_tendency_info_node->prev = NULL;
            }
            else
            {
                latest_jump_rise_tendency_info_node->prev = g_buildin_temp_jump_rise_tendency_tail;
                g_buildin_temp_jump_rise_tendency_tail->next = latest_jump_rise_tendency_info_node;
            }
            g_buildin_temp_jump_rise_tendency_tail = latest_jump_rise_tendency_info_node;
            latest_jump_rise_tendency_info_node->next = NULL;

            latest_jump_rise_tendency_info_node->tendency = TDC_JUMP_RISE;
            latest_jump_rise_tendency_info_node->tdc_start_avg_temp = start_jump_temp;
            latest_jump_rise_tendency_info_node->tdc_end_avg_temp = avg_arr[arr_valid_size - 1];
            latest_jump_rise_tendency_info_node->tdc_start_tick = start_jump_tick;
            latest_jump_rise_tendency_info_node->tdc_end_tick = g_ca_tick;

            return true;
        }
    }
    return false;
}

static void emit_jump_rise_tendency(sig_temp_jump_rise_tendency_t *signal)
{
    log_debug("%s", __func__);
    if (g_ca_tick % signal->sig_average_compare->sig_average_temp_of_n->interval == 0 && api_is_jump_rise_tendency(signal))
        CA_SIG_EMIT(signal);
}

bool api_is_jump_down_tendency(sig_temp_jump_down_tendency_t *signal)
{
    log_debug("%s", __func__);
    unsigned short *avg_arr = signal->sig_average_compare->sig_average_temp_of_n->avg_arr;
    unsigned short arr_valid_size = signal->sig_average_compare->sig_average_temp_of_n->valid_size;
    unsigned short interval = signal->sig_average_compare->sig_average_temp_of_n->interval;
    int start_jump_temp = -1;
    int start_jump_tick = 0;
    int i;

    if (api_is_average_compare_meet_rule(signal->sig_average_compare))
    {
        if (signal->is_set_range && (avg_arr[0] < signal->jump_down_range_floor || avg_arr[arr_valid_size - 1] > signal->jump_down_range_ceiling))
        {
            log_warn("超过了设置的温度范围");
            return false;
        }

        temp_tendency_info_t *latest_jump_down_tendency_info_node;
        int j = 1;
        for (i=arr_valid_size - 2; i>=0; i--)
        {
            bool is_meet_in_4 = false;
            if (avg_arr[i] - avg_arr[arr_valid_size - 1] <= signal->at_most_jump_down && avg_arr[i] - avg_arr[arr_valid_size - 1] >= signal->at_leaset_jump_down * j)
            {
                start_jump_temp = avg_arr[i];
                start_jump_tick = g_ca_tick - j * interval;
                if (j <= 4)
                    is_meet_in_4 = true;
            }

            if (j <=4 && !is_meet_in_4 && avg_arr[i]- avg_arr[arr_valid_size - 1] >= signal->at_leaset_jump_down)  // 距离最新温度一秒内的不需要乘系数
            {
                if (avg_arr[i] > start_jump_temp)  // start值取小的
                {
                    start_jump_temp = avg_arr[i];
                    start_jump_tick = g_ca_tick - j * interval;
                }
            }
            j++;  // 需要放到外面，因为不满足上面的if也是需要累加的
        }

        if (start_jump_temp != -1)
        {
            if (g_buildin_temp_jump_down_tendency_tail != NULL)  // 查看这次与前一次的记录是否有温度时间上的重复，有的话就更新node信息,不新增node
            {
                // 最新一次的开始跳变的时间在上一次的开始结束时间内，就更新
                if (start_jump_tick >= g_buildin_temp_jump_down_tendency_tail->tdc_start_tick && start_jump_tick <= g_buildin_temp_jump_down_tendency_tail->tdc_end_tick )
                {
                    // end值更小，更新end值
                    if (avg_arr[arr_valid_size - 1] < g_buildin_temp_jump_down_tendency_tail->tdc_end_avg_temp)
                    {
                        g_buildin_temp_jump_down_tendency_tail->tdc_end_tick = g_ca_tick;
                        g_buildin_temp_jump_down_tendency_tail->tdc_end_avg_temp = avg_arr[arr_valid_size - 1];
                    }
                    // start值更大，更新start值
                    if (start_jump_temp > g_buildin_temp_jump_down_tendency_tail->tdc_start_avg_temp)
                    {
                        g_buildin_temp_jump_down_tendency_tail->tdc_start_avg_temp = start_jump_temp;
                        g_buildin_temp_jump_down_tendency_tail->tdc_end_tick = start_jump_tick;
                    }

                     return false;  // 整合不需要触发信号回调
                }
            }

            latest_jump_down_tendency_info_node = malloc(sizeof(temp_tendency_info_t));
            if (latest_jump_down_tendency_info_node == NULL)
            {
                log_error("latest_jump_down_tendency_info_node指针获取失败");
                return false;
            }
            g_temp_jump_down_tendency_node_num++;

            if (g_buildin_temp_jump_down_tendency_head == NULL)
            {
                g_buildin_temp_jump_down_tendency_head = latest_jump_down_tendency_info_node;
                latest_jump_down_tendency_info_node->prev = NULL;
            }
            else
            {
                latest_jump_down_tendency_info_node->prev = g_buildin_temp_jump_down_tendency_tail;
                g_buildin_temp_jump_down_tendency_tail->next = latest_jump_down_tendency_info_node;
            }
            g_buildin_temp_jump_down_tendency_tail = latest_jump_down_tendency_info_node;
            latest_jump_down_tendency_info_node->next = NULL;

            latest_jump_down_tendency_info_node->tendency = TDC_JUMP_DOWN;
            latest_jump_down_tendency_info_node->tdc_start_avg_temp = start_jump_temp;
            latest_jump_down_tendency_info_node->tdc_end_avg_temp = avg_arr[arr_valid_size - 1];
            latest_jump_down_tendency_info_node->tdc_start_tick = start_jump_tick;
            latest_jump_down_tendency_info_node->tdc_end_tick = g_ca_tick;

            return true;
        }
    }
    return false;
}

static void emit_jump_down_tendency(sig_temp_jump_down_tendency_t *signal)
{
    log_debug("%s", __func__);
    if (g_ca_tick % signal->sig_average_compare->sig_average_temp_of_n->interval == 0 && api_is_jump_down_tendency(signal))
        CA_SIG_EMIT(signal);
}

bool api_is_shake_tendency(sig_temp_shake_tendency_t *signal)
{
    log_debug("%s", __func__);
    unsigned short *avg_arr = signal->sig_average_compare->sig_average_temp_of_n->avg_arr;
    unsigned short arr_valid_size = signal->sig_average_compare->sig_average_temp_of_n->valid_size;
    unsigned short interval = signal->sig_average_compare->sig_average_temp_of_n->interval;
    unsigned short arr_size = signal->sig_average_compare->sig_average_temp_of_n->arr_size;
    int max = 0;
    int min = 10000;
    int direction_change_times = 0;
    int i;

    if (api_is_average_compare_meet_rule(signal->sig_average_compare))
    {
        if (signal->is_set_range && (avg_arr[0] < signal->jump_down_range_floor || avg_arr[arr_valid_size - 1] > signal->jump_down_range_ceiling))
        {
            log_warn("超过了设置的温度范围");
            return false;
        }
        
        for (i=0; i<arr_valid_size - 2; i++)
        {
            if (avg_arr[i] > max)
                max = avg_arr[i];
            if (avg_arr[i] < min)
                min = avg_arr[i];
            
            // v 或者　倒v
            if ((avg_arr[i] > avg_arr[i + 1] && avg_arr[i + 2] > avg_arr[i + 1]) || (avg_arr[i] < avg_arr[i + 1] && avg_arr[i + 2] < avg_arr[i + 1]))
                direction_change_times++;   
        }
        for (i=arr_valid_size - 2; i<arr_valid_size; i++)
        {
            if (avg_arr[i] > max)
                max = avg_arr[i];
            if (avg_arr[i] < min)
                min = avg_arr[i];
        }

        if (direction_change_times >= signal->at_least_temp_direction_change_times && max - min >= signal->at_least_diff_of_max_min)
        {
            if (g_buildin_temp_shake_tendency_tail != NULL)
            {
                // 时间没超过一轮，就更新
                if (g_ca_tick - g_buildin_temp_shake_tendency_tail->tdc_end_tick <= interval * arr_size)
                {
                    g_buildin_temp_shake_tendency_tail->tdc_end_avg_temp = avg_arr[arr_valid_size - 1];
                    g_buildin_temp_shake_tendency_tail->tdc_end_tick = g_ca_tick;

                    // 这里与跳升跳降不同，场景不同，跳变只需要触发一次信号即可，本来就是短时的场景，而跳变是个长时的场景，因此不能像跳变一样直接返回false.这里当满足条件时，需要持续触发回调的
                    if (g_buildin_temp_shake_tendency_tail->tdc_end_tick - g_buildin_temp_shake_tendency_tail->tdc_start_tick >= signal->at_least_duration_ticks)
                        return true;
                    else
                        return false;  // 更新后不满足条件，就先不触发信号回调
                }
            }

            temp_tendency_info_t *latest_shake_tendency_info_node;
            latest_shake_tendency_info_node = malloc(sizeof(temp_tendency_info_t));
            if (latest_shake_tendency_info_node == NULL)
            {
                log_error("latest_shake_tendency_info_node指针获取失败");
                return false;
            }
            g_temp_shake_tendency_node_num++;

            if (g_buildin_temp_shake_tendency_head == NULL)
            {
                g_buildin_temp_shake_tendency_head = latest_shake_tendency_info_node;
                latest_shake_tendency_info_node->prev = NULL;
            }
            else
            {
                latest_shake_tendency_info_node->prev = g_buildin_temp_shake_tendency_tail;
                g_buildin_temp_shake_tendency_tail->next = latest_shake_tendency_info_node;
            }
            g_buildin_temp_shake_tendency_tail = latest_shake_tendency_info_node;
            latest_shake_tendency_info_node->next = NULL;

            latest_shake_tendency_info_node->tendency = TDC_SHAKE;
            latest_shake_tendency_info_node->tdc_start_avg_temp = avg_arr[0];
            latest_shake_tendency_info_node->tdc_end_avg_temp = avg_arr[arr_valid_size - 1];
            latest_shake_tendency_info_node->tdc_start_tick = g_ca_tick - interval * arr_size;
            latest_shake_tendency_info_node->tdc_end_tick = g_ca_tick;
            if (g_buildin_temp_shake_tendency_tail->tdc_end_tick - g_buildin_temp_shake_tendency_tail->tdc_start_tick >= signal->at_least_duration_ticks)
                return true;
        }
    }
    return false;
}

static void emit_shake_tendency(sig_temp_shake_tendency_t *signal)
{
    log_debug("%s", __func__);
    if (g_ca_tick % signal->sig_average_compare->sig_average_temp_of_n->interval == 0 && api_is_shake_tendency(signal))
        CA_SIG_EMIT(signal);
}

bool api_is_rise_ff_tendency(sig_rise_faster_and_faster_t *signal)
{
    unsigned short *avg_arr = signal->sig_average_temp_of_n->avg_arr;
    unsigned short arr_valid_size = signal->sig_average_temp_of_n->valid_size;

    if (signal->is_set_range && (avg_arr[0] > signal->faster_and_faster_range_ceiling || avg_arr[arr_valid_size - 1] < signal->faster_and_faster_range_floor))
    {
        log_warn("超过了设置的温度范围");
        return false;
    }
    if (rise_ff_last_reach_step_avg_temp == 0)
    {
        rise_ff_last_reach_step_avg_temp = avg_arr[0];
        rise_ff_last_reach_step_tick = 1;
    }

    if (avg_arr[arr_valid_size - 1] - rise_ff_last_reach_step_avg_temp >= signal->rise_step)
    {
        log_debug("shake0 avg_arr[arr_valid_size - 1] %d rise_ff_last_reach_step_avg_temp %d", avg_arr[arr_valid_size - 1], rise_ff_last_reach_step_avg_temp);
        temp_tendency_info_t *latest_rise_ff_tendency_info_node;
        latest_rise_ff_tendency_info_node = malloc(sizeof(temp_tendency_info_t));
        if (latest_rise_ff_tendency_info_node == NULL)
        {
            log_error("latest_rise_ff_tendency_info_node指针获取失败");
            return false;
        }
        g_temp_rise_ff_tendency_node_num++;

        if (g_buildin_temp_rise_ff_tendency_head == NULL)
        {
            g_buildin_temp_rise_ff_tendency_head = latest_rise_ff_tendency_info_node;
            latest_rise_ff_tendency_info_node->prev = NULL;
        }
        else
        {
            latest_rise_ff_tendency_info_node->prev = g_buildin_temp_rise_ff_tendency_tail;
            g_buildin_temp_rise_ff_tendency_tail->next = latest_rise_ff_tendency_info_node;
        }
        g_buildin_temp_rise_ff_tendency_tail = latest_rise_ff_tendency_info_node;
        latest_rise_ff_tendency_info_node->next = NULL;

        latest_rise_ff_tendency_info_node->tendency = TDC_RISE_FF;
        latest_rise_ff_tendency_info_node->tdc_start_avg_temp = rise_ff_last_reach_step_avg_temp;
        latest_rise_ff_tendency_info_node->tdc_end_avg_temp = avg_arr[arr_valid_size - 1];
        latest_rise_ff_tendency_info_node->tdc_start_tick = rise_ff_last_reach_step_tick;
        latest_rise_ff_tendency_info_node->tdc_end_tick = g_ca_tick;

        rise_ff_last_reach_step_avg_temp = avg_arr[arr_valid_size - 1];
        rise_ff_last_reach_step_tick = g_ca_tick;
    }

    // 至少有cost_compate_times + 1个节点，才能满足设置的判断"越来越"的现象，检查节点够不够
    int node_num = 0;
    temp_tendency_info_t *p = g_buildin_temp_rise_ff_tendency_tail;
    while (p != NULL)
    {
        node_num++;
        if (node_num > signal->cost_compate_times)
            break;
        p = p->prev;
    }
    if (node_num <= signal->cost_compate_times)
        return false;

    // 如果距离最后一次的时间超过了最后一次cost的两倍，或者最新的平均值小于g_buildin_temp_rise_ff_tendency_tail的tdc_end_avg_temp，就返回false
    p = g_buildin_temp_rise_ff_tendency_tail;
    log_debug("g_ca_tick %d, p->tdc_end_tick %d, %d, p->tdc_start_tick %d, %d",g_ca_tick, p->tdc_end_tick, g_ca_tick - p->tdc_end_tick, p->tdc_start_tick, p->tdc_end_tick - p->tdc_start_tick);
    if (g_ca_tick - p->tdc_end_tick > (p->tdc_end_tick - p->tdc_start_tick) * 2 || avg_arr[arr_valid_size - 1] < p->tdc_end_avg_temp)
        return false;

    // 节点数够比较了
    int compate_times = signal->cost_compate_times;
    unsigned int tail_cost_ticks = g_buildin_temp_rise_ff_tendency_tail->tdc_end_tick - g_buildin_temp_rise_ff_tendency_tail->tdc_start_tick;
    p = g_buildin_temp_rise_ff_tendency_tail;
    while (1)
    {
        compate_times--;
        unsigned int prev_cost_ticks = p->prev->tdc_end_tick - p->prev->tdc_start_tick;
        unsigned int p_cost_ticks = p->tdc_end_tick - p->tdc_start_tick;
        if (prev_cost_ticks > signal->at_most_single_step_cost)  // cost遇到设定的最大值就退出循环
            break;

        log_debug("compate_times %d ", compate_times);
        // 最后一个cost一定要比前面第cost_compate_times个小
        if (compate_times == 0 && tail_cost_ticks < prev_cost_ticks && p_cost_ticks <= prev_cost_ticks)
        {
            log_debug("tail_cost_ticks %d, prev_cost_ticks %d, p_cost_ticks %d", tail_cost_ticks , prev_cost_ticks, p_cost_ticks);
            p = p->prev;
        }
        else if(compate_times != 0 && p_cost_ticks <= prev_cost_ticks)
        {
            log_debug("p_cost_ticks %d, prev_cost_ticks %d", p_cost_ticks , prev_cost_ticks);
            p = p->prev;
        }
        else
            break;
        
        if (compate_times == 0)
            return true;
    }
    return false;
}

static void emit_rise_ff_tendency(sig_rise_faster_and_faster_t *signal)
{
    log_debug("%s", __func__);
    if (g_ca_tick % signal->sig_average_temp_of_n->interval == 0 && api_is_rise_ff_tendency(signal))
        CA_SIG_EMIT(signal);
}

static int user_switch_signal_type(ca_signal_list_t *signal_list_node)  // 返回0：信号匹配到了某个业务．1：没有匹配到任何业务
{
    log_debug("%s", __func__);
    user_signals_worker_list_t *p = g_user_signals_worker_head;
    while (p != NULL)
    { 
        if (!p->func(signal_list_node))  // func返回int类型,0：说明信号类型属于该函数所在的业务模块；1：说明不是该模块的信号
            return 0;
        else
        {
            p = p->next;
            continue;
        }
    }
    return 1;  // 到这里说明没匹配到任何业务
}

// 遍历信号链表 针对信号做相应计算
static void signals_worker()
{
    log_debug("%s", __func__);
    ca_signal_list_t *p = g_signal_list_head;
    while (p != NULL)
    {
        switch (p->type)
        {
            case SIG_AVERAGE_TEMP_OF_N:
                average_temp_of_n_worker(p->sig);
                break;
            case SIG_AVERAGE_COMPARE:
                emit_average_compare(p->sig);
                break;
            case SIG_TEMP_TENDENCY_GENTAL:
                emit_gental_tendency(p->sig);
                break;
            case SIG_TEMP_TENDENCY_RISE:
                emit_rise_tendency(p->sig);
                break;
            case SIG_TEMP_TENDENCY_DOWN:
                emit_down_tendency(p->sig);
                break;
            case SIG_TEMP_TENDENCY_JUMP_RISE:
                emit_jump_rise_tendency(p->sig);
                break;
            case SIG_TEMP_TENDENCY_JUMP_DOWN:
                emit_jump_down_tendency(p->sig);
                break;
            case SIG_TEMP_TENDENCY_SHAKE:
                emit_shake_tendency(p->sig);
                break;
            case SIG_TEMP_TENDENCY_RISE_FF:
                emit_rise_ff_tendency(p->sig);
                break;
            default:
                user_switch_signal_type(p);  // 0：信号匹配到了某个业务．1：没有匹配到任何业务
                break;
        }
        p = p->next;
    }
}

static void enter_apps()
{
    log_debug("%s", __func__);
    buildin_app();

    extern app_enter_func enter_app_funcs[ENTER_APP_FUNC_SIZE];
    int i;
    for (i=0; i<ENTER_APP_FUNC_SIZE; i++)
    {
        enter_app_funcs[i]();
    }
}

static void future_worker()
{
    log_debug("%s", __func__);
    if (g_future_cb_list_head == NULL) return;

    future_cb_list_t *f = g_future_cb_list_head;
    while (f != NULL)
    {
        bool this_f_cb_is_called = false;
        if (g_ca_tick - f->register_tick >= f->delay_times)
        {
            f->cb(f->cb_param);
            this_f_cb_is_called = true;

            // 执行完，就删掉该节点
            if (f == g_future_cb_list_head && f->next == NULL)
            {
                free(f);
                g_future_cb_list_head = NULL;
                break;
            }
            else if (f == g_future_cb_list_head)
            {
                g_future_cb_list_head = f->next;
                f->next->prev = NULL;
                free(f);
                f = g_future_cb_list_head;
            }
            else if (f != g_future_cb_list_head && f->next != NULL)
            {
                future_cb_list_t *p = f;
                f->prev->next = f->next;
                f->next->prev = f->prev;
                p = f->next;
                free(f);
                f = p;
            }
            else if (f != g_future_cb_list_head && f->next == NULL)
            {
                f->prev->next = NULL;
                free(f);
                break;
            }
        }
        if (!this_f_cb_is_called)
            f = f->next;
    }
}

// 按规则检查各个趋势链表的node个数  目前统一清理规则：node数超过30个（1个temp_tendency_info_t node40个字节） 目前不会有影响
static check_tendency_nodes()
{
    log_debug("%s", __func__);
    temp_tendency_info_t *p;
    int common_num = 30;

    if (g_temp_gental_tendency_node_num > common_num)
    {
        p = g_buildin_temp_gental_tendency_head;
        g_buildin_temp_gental_tendency_head->next->prev = NULL;
        g_buildin_temp_gental_tendency_head = g_buildin_temp_gental_tendency_head->next;
        free(p);
        g_temp_gental_tendency_node_num--;
    }

    if (g_temp_rise_tendency_node_num > common_num)
    {
        p = g_buildin_temp_rise_tendency_head;
        g_buildin_temp_rise_tendency_head->next->prev = NULL;
        g_buildin_temp_rise_tendency_head = g_buildin_temp_rise_tendency_head->next;
        free(p);
        g_temp_rise_tendency_node_num--;
    }

    if (g_temp_down_tendency_node_num > common_num)
    {
        p = g_buildin_temp_down_tendency_head;
        g_buildin_temp_down_tendency_head->next->prev = NULL;
        g_buildin_temp_down_tendency_head = g_buildin_temp_down_tendency_head->next;
        free(p);
        g_temp_down_tendency_node_num--;
    }

    if (g_temp_jump_rise_tendency_node_num > common_num)
    {
        p = g_buildin_temp_jump_rise_tendency_head;
        g_buildin_temp_jump_rise_tendency_head->next->prev = NULL;
        g_buildin_temp_jump_rise_tendency_head = g_buildin_temp_jump_rise_tendency_head->next;
        free(p);
        g_temp_jump_rise_tendency_node_num--;
    }

    if (g_temp_jump_down_tendency_node_num > common_num)
    {
        p = g_buildin_temp_jump_down_tendency_head;
        g_buildin_temp_jump_down_tendency_head->next->prev = NULL;
        g_buildin_temp_jump_down_tendency_head = g_buildin_temp_jump_down_tendency_head->next;
        free(p);
        g_temp_jump_down_tendency_node_num--;
    }

    if (g_temp_shake_tendency_node_num > common_num)
    {
        p = g_buildin_temp_shake_tendency_head;
        g_buildin_temp_shake_tendency_head->next->prev = NULL;
        g_buildin_temp_shake_tendency_head = g_buildin_temp_shake_tendency_head->next;
        free(p);
        g_temp_shake_tendency_node_num--;
    }

    if (g_temp_rise_ff_tendency_node_num > common_num)
    {
        p = g_buildin_temp_rise_ff_tendency_head;
        g_buildin_temp_rise_ff_tendency_head->next->prev = NULL;
        g_buildin_temp_rise_ff_tendency_head = g_buildin_temp_rise_ff_tendency_head->next;
        free(p);
        g_temp_rise_ff_tendency_node_num--;
    }
}

// #include <time.h>
void signal_module_start(unsigned short t)
{
    log_debug("%s", __func__);
    g_ca_tick++;
    // 记录初始温度 和 最近温度
    if (g_first_tick_temp == 0)
        g_first_tick_temp = t;
    g_last_tick_temp = t;


    signals_worker();  // 执行所有信号指定的信号逻辑以及触发回调
    enter_apps(); // 进入各个app，执行添加信号等初始化逻辑
    
    // clock_t start, end;
    // double time_used;
    // start = clock();
    future_worker();
    // end = clock();
    // log_debug("future使用时间: %f", ((double)(end - start)) / CLOCKS_PER_SEC);

    check_tendency_nodes();  // 按规则检查各个趋势链表的node个数，防止过度占用运存  用{230, 450, 640, 720, 950, 1900, 1000, 450}这样的温度测试时，调降、跳升节点过多，导致内存耗尽

    // init_temp_cells();
    // if (add_temp_to_node(t))
    //     return;
    // int i, j;
    // for (i = 0; i < g_temp_cell_valid_size; i++)
    // {
    //     if (g_temp_cells[i].cell_temp_num == 0)
    //     {
    //         continue;
    //     }
    //     log_debug("区间总个数: %-5d, ", g_temp_cells[i].cell_temp_num);
    //     for (j = 0; j < TEMP_INTERVAL / 10; j++)  // 除以10，是回归正常个数
    //     {
    //         log_debug("|温度：%-5d, 个数: %-5d|", g_temp_cells[i].temp_num_dict[j].temp, g_temp_cells[i].temp_num_dict[j].num);
    //         // log_debug("wath!!");
    //     }
    //     log_debug("");
    // }
}