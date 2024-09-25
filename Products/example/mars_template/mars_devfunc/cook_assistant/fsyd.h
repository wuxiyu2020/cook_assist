#ifndef _FSYD_H_
#define _FSYD_H_

#define BURN_DATA_SIZE 100        //保留连续100组数据的
#define BURN_DATA_FREQUENCY 10  //2.5s采集一次温度数据
#define DRYBURN_ENABLE

 //实际机型中采集到的水沸温度小于100℃就定义为100，水沸温度大于100就定义为沸腾时采集到的温度+5度(减少误触)，因为在其他机型行水沸温度有超过100℃的意外情况
#define BOILED_TEMP 105     

#ifdef __cplusplus
extern "C"
{
#endif
#include <stdbool.h>
#include "ring_buffer.h"
#include "cook_wrapper.h"

//#define BOIL_ENABLE             //水开小火
//#define DRYBURN_ENABLE          //防干烧
// #define FIRE_CONFIRM_ENABLE
#define STATE_JUDGE_DATA_SIZE (10)
// #define INPUT_DATA_HZ (4)
//#define ENTER_GENTLE_INIT_VALUE (-1000)

    // 大小火
    // enum FIRE
    // {
    //     FIRE_SMALL = 0,
    //     FIRE_BIG
    // };


    enum DRYBURN_USER
    {
        VOLUME_USER = 0,    //量产用户
        ANGEL_USER = 1,      //天使用户
    };

    // 烟机档位
    enum GEAR
    {
        GEAR_CLOSE = 0,
        GEAR_ONE,
        GEAR_TWO,
        GEAR_THREE,
        GEAR_INVALID = 0xff
    };

    // 移锅小火状态
    enum PAN_FIRE
    {
        PAN_FIRE_CLOSE = 0,
        PAN_FIRE_ERROR_CLOSE,
        PAN_FIRE_START,   // start 和 enter 有什么区别？？？   处于start，此时疑似是移锅小火，需要再判断才能确定是移锅小火，这个确认时间是PAN_FIRE_ENTER_TICK，在这个时间内判断是，就转换到enter状态
        PAN_FIRE_ENTER,
    };
    // 风随烟动状态
    enum STATE_FSYD
    {
        STATE_SHAKE = 0,
        STATE_DOWN_JUMP,
        STATE_RISE_JUMP,
        STATE_RISE_SLOW,
        STATE_DOWN_SLOW,
        STATE_GENTLE,
        STATE_IDLE,
        STATE_PAN_FIRE,
#ifdef BOIL_ENABLE
        STATE_BOIL,
#endif

#ifdef DRYBURN_ENABLE
        STATE_DRYBURN,
#endif
    };
    enum TEMP_VALUE
    {
        PAN_FIRE_LOW_TEMP = 2000,
        PAN_FIRE_HIGH_TEMP = 2300,
        SHAKE_PERMIT_TEMP = 1100,
#ifdef BOIL_ENABLE
        BOIL_LOW_TEMP = 820,
        BOIL_HIGH_TEMP = 1000,
#endif
    };

    enum TICK_VALUE
    {
        START_FIRST_MINUTE_TICK = INPUT_DATA_HZ * 40,

        PAN_FIRE_ENTER_TICK = INPUT_DATA_HZ * 5,
        PAN_FIRE_DOWN_JUMP_EXIT_TICK = INPUT_DATA_HZ * 3,
        PAN_FIRE_RISE_JUMP_EXIT_LOCK_TICK = INPUT_DATA_HZ * 3,
        PAN_FIRE_HIGH_TEMP_EXIT_LOCK_TICK = INPUT_DATA_HZ * 5,
        PAN_FIRE_ERROR_LOCK_TICK = INPUT_DATA_HZ * 40,
        SHAKE_EXIT_TICK = INPUT_DATA_HZ * 20,
        TEMP_CONTROL_LOCK_TICK = INPUT_DATA_HZ * 5,         //辅助控温锁定时间
#ifdef BOIL_ENABLE
        BOIL_START_TICK = INPUT_DATA_HZ * 15,            //进入波动后持续的时间
        BOIL_STOP_TICK = INPUT_DATA_HZ * 90,
#endif
    };

    //烹饪助手状态结构体
    typedef struct
    {
        enum INPUT_DIR input_dir;

        unsigned char pan_fire_switch;
        unsigned char temp_control_switch;
        unsigned char dry_burn_switch;
        //---------------------
        unsigned char last_prepare_state;
        unsigned int last_prepare_state_tick;

        unsigned char pre_pre_state;    //前前一个状态
        unsigned char pre_state;        //前一个状态
        unsigned char state;            // 总状态
        unsigned char shake_status_flag;
        int shake_status_average;
        unsigned char shake_down_flag;
        int after_shake_down_temp;
        int after_shake_down_timestamp;
        //unsigned char fry_no_shake_flag1;    //热锅阶段标志位
        unsigned char heat_pot_flag;    //热锅阶段标志位
        unsigned char high_temp_shake_flag;     //高温翻炒标志位
        // unsigned char fry_no_shake_flag2; 
        // unsigned int  fry_no_shake_timestamp;
        // int fry_no_shake_down_temp;

        unsigned char pan_fire_state;
        unsigned char pan_fire_high_temp_exit_lock_tick; // 高温时，移锅小火退出后，锁定时间
        unsigned char pan_fire_rise_jump_exit_lock_tick; // 跳升退出移锅小火后，锁定时间
        unsigned char pan_fire_first_error;              // 开始一分钟内,移锅小火第一次判断错误
        unsigned int pan_fire_error_lock_tick;           // 移锅小火判断错误，锁定时间
        unsigned int pan_fire_tick;                      // 移锅小火进入判断时间 （这里的时间要理解为时刻，记录确定要小火的那个时刻state_handle->current_tick）
        unsigned char pan_fire_enter_type;               // 移锅小火进入类型 1:翻炒进入 0:其他进入 2:防干烧进入
        unsigned short pan_fire_enter_start_tick;        //进入移锅小火开始时间
        unsigned short pan_fire_close_delay_tick;        //移锅小火延时熄火时间

        unsigned short temp_control_first;
        unsigned char temp_control_lock_countdown; // 控温，锁定时间
        unsigned short temp_control_enter_start_tick;
        unsigned short temp_control_target_value;

        unsigned int shake_permit_start_tick; // 允许翻炒开始的时间
        unsigned int shake_exit_tick;
        unsigned char shake_long;  // 长时间翻炒  翻炒状态持续30秒以上算长时间
        unsigned short shake_exit_average_temp;
        unsigned int shake_exit_timestamp;

        unsigned char ignition_switch;
        unsigned short ignition_switch_close_temp;     //开火前的最新温度
        unsigned short before_ignition_temp_for_panfire;    //专门给移锅小火刚开火时用于判断温度的变量

        unsigned char fire_gear;
        unsigned char hood_gear;
        int current_tick;                               //当前所处状态的运行,tick  当前状态运行了多久
        unsigned int total_tick;
        unsigned int tick_for_dryburn;                  //专门为防干烧时间兜底计时的变量

        unsigned char temp_data_size;                         //ring_buffer中数据量的大小
        unsigned short last_temp;                             // 最新温度
        unsigned short last_temp_average;                     //最新平均温度
        unsigned short last_temp_data[STATE_JUDGE_DATA_SIZE]; // 最新温度数组

        unsigned short state_jump_temp;
        unsigned short state_jump_diff;         

        unsigned short gentle_state_enter_temp;                            //进入平缓的温度
        int max_gentle_average_temp;                    //gentle中的最大平均温度
        unsigned char water_down_flag;                  //炖煮中水位下降的标志位
        unsigned int water_down_timestamp;


#ifdef BOIL_ENABLE
        unsigned char boil_gengle_state;                  //小波动计数
        unsigned char boil_gengle_small_state;            //大波动计数
        unsigned short boil_start_tick;                   //满足某一条件的开始时间
        unsigned short boil_temp_tick;                    //水温满足某一温度条件维持时间
        unsigned char boil_stop_count;
#endif

// #ifdef DRYBURN_ENABLE
        unsigned char dryburn_start_tick;       //干烧判断tick
        unsigned int dryburn_average_temp;               //保存本次的平均值，用于与下次的平均值进行比较
        unsigned int average_difference;                 //平均值的差值
        unsigned char dryburn_judge_time;       //进入防干烧判断的次数
        unsigned int very_very_gentle_temp_longest_clear_timestamp;

        int last_average_temp_arr[BURN_DATA_SIZE];      //平均温度数组
        unsigned short arr_size;            //平均温度数组的当前数据量
        int last_arr_average;               //平均温度数组的平均值
        unsigned int last_prepare_shake_tick;
        unsigned short shake_exit_count;     //翻炒进入gentle的条件计数
        unsigned int enter_gentle_prepare_temp;

        unsigned short dryburn_gentle_temp;                //防干烧平缓温度
        unsigned int dryburn_gentle_tick;       //防干烧平缓时间
        unsigned int max_temp;

        unsigned int judge_flag;
        unsigned int judge_30_temp;
        unsigned int judge_time;
        unsigned int dryburn_fry_uptemp;  // 煎炸干烧时最大上升温度

        unsigned short shake_state_count;
        unsigned short rise_slow_state_count;
        unsigned short rise_jump_state_count;
        unsigned short gentle_state_count;
        unsigned short down_slow_state_count;
        unsigned short down_jump_state_count;
        unsigned char dryburn_state;        //当前的干烧状态 0：未干烧(或干烧退出) 1：已干烧 2:灶具关火干烧退出
        unsigned char dryburn_user_category;        //防干烧用户类别：0：量产用户 1:天使用户
        unsigned char dryburn_reason;       //干烧原因 0：未干烧    1：温度兜底触发 2：时间兜底触发 3：平缓上升触发(包括炖煮干烧和炒后干烧) 4：小火炖煮触发
        unsigned char fire_status;          //0:关火 1:开火
// #endif

        ring_buffer_t ring_buffer;
    } state_handle_t;

    typedef int (*state_func_def)(unsigned char prepare_state, state_handle_t *state_handle);

    //烟机档位结构体
    typedef struct
    {
        unsigned char work_mode;
        unsigned char smart_smoke_switch;
        unsigned char lock;
        unsigned char gear;                 //当前的烟机档位
        unsigned char min_gear;
        unsigned char prepare_gear;         //预切换档位（时间不满足时设置此档位）
        unsigned int gear_tick;             //上一次档位切换时间
        unsigned int close_delay_tick;
        unsigned int delayoff_time_s;
    } state_hood_t;

    void cook_assistant_reinit(state_handle_t *state_handle);
    state_handle_t *get_input_handle(enum INPUT_DIR input_dir);
    state_hood_t *get_hood_handle();
    char *get_current_time_format(void);
        void set_fire_gear(unsigned char fire_gear, state_handle_t *state_handle, const unsigned char type);

#ifdef __cplusplus
}
#endif
#endif
