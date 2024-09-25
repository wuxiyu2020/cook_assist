
#ifndef _CA_APPS_H_
#define _CA_APPS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define VAR_NAME(name)  (#name)

#include "ca_signals_lib.h"
#include "buildin_signals.h"
#include "app_pot_cover/pot_cover.h"
#include "app_water_boil/water_boil.h"
#include "app_quick_cooking/quick_cooking.h"

#define INPUT_DATA_HZ (4)
#define ENTER_APP_FUNC_SIZE 3
#define RESET_APP_FUNC_SIZE 3

typedef void (*app_enter_func)();
typedef void (*app_reset_func)();

enum INPUT_DIR  // 这个原本在cook_wrapper.h里面，移到这里
{
    INPUT_LEFT = 0, // 左灶
    INPUT_RIGHT,    // 右灶
    INPUT_MAX
};

// 大小火
enum FIRE
{
    FIRE_SMALL = 0,
    FIRE_BIG,
    FIRE_UNKNOW,
};
extern enum FIRE fire_state;

/*　存放所有业务app入口函数声明　以及　提供出来的全局变量和信号　！不要轻易更改其他模块的变量！　*/

/********************************** buildin *************************************/
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

extern unsigned short g_buildin_avg_arr1[BUILDIN_AVG_ARR_SEIZ_1];
extern sig_average_temp_of_n_t *g_buildin_signal_average_temp_of_1;
extern unsigned short g_buildin_avg_arr4[BUILDIN_AVG_ARR_SEIZ_4];
extern sig_average_temp_of_n_t *g_buildin_signal_average_temp_of_4;
extern unsigned short g_buildin_avg_arr10[BUILDIN_AVG_ARR_SEIZ_10];
extern sig_average_temp_of_n_t *g_buildin_signal_average_temp_of_10;
extern sig_temp_gental_tendency_t *g_buildin_signal_temp_gental;
extern sig_temp_rise_tendency_t *g_buildin_signal_temp_rise;
extern unsigned int g_buildin_snatch_rise_slope;
extern sig_temp_down_tendency_t *g_buildin_signal_temp_down;
extern sig_temp_jump_rise_tendency_t *g_buildin_signal_temp_jump_rise;
extern sig_temp_jump_down_tendency_t *g_buildin_signal_temp_jump_down;
extern sig_temp_shake_tendency_t *g_buildin_signal_temp_shake;
extern sig_rise_faster_and_faster_t *g_buildin_signal_temp_rise_ff;

void signal_module_start(unsigned short t);
void reset_signal_module();
/********************************** buildin *************************************/


/********************************** APIS *************************************/
bool api_is_average_compare_meet_rule(sig_average_compare_t *signal);
bool api_is_gental_tendency(sig_temp_gental_tendency_t *signal);
bool api_is_rise_tendency(sig_temp_rise_tendency_t *signal);
bool api_is_down_tendency(sig_temp_down_tendency_t *signal);
bool api_is_jump_rise_tendency(sig_temp_jump_rise_tendency_t *signal);
bool api_is_jump_down_tendency(sig_temp_jump_down_tendency_t *signal);
bool api_is_shake_tendency(sig_temp_shake_tendency_t *signal);
bool api_is_rise_ff_tendency(sig_rise_faster_and_faster_t *signal);
bool api_is_cb_in_future(future_cb_type future_cb);
unsigned int get_latest_avg_temp_of_10();
unsigned int get_latest_avg_temp_of_4();

/// @brief 检查平缓链表里面有没有满足条件的node   其他接口规则一样
/// @param at_least_duration 平稳至少持续的时间 为0时不比较
/// @param at_most_duration 平稳至多持续的时间 为0时不比较
/// @param start_temp_range_floor 节点start_temp温度范围的最低值，为0时不比较
/// @param start_temp_range_ceiling 节点start_temp温度范围的最高值，为0时不比较
/// @param end_temp_range_floor 节点end_temp温度范围的最低值，为0时不比较
/// @param end_temp_range_ceiling 节点end_temp温度范围的最高值，为0时不比较
/// @return bool
bool api_exist_gental_node_duration_and_range(unsigned int at_least_duration, unsigned int at_most_duration, unsigned short start_temp_range_floor, unsigned short start_temp_range_ceiling, unsigned short end_temp_range_floor, unsigned short end_temp_range_ceiling);
bool api_exist_rise_node_duration_and_range(unsigned int at_least_duration, unsigned int at_most_duration, unsigned short start_temp_range_floor, unsigned short start_temp_range_ceiling, unsigned short end_temp_range_floor, unsigned short end_temp_range_ceiling);
bool api_exist_down_node_duration_and_range(unsigned int at_least_duration, unsigned int at_most_duration, unsigned short start_temp_range_floor, unsigned short start_temp_range_ceiling, unsigned short end_temp_range_floor, unsigned short end_temp_range_ceiling);
bool api_exist_shake_node_duration_and_range(unsigned int at_least_duration, unsigned int at_most_duration, unsigned short start_temp_range_floor, unsigned short start_temp_range_ceiling, unsigned short end_temp_range_floor, unsigned short end_temp_range_ceiling);
bool api_exist_ise_ff_node_duration_and_range(unsigned int at_least_duration, unsigned int at_most_duration, unsigned short start_temp_range_floor, unsigned short start_temp_range_ceiling, unsigned short end_temp_range_floor, unsigned short end_temp_range_ceiling);
/********************************** APIS *************************************/


/********************************** pot_cover *************************************/
void app_pot_cover();
extern enum POT_COVER_TYPE g_pot_cover_type;
extern enum POT_COVER_TYPE g_last_judge_cover_type;
extern enum POT_COVER_TYPE g_cover_type_when_water_boil;
extern sig_open_cover_t *g_sig_pot_cover_open_cover;  // 信号命名规则　g:全局　sig:是个信号 pot_cover:app名称　open_cover:信号名称
extern sig_close_cover_t *g_sig_pot_cover_close_cover;
/********************************** pot_cover *************************************/


/********************************** water_boil *************************************/
void app_water_boil();
extern sig_water_boil_t *g_sig_water_boil;
extern unsigned int g_water_boil_tick;
extern unsigned short g_water_boil_temp;
extern unsigned short g_update_water_boil_temp;
extern unsigned int g_update_water_boil_tick;
/********************************** water_boil *************************************/


/********************************** quick_cooking *************************************/
void app_quick_cooking();
int quick_cooking_switch(unsigned short switch_flag, int cook_type, unsigned short seted_time, unsigned short safety_time);  // 这个接口放这里是因为时提供给通讯板调用的  因为cook_wrapper.h include了这个头文件,因此可以直接调用这个接口
extern int (*g_fire_state_cb)(int fire_state, bool is_first_small);
void ca_api_register_fire_state_cb(int (*cb)(int fire_state, bool is_first_small));  // 火的状态回调
extern int (*g_quick_cooking_info_cb)(cook_model_t *cook_model);
void ca_api_register_quick_cooking_info_cb(int (*cb)(cook_model_t *cook_model));          // 一键菜谱信息回调，包含模式、设置时间、剩余倒计时
/********************************** quick_cooking *************************************/


/********************************** 公共对外　原本应该写在cook_wrapper.h中的内容 *************************************/
extern int (*g_fire_gear_cb)(const int gear, enum INPUT_DIR input_dir);  // 火力档位
void ca_api_register_fire_gear_cb(int (*cb)(const int gear, enum INPUT_DIR input_dir));
extern int (*g_close_fire_cb)(enum INPUT_DIR input_dir);  //　关火
void ca_api_register_close_fire_cb(int (*cb)(enum INPUT_DIR input_dir));
extern int (*g_hood_gear_cb)(const int gear);  // 风机档位调节
void ca_api_register_hood_gear_cb(int (*cb)(const int gear));
extern int (*g_hood_light_cb)(const int gear);  // 灯调节
void ca_api_register_hood_light_cb(int (*cb)(const int gear));
/********************************** 公共对外　原本应该写在cook_wrapper.h中的内容 *************************************/

void ca_set_fire(enum FIRE fire, enum INPUT_DIR input_dir);
void ca_close_fire(enum INPUT_DIR input_dir);
void reset_on_off_value();

#ifdef __cplusplus
}
#endif
#endif  // _CA_APPS_H_
