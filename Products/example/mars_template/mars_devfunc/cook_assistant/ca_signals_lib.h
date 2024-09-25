#ifndef _CA_SIGNALS_LIB_H_
#define _CA_SIGNALS_LIB_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>
#include <stdbool.h>

#define PRODUCTION  // 生产环境(真机)打开    用测试程序跑就关闭

#ifdef PRODUCTION
    #include "../mars_devmgr.h"
#endif

#ifdef PRODUCTION
    #include <aos/log.h>
#else
    #include "ca_test/log.h"
#endif

#define TEMP_INTERVAL    (5 * 10)  // 0-300 度之间，每隔n度建立一个cell   乘10是因为进入的温度 *10了
#define TEMP_CELL_SIZE    (300 * 10 / TEMP_INTERVAL)
#define AMEND_NUM    (2 * 10)  // 修正温度数值

// 温度变化趋势
enum TENDENCY
{
    TDC_GENTAL = 0,     // 平稳
    TDC_RISE,           // 上升趋势
    TDC_DOWN,           // 下降趋势
    TDC_JUMP_RISE,      // 跳升
    TDC_JUMP_DOWN,      // 跳降
    TDC_SHAKE,          // 上下波动
    TDC_RISE_FF,        // 上升越来越快
    TDC_RISE_SS,        // 上升越来越慢
    TDC_DOWN_FF,        // 下降越来越快
    TDC_DOWN_SS,        // 下降越来越慢
};

// 各种信号
enum CA_SIGNALS
{
    // 内置
    SIG_AVERAGE_TEMP_OF_N = 0,      // 每多少个温度计算一次平均值
    SIG_AVERAGE_COMPARE,            // 利用平均值数组对比信号
    SIG_TEMP_TENDENCY_GENTAL,       // 温度平稳趋势信号
    SIG_TEMP_TENDENCY_RISE,         // 温度上升趋势信号
    SIG_TEMP_TENDENCY_DOWN,         // 温度下降趋势信号
    SIG_TEMP_TENDENCY_JUMP_RISE,    // 温度跳升
    SIG_TEMP_TENDENCY_JUMP_DOWN,    // 温度跳降
    SIG_TEMP_TENDENCY_SHAKE,        // 温度波动趋势信号
    SIG_TEMP_TENDENCY_RISE_FF,               // 温度上升越来越快
    SIG_TEMP_TENDENCY_RISE_SS,               // 温度上升越来越慢
    SIG_TEMP_TENDENCY_DOWN_FF,               // 温度下降越来越快
    SIG_TEMP_TENDENCY_DOWN_SS,               // 温度下降越来越慢

    /* 其他业务功能信号　　从0xff开始 */
    // pot_cover
    SIG_POT_COVER_OPEN_COVER = 0xff,
    SIG_POT_COVER_CLOSE_COVER,

    // 水开小火
    SIG_WATER_BOIL,
    
};

// 比较中的算数类型
enum CALCULATE
{
    CALC_MINUS = 0,  // 直接做减法
    CALC_ABS,        // 做减法后再取绝对值
};

// 对比条件关系类型
enum RULE_REL
{
    AND = 0,
    OR,
};

// 比较类型
enum COMPARE
{
    COMPARE_GTE = 0,    // 大于等于
    COMPARE_LTE,        // 小于等于
    COMPARE_GT,         // 大于
    COMPARE_LT,         // 小于
    COMPARE_E           // 等于
};

#define CA_SYMBOL(compare)                     \
    (compare == COMPARE_GTE) ? ">=" :       \
    (compare == COMPARE_LTE) ? "<=" :       \
    (compare == COMPARE_GT) ? ">" :         \
    (compare == COMPARE_LT) ? "<" : "="

#define CA_ABS(calcu)                          \
    calcu == CALC_ABS ? "abs" : ""

typedef union future_cb_param future_cb_param_t;
typedef void (*future_cb_type)(future_cb_param_t *param);
typedef void (*void_void_func)(void);  // 只支持void void的回调
// typedef void (*void_temp_tendency_info)(temp_tendency_info_t *tdc_info);
typedef struct ca_signal_list ca_signal_list_t;
typedef int (*user_signals_worker_func)(ca_signal_list_t *p);

/* 单个温度突然上升/下降n度说明：
* state_range_floor、state_range_ceiling、changed_num都为0时，信号不生效；
* state_range_floor、state_range_ceiling同时为0时，范围条件就不生效，只监控changed_num;
* 三个都不为0时，监控在范围内上升/下降了changed_num
*/
typedef struct sigle_temp_change
{
    unsigned short range_floor;  // 要求状态发生在某一个范围的最小值
    unsigned short range_ceiling;  // 要求状态发生在某一个范围的最大值
    unsigned short changed_num;  // 具体上升/下降的数值
    enum CA_SIGNALS signal;

    struct sigle_temp_change *next;
} single_temp_change_t;  // 貌似这个结构体只适用于上升下降 其他状态要另外定义结构体

typedef struct signal_slope_change
{
    unsigned short range_floor;  // 要求状态发生在某一个范围的最小值
    unsigned short range_ceiling;  // 要求状态发生在某一个范围的最大值
    unsigned short changed_num;  // 具体上升/下降的数值
    enum CA_SIGNALS signal;

    struct signal_slope_change *next;
} signal_slope_change_t; 

// typedef struct temp_cell temp_cell_t;  // 用到后面的结构体，需要先声明
// typedef struct temp_list
// {
//     temp_cell_t *belong_cell;
//     unsigned short first_in_temp;
//     unsigned short last_in_temp;
//     unsigned int first_in_tick;
//     unsigned int last_in_tick;
//     unsigned int ticks;
//     int slope;
    
//     struct temp_list *next;
//     struct temp_list *prev;
//     struct temp_list *cell_next;
//     struct temp_list *cell_prev;

// } temp_list_t;

// typedef struct temp_num_dict  // 某个温度出现的次数
// {
//     unsigned short temp;
//     unsigned int num; 
// } temp_num_dict_t;

// typedef struct temp_cell
// {
//     unsigned short temp_start;
//     unsigned short temp_end;
//     unsigned int cell_temp_num;  // 当前范围内温度出现的个数
//     temp_num_dict_t temp_num_dict[TEMP_INTERVAL];  // 因为每个区间都有TEMP_INTERVAL个不同的温度值
//     // temp_list_t *cell_temp_list_tail;
// } temp_cell_t;

// 每隔n个温度计算一次平均温度   要获得连续的温度数据的话，只要把interval设置为1即可
typedef struct sig_average_temp_of_n  //　每新增一个平均值就会触发信号
{
    unsigned short interval;  // 间隔多少个温度算一次平均值  一般间隔也不用超过127,因此用char类型
    unsigned short arr_size;  // 保留多少个平均值  不能超过32个，因为后面需要计算平均值之间的差值 由于算法是通过位来实现哪个跟哪个平均值比较 为了防止位过多出现意外  而且一般32个平均数足够了
    unsigned short valid_size;  // 当前多少个平均值了
    int sum;  // 每个平均值计算需要的累计和 g_ticks % interval 不为0时就把温度累加到sum，为0时，就把sum添加到avg_arr，然后置为0
    unsigned short *avg_arr;  // 数组指针  因为是可变数组，因此用指针
    enum CA_SIGNALS type;

    struct sig_average_temp_of_n *next;
} sig_average_temp_of_n_t;

typedef struct average_compare_rule  // 平均数对比规则
{
    unsigned short minuend_index;  // 作为被减数的平均数index，从0开始  因为平均数个数在average_temp_of_n_t中限制了最多32个，所以用char类型
    unsigned short subtrahend_index;  // 作为减数的平均数index
    short compare_target;  // 被减数 - 减数 得到的结果，根据compare_type来比较
    enum CALCULATE calculate_type;  // 计算类型
    enum COMPARE compare_type;  // 比较类型
} average_compare_rule_t;

typedef struct sig_average_compare  // 平均温度对比   对比规则满足时就会触发信号
{
    sig_average_temp_of_n_t *sig_average_temp_of_n;  // 平均温度所在的信号
    average_compare_rule_t *average_compare_rules_arr;  // 平均数对比的数组，这里面保存了若干个（不大于127个，下面的size类型是char）对比规则，满足所有规则时，就会触发信号
    unsigned short average_compare_rules_arr_size;
    enum CA_SIGNALS type;
    enum RULE_REL rule_relation;  // 规则之间的关系　　and or
    
    struct sig_average_compare *next;
} sig_average_compare_t;

typedef struct temp_tendency_info
{
    enum TENDENCY tendency;
    unsigned short tdc_start_avg_temp;  // 趋势开始时平均温度
    unsigned int tdc_start_tick;  // 趋势开始时tick
    unsigned short tdc_end_avg_temp;  //　趋势结束时平均温度　一直在当前趋势时会一直刷新
    unsigned int tdc_end_tick;  // 趋势结束时tick　一直在当前趋势时会一直刷新

    struct temp_tendency_info *prev;
    struct temp_tendency_info *next;
} temp_tendency_info_t;

typedef struct sig_temp_gental_tendency  // 温度平稳趋势　每新增一段平稳趋势，就会触发信号；原来的趋势时间更新时也会触发
{
    sig_average_compare_t *sig_average_compare;  // 用什么比较信号来判断平稳
    enum CA_SIGNALS type;
    bool is_set_range;  // 是否要设置范围，true的话会检查温度是否在floor和ceiling内，false的话就不检查了
    unsigned short gental_range_floor;  // 规定平稳发生时的最低温度　　　在范围内才会触发信号
    unsigned short gental_range_ceiling;  // 规定平稳发生时的最高温度
    unsigned int at_least_duration_ticks;  // 至少需要持续多少个tick　　４个tick是一秒
    unsigned short at_most_diff_of_tdc_start_avg_temp_and_tdc_end_avg_temp;  // 一段平稳趋势中，结束温度与开始温度差值最多不超多这个值　设置这个值是为了防止很长一段时间判断成平稳，但实际是阶梯上升的，而且上升了很多度
} sig_temp_gental_tendency_t;

typedef struct sig_temp_rise_tendency  // 温度上升趋势
{
    sig_average_compare_t *sig_average_compare;  // 用什么比较信号来判断平稳
    enum CA_SIGNALS type;
    bool is_set_range;
    unsigned short rise_range_floor;
    unsigned short rise_range_ceiling;
    short at_leaset_rise;  // 在一整轮平均值时间内最少需要上升的温度值
    unsigned short at_most_rise;  // 在一整轮平均值时间内最多上升的温度值
    unsigned int at_least_duration_ticks;  // 满足体这些条件持续的时间至少是这个
} sig_temp_rise_tendency_t;

typedef struct sig_temp_down_tendency  // 温度下降趋势
{
    sig_average_compare_t *sig_average_compare;  // 用什么比较信号来判断平稳
    enum CA_SIGNALS type;
    bool is_set_range;
    unsigned short down_range_floor;
    unsigned short down_range_ceiling;
    unsigned short at_leaset_down;  // 在一整轮平均值时间内最少需要下降的温度值
    unsigned short at_most_down;  // 在一整轮平均值时间内最多下降的温度值
    unsigned int at_least_duration_ticks;  // 上升至少持续这个时间才触发信号回调
} sig_temp_down_tendency_t;

typedef struct sig_temp_jump_rise  // 温度跳升现象
{
    sig_average_compare_t *sig_average_compare;
    enum CA_SIGNALS type;
    bool is_set_range;
    unsigned short jump_rise_range_floor;
    unsigned short jump_rise_range_ceiling;
    unsigned short at_leaset_jump_rise;  // 至少跳升多少
    unsigned short at_most_jump_rise;  // 最多跳升多少
} sig_temp_jump_rise_tendency_t;

typedef struct sig_temp_jump_down  // 温度跳升现象
{
    sig_average_compare_t *sig_average_compare;
    enum CA_SIGNALS type;
    bool is_set_range;
    unsigned short jump_down_range_floor;
    unsigned short jump_down_range_ceiling;
    unsigned short at_leaset_jump_down;
    unsigned short at_most_jump_down;
} sig_temp_jump_down_tendency_t;

typedef struct sig_temp_shake  // 温度大幅波动
{
    sig_average_compare_t *sig_average_compare;
    enum CA_SIGNALS type;
    bool is_set_range;
    unsigned short jump_down_range_floor;
    unsigned short jump_down_range_ceiling;
    unsigned int at_least_duration_ticks;  // 至少持续的时间
    unsigned int at_least_temp_direction_change_times;  // 在当前平均数数组中，温度方向至少变化的次数 v和倒v的个数
    unsigned short at_least_diff_of_max_min;  // 在当前平均数数组中，最大值与最小值至少相差多少
} sig_temp_shake_tendency_t;

typedef struct sig_rise_faster_and_faster  // 温度上升越来越快
{
    sig_average_temp_of_n_t *sig_average_temp_of_n;  // 平均温度所在的信号
    enum CA_SIGNALS type;
    bool is_set_range;
    unsigned short faster_and_faster_range_floor;
    unsigned short faster_and_faster_range_ceiling;
    unsigned short rise_step;  // 平均温度数组上升的步长，以这个步长上升的时间，来前后对比，得到是不是“越来越”
    unsigned int at_most_single_step_cost;  // 单个步长上升时间最多是多少，通过这个排除平稳段，建议这个值为判断平稳的时间
    unsigned int cost_compate_times;  // 通过对比Ｎ次前后花费的时间来判断出＂越来越＂的现象．一般为３
} sig_rise_faster_and_faster_t;

typedef struct ca_signal_list  // 所有信号链表
{
    void *sig;
    enum CA_SIGNALS type;

    struct ca_signal_list *next;
} ca_signal_list_t;

// enum CALLBACK
// {
//     CB_VOID_VOID = 0,
//     CB_BOID_TEMP_TENDENCY_INFO,
// };

// typedef union callback_type
// {
//     void_void void_void_cb;
//     void_temp_tendency_info void_temp_tendency_info_cb;
// } callback_type_t;

typedef struct signal_cb_list  // 信号与回调函数对应关系链表
{
    void *sig;
    void_void_func cb;

    struct signal_cb_list *next;
} signal_cb_list_t;

typedef struct user_signals_worker_list
{
    user_signals_worker_func func;
    struct user_signals_worker_list *next;
} user_signals_worker_list_t;

typedef union future_cb_param  // 作为协程回调函数的参数,参数类型可以多种
{
    temp_tendency_info_t *tendency_node;
    unsigned int tick;
    unsigned short temp;
} future_cb_param_t;

typedef void (*future_func)(future_cb_param_t *f);
typedef struct future_cb_list  // ＂协程＂链表
{
    unsigned int register_tick;  // 添加到链表时的tick
    unsigned int delay_times;  // 隔多久来执行回调函数cb
    future_cb_param_t *cb_param;
    future_func cb;

    struct future_cb_list *prev;
    struct future_cb_list *next;
} future_cb_list_t;


/* *************************全局变量 声明************************* */ // 头文件中最好不要定义全局变量，声明即可。否则会出现多次定义问题
extern unsigned short g_first_tick_temp;  // 开灶后的第一个温度
extern unsigned short g_last_tick_temp;   // 最新的一个温度
extern unsigned int g_ca_tick;  // 目前进来的温度个数，每进来一个温度就+1

extern ca_signal_list_t *g_signal_list_head;  // 保存所有信号链表的头指针
extern signal_cb_list_t *g_signal_cb_list_head;  // 存放所有信号与回调函数对应关系链表的头指针
extern user_signals_worker_list_t *g_user_signals_worker_head;
extern temp_tendency_info_t *g_buildin_temp_gental_tendency_head;
extern temp_tendency_info_t *g_buildin_temp_gental_tendency_tail;
extern temp_tendency_info_t *g_buildin_temp_rise_tendency_head;
extern temp_tendency_info_t *g_buildin_temp_rise_tendency_tail;
extern temp_tendency_info_t *g_buildin_temp_down_tendency_head;
extern temp_tendency_info_t *g_buildin_temp_down_tendency_tail;
extern temp_tendency_info_t *g_buildin_temp_jump_rise_tendency_head;
extern temp_tendency_info_t *g_buildin_temp_jump_rise_tendency_tail;
extern temp_tendency_info_t *g_buildin_temp_jump_down_tendency_head;
extern temp_tendency_info_t *g_buildin_temp_jump_down_tendency_tail;
extern temp_tendency_info_t *g_buildin_temp_shake_tendency_head;
extern temp_tendency_info_t *g_buildin_temp_shake_tendency_tail;
extern temp_tendency_info_t *g_buildin_temp_rise_ff_tendency_head;
extern temp_tendency_info_t *g_buildin_temp_rise_ff_tendency_tail;
extern future_cb_list_t *g_future_cb_list_head;
extern unsigned short g_temp_gental_tendency_node_num;
extern unsigned short g_temp_rise_tendency_node_num;
extern unsigned short g_temp_down_tendency_node_num;
extern unsigned short g_temp_jump_rise_tendency_node_num;
extern unsigned short g_temp_jump_down_tendency_node_num;
extern unsigned short g_temp_shake_tendency_node_num;
extern unsigned short g_temp_rise_ff_tendency_node_num;
/* *************************全局变量 声明************************* */

/* *************************添加　连接　触发信号宏************************* */
#define CA_ADD_SIGNAL(signal)                                                          \
{                                                                                   \
    /*　先检查信号是否已经注册过了　*/                                                   \
    ca_signal_list_t *p = g_signal_list_head;                                       \
    bool already_add = false;                                                       \
    while (p != NULL)                                                               \
    {                                                                               \
        if (p->sig == (void *)signal)                                               \
        {                                                                           \
            printf("已忽略重复添加信号：type：%d\r\n", signal->type);                   \
            already_add = true;                                                     \
            break;                                                                  \
        }                                                                           \
        p = p->next;                                                                \
    }                                                                               \
    if (!already_add)                                                               \
    {                                                                               \
        ca_signal_list_t *signal_node = malloc(sizeof(ca_signal_list_t));           \
        memset(signal_node, 0, sizeof(ca_signal_list_t));                           \
        signal_node->type = signal->type;                                           \
        signal_node->sig = signal;                                                  \
        if (g_signal_list_head == NULL)                                             \
        {                                                                           \
            g_signal_list_head = signal_node;                                       \
        }                                                                           \
        else                                                                        \
        {                                                                           \
            p = g_signal_list_head;                                                 \
            while (p->next != NULL)                                                 \
            {                                                                       \
                p = p->next;                                                        \
            }                                                                       \
            p->next = signal_node;                                                  \
        }                                                                           \
        signal_node->next = NULL;                                                   \
    }                                                                               \
}

#define CA_SIG_CONNECT(signal, call_back)                                       \
{                                                                               \
    signal_cb_list_t *signal_cb_node = malloc(sizeof(signal_cb_list_t));        \
    memset(signal_cb_node, 0, sizeof(signal_cb_list_t));                        \
    signal_cb_node->cb = call_back;                                             \
    signal_cb_node->sig = signal;                                               \
    if (g_signal_cb_list_head == NULL)                                          \
    {                                                                           \
        g_signal_cb_list_head = signal_cb_node;                                 \
    }                                                                           \
    else                                                                        \
    {                                                                           \
        signal_cb_list_t *p = g_signal_cb_list_head;                            \
        while (p->next != NULL)                                                 \
        {                                                                       \
            p = p->next;                                                        \
        }                                                                       \
        p->next = signal_cb_node;                                               \
    }                                                                           \
    signal_cb_node->next = NULL;                                                \
}

#define CA_SIG_EMIT(signal)                                                            \
{                                                                               \
    signal_cb_list_t *p = g_signal_cb_list_head;                                \
    while (p != NULL)                                                           \
    {                                                                           \
        if (p->sig == (void *)signal)                                           \
        {                                                                       \
            p->cb();                                                            \
            /*这后面不能用break，因为一个信号可能绑定多个回调*/                        \
        }                                                                       \
        p = p->next;                                                            \
    }                                                                           \
}

#define ADD_USER_SIGNAL_WORKER(function)                                                \
{                                                                                       \
    /*　　先检查函数是否已经加过了　*/                                                       \
    user_signals_worker_list_t *p = g_user_signals_worker_head;                         \
    bool already_add = false;                                                           \
    while (p != NULL)                                                                   \
    {                                                                                   \
        if (p->func == (user_signals_worker_func)function)                                                        \
        {                                                                               \
            already_add = true;                                                         \
            break;                                                                      \
        }                                                                               \
        p = p->next;                                                                    \
    }                                                                                   \
    if (!already_add)                                                                   \
    {                                                                                   \
        user_signals_worker_list_t *node = malloc(sizeof(user_signals_worker_list_t));  \
        memset(node, 0, sizeof(user_signals_worker_list_t));                            \
        node->func = function;                                                          \
        if (g_user_signals_worker_head == NULL)                                         \
        {                                                                               \
            g_user_signals_worker_head = node;                                          \
        }                                                                               \
        else                                                                            \
        {                                                                               \
            p = g_user_signals_worker_head;                                             \
            while (p->next != NULL)                                                     \
            {                                                                           \
                p = p->next;                                                            \
            }                                                                           \
            p->next = node;                                                             \
        }                                                                               \
        node->next = NULL;                                                              \
    }                                                                                   \
}

#define CA_FUTURE(delay, function, param)                                                  \
{                                                                                       \
    /*　　先检查函数是否已经加过了　*/                                                       \
    future_cb_list_t *p = g_future_cb_list_head;                                        \
    bool already_add = false;                                                           \
    while (p != NULL)                                                                   \
    {                                                                                   \
        if (p->cb == (future_func)function)                                                          \
        {                                                                               \
            already_add = true;                                                         \
            break;                                                                      \
        }                                                                               \
        p = p->next;                                                                    \
    }                                                                                   \
    if (!already_add)                                                                   \
    {                                                                                   \
        future_cb_list_t *node = malloc(sizeof(future_cb_list_t));                      \
        memset(node, 0, sizeof(future_cb_list_t));                                      \
        node->cb = function;                                                            \
        node->register_tick = g_ca_tick;                                                   \
        node->delay_times = delay;                                                      \
        node->cb_param = param;                                                         \
        if (g_future_cb_list_head == NULL)                                              \
        {                                                                               \
            g_future_cb_list_head = node;                                               \
            node->prev = NULL;                                                          \
        }                                                                               \
        else                                                                            \
        {                                                                               \
            p = g_future_cb_list_head;                                                  \
            while (p->next != NULL)                                                     \
            {                                                                           \
                p = p->next;                                                            \
            }                                                                           \
            p->next = node;                                                             \
            node->prev = p;                                                             \
        }                                                                               \
        node->next = NULL;                                                              \
    }                                                                                   \
}
/* *************************添加　连接　触发信号宏************************* */

#ifdef __cplusplus
}
#endif
#endif  // _CA_SIGNALS_LIB_H_
