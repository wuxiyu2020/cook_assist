/*
 * @Date: 2023-02-27 16:58:40
 * @LastEditors: Zhouxc
 * @LastEditTime: 2024-06-19 10:38:32
 * @FilePath: /now/Products/example/mars_template/mars_devfunc/cook_assistant/fsyd.c
 * @Description: Do not edit
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>

#include "cloud.h"
#include "fsyd.h"
#include "ca_apps.h"

#define log_module_name "fsyd"
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)
#define log_info(...)  LOGI(log_module_name, ##__VA_ARGS__)
#define log_warn(...)  LOGW(log_module_name, ##__VA_ARGS__)
#define log_error(...) LOGE(log_module_name, ##__VA_ARGS__)
#define log_fatal(...) LOGF(log_module_name, ##__VA_ARGS__)

static void (*oil_temp_cb)(const unsigned short, enum INPUT_DIR);
void register_oil_temp_cb(void (*cb)(const unsigned short, enum INPUT_DIR))
{
    oil_temp_cb = cb;
}

static char *state_info[] = {"shake", "down_jump", "rise_jump", "rise_slow", "down_slow", "gentle", "idle", "pan_fire"
#ifdef BOIL_ENABLE
                             ,
                             "boil"
#endif

#ifdef DRYBURN_ENABLE
                            ,
                            "dry_burn"
#endif

};
//两次风机档位切换要满足的延时
static unsigned char g_gear_delay_time = INPUT_DATA_HZ * 2;

static state_handle_t g_state_func[2];
static state_hood_t state_hood = {0};

char *get_current_time_format(void)
{
    static char data_time[16];
    time_t t = time(NULL);
    struct tm *cur_tm = localtime(&t);
    sprintf(data_time, "%d:%d:%d", cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec);
    return data_time;
}
state_hood_t *get_hood_handle()
{
    return &state_hood;
}

state_handle_t *get_input_handle(enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = NULL;
    if (input_dir == INPUT_LEFT)
    {
        state_handle = &g_state_func[INPUT_LEFT];
    }
    else
    {
        state_handle = &g_state_func[INPUT_RIGHT];
    }
    return state_handle;
}

#ifdef SIMULATION
static char *dispaly_state_info[] = {"翻炒", "跳降", "跳升", "缓升", "缓降", "平缓", "空闲", "移锅小火", "防干烧"};
static char dispaly_msg[33];
static char *fire_info[] = {"小火", "大火"};
int get_fire_gear(char *msg, enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    if (msg != NULL)
        strcpy(msg, fire_info[state_handle->fire_gear]);
    return state_handle->fire_gear;
}
int get_hood_gear(char *msg)
{
    if (msg != NULL)
        strcpy(msg, dispaly_msg);
    return state_hood.gear;
}
int get_cur_state(char *msg, enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    if (msg != NULL)
        strcpy(msg, dispaly_state_info[state_handle->state]);
    return state_handle->state;
}
#endif

static int (*close_fire_cb)(enum INPUT_DIR input_dir);
void register_close_fire_cb(int (*cb)(enum INPUT_DIR input_dir))
{
    close_fire_cb = cb;
}
static int (*fire_gear_cb)(const int gear, enum INPUT_DIR input_dir);
void register_fire_gear_cb(int (*cb)(const int gear, enum INPUT_DIR input_dir))
{
    fire_gear_cb = cb;
}
static int (*hood_gear_cb)(const int gear);

//注册烟机档位切换回调
void register_hood_gear_cb(int (*cb)(const int gear))
{
    hood_gear_cb = cb;
}

static int (*hood_light_cb)(const int gear);
//临时注册烟机照明回调，使用烟机照明关闭来表示防干烧关火
void register_hood_light_cb(int (*cb)(const int gear))
{
    hood_light_cb = cb;
}

static int (*thread_lock_cb)();
void register_thread_lock_cb(int (*cb)())
{
    thread_lock_cb = cb;
}
static int (*thread_unlock_cb)();
void register_thread_unlock_cb(int (*cb)())
{
    thread_unlock_cb = cb;
}
static int (*cook_assist_remind_cb)(int);
void register_cook_assist_remind_cb(int (*cb)(int)) // 0:辅助控温3分钟 1:移锅小火3分钟
{
    cook_assist_remind_cb = cb;
}

static int (*cook_dryburn_remind_cb)(int,int);
/**
 * @description: 防干烧状态回调函数
 * @return {*}
 */
void register_dryburn_remind_cb(int(* cb)(int,int))
{
    cook_dryburn_remind_cb = cb;
}

static int(*close_beeper_cb)(int);
/**
 * @description: 蜂鸣器关闭回调注册函数
 * @return {*}
 */
void register_close_beeper_cb(int(*cb)(int))
{
    close_beeper_cb = cb;
}

/***********************************************************
 * 火焰档位切换函数
 * 1.type:0 无效 type:1 火焰档位同步
 ***********************************************************/
void set_fire_gear(unsigned char fire_gear, state_handle_t *state_handle, const unsigned char type)
{
    log_debug("%s,set_fire_gear:%d", __func__, fire_gear);
    // if(state_handle->fire_gear != fire_gear && type == 2)
    // {
    //   if (fire_gear_cb != NULL)
    //       fire_gear_cb(fire_gear, state_handle->input_dir);
    // }

    if (state_handle->fire_gear != fire_gear || type == 1)
    {
        log_debug("%s,change set_fire_gear:%d type:%d", __func__, fire_gear, type);
        state_handle->fire_gear = fire_gear;
        if (fire_gear_cb != NULL)
            fire_gear_cb(fire_gear, state_handle->input_dir);
    }
}

/***********************************************************
 * 烟机档位切换函数
 * 1.升档降档延时切换
 * 2.type:0 无效 type:1 档位可以从关闭到打开 type:2 更新档位切换时间 type:3 档位马上切换 type:4 档位lock type:5 档位unlock
 ***********************************************************/
static void gear_change(unsigned char gear, const unsigned char type, const char *msg, state_handle_t *state_handle)
{
    log_debug("%s,gear_change gear:%d type:%d msg:%s", __func__, gear, type, msg);
    
    //如果智能排烟开关是关闭的直接返回，无法进行档位调整
    if (state_hood.smart_smoke_switch == 0)
    {
        log_debug("smart_smoke_switch close,not change hood gear");
        return;
    }

    if (thread_lock_cb != NULL)
    {
        if (thread_lock_cb() == 0)
        {
            log_debug("%s,thread_lock_cb fail", __func__);
            goto fail;
        }
    }
    if (state_handle != NULL)
    {
        //将档位赋值给本次传入的（可能是左灶或右灶）结构体成员之后，gear取出左右灶具中最大的档位
        state_handle->hood_gear = gear;

        //取出最大档位作为此次要切换的目标档位
        gear = g_state_func[INPUT_LEFT].hood_gear > g_state_func[INPUT_RIGHT].hood_gear ? g_state_func[INPUT_LEFT].hood_gear : g_state_func[INPUT_RIGHT].hood_gear;
    }

    //解锁操作type = 5
    if (type == 5)
    {
        state_hood.lock = 0;
    }
    //type != 5且处于锁定状态则goto后return
    else if (state_hood.lock)
    {
        log_debug("%s,state_hood lock", __func__);
        goto end;
    }

    //获取当前的烟机档位
    unsigned char last_gear = state_hood.gear;

    //更新档位切换时间
    if (type == 2)
    {
        state_hood.gear_tick = 1;   //更新档位切换时间，时间置为1
    }
    else if (type == 3)
    {
        state_hood.gear_tick = g_gear_delay_time;   //立即切换，时间置为切换延时时间
    }
    else if (type == 4)
    {
        state_hood.lock = 1;          //锁定档位
    }

    if (gear == GEAR_INVALID)
        goto end;
    if (gear < state_hood.min_gear)
    {
        gear = state_hood.min_gear;
    }

    log_debug("hood warning!gear:%d,state_hood.gear:%d,type:%d",gear,state_hood.gear,type);
    //档位未发生变化
    if (last_gear == gear)
    {
        state_hood.prepare_gear = GEAR_INVALID;
    }
    else
    {
        //烟机档位为0，档位切换未锁定，但type不是1（可以从关闭到打开），则无法打开烟机
        if (state_hood.gear == 0 && gear > 0 && type != 1 && type != 4)
        {
            log_debug("%s,Cannot be turned on in off state type:%d", __func__, type);
        }
        else
        {
            if (state_hood.gear_tick >= g_gear_delay_time)
            {
                //符合烟机档位延时时间
                state_hood.gear = gear;
            }
            else
            {
                //时间不满足则设置预切换档位，由预切换任务继续后续的切换工作
                state_hood.prepare_gear = gear;
                log_debug("%s,Prepare the gear:%d", __func__, gear);
            }
        }
    }

    //前面获取的最新档位 和 当前的最新档位不一致，表示上方执行了“符合烟机档位延时时间”的档位切换
    if (last_gear != state_hood.gear)
    {
        state_hood.prepare_gear = GEAR_INVALID;
        state_hood.gear_tick = 1;

        log_debug("%s,Enter the gear:%d", __func__, state_hood.gear);
        
        if (hood_gear_cb != NULL)
        {
            log_debug("回调传参:%d",state_hood.gear);
            hood_gear_cb(state_hood.gear);
        }
    }
#ifdef SIMULATION
    if (msg != NULL && strcmp(msg, ""))
    {
        strcpy(dispaly_msg, msg);
    }
#endif
end:
    if (thread_unlock_cb != NULL)
        thread_unlock_cb();
fail:
    return;
}


//设置点火开关，完成点火或开火时相关变量的设置
void set_ignition_switch(unsigned char ignition_switch, enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    log_debug("%s,set_ignition_switch:%d input_dir:%d", __func__, ignition_switch, input_dir);

    if (!state_hood.work_mode)
        return;
    if (state_handle->ignition_switch == ignition_switch)
        return;
    state_handle->ignition_switch = ignition_switch;
    log_debug("%s,ignition_switch change:%d", __func__, ignition_switch);

    state_handle_t *state_handle_another;
    if (input_dir == INPUT_LEFT)
    {
        state_handle_another = get_input_handle(INPUT_RIGHT);
    }
    else
    {
        state_handle_another = get_input_handle(INPUT_LEFT);
    }

    //开火
    if (ignition_switch)
    {
        // if (hood_gear_cb != NULL && state_handle_another->hood_gear == 0)
        //     hood_gear_cb(0);

        state_hood.close_delay_tick = 0;
        // cook_assistant_reinit(state_handle);
    }
    //关火
    else
    {
        if(state_handle->state == STATE_PAN_FIRE)
        {    
            move_pan_fire(4);                       //关火的时候如果处于移锅小火状态中删除一次移锅标志
        }
        if (state_handle_another->ignition_switch > 0)
        {
            cook_assistant_reinit(state_handle);
            return;
        }
        if (state_handle->total_tick > INPUT_DATA_HZ * 50 * 10 || state_handle->shake_long > 0)
            state_hood.close_delay_tick = INPUT_DATA_HZ * 40;
        else
            state_hood.close_delay_tick = INPUT_DATA_HZ * 20;

        if (state_hood.gear > GEAR_TWO)
        {
            gear_change(2, 3, "set_ignition_switch sw 0", state_handle);
        }
        cook_assistant_reinit(state_handle);
    }
}
void set_hood_min_gear(unsigned char gear)
{
    log_debug("%s,min_gear:%d", __func__, gear);
    if (state_hood.min_gear != gear)
    {
        state_hood.min_gear = gear;
        if (state_hood.gear < state_hood.min_gear)
        {
            state_hood.gear = state_hood.min_gear;
        }
    }
}
void recv_ecb_gear(unsigned char gear)
{
    log_debug("%s,recv ecb gear:%d hood_gear:%d", __func__, gear, state_hood.gear);
    state_hood.gear = gear;
	log_debug("烹饪助手: 更新智能排烟当前烟机档位: %d", state_hood.gear);
}

void recv_ecb_fire(unsigned char fire, enum INPUT_DIR input_dir)
{
    /*
    state_handle_t *state_handle = get_input_handle(input_dir);
    log_debug("%s,recv ecb fire:%d fire_gear:%d pan_fire_switch:%d temp_control_switch:%d", __func__, fire, state_handle->fire_gear, state_handle->pan_fire_switch, state_handle->temp_control_switch);
    if (state_handle->pan_fire_switch || state_handle->temp_control_switch)
    {
        if (fire != state_handle->fire_gear)
        {
            //set_fire_gear(state_handle->fire_gear, state_handle, 1);
        }
    }
    else
    {
#ifndef BOIL_ENABLE
        if (fire != FIRE_BIG)
        {
            set_fire_gear(FIRE_BIG, state_handle, 1);
        }
#endif
    }
    */
}

/***********************************************************
 * 风随烟动翻炒处理函数
 ***********************************************************/
static int state_func_shake(unsigned char prepare_state, state_handle_t *state_handle)
{
    static int shake_temp_count = 0;
    static int shake_total_temp = 0;
    if (state_handle->current_tick == 0)
    {
        state_handle->current_tick = 1;
        state_handle->last_prepare_state = STATE_IDLE;
        state_handle->last_prepare_state_tick = 0;
        log_debug("%s,enter state:%s", __func__, state_info[STATE_SHAKE]);
        state_handle->last_prepare_shake_tick = 0;
        state_handle->shake_exit_count = 0;

        //每次进入shake状态将翻炒后的判断变量清零
        state_handle->shake_down_flag = 0;

        shake_temp_count = 0;
        shake_total_temp = 0;

        state_handle->shake_exit_count = 0;
        state_handle->last_prepare_shake_tick = state_handle->total_tick;

        gear_change(3, 1, state_info[STATE_SHAKE], state_handle);

        //进入翻炒时温度大于200℃认为是高温翻炒，此时可能是移锅操作，在防干烧中认为这不是炒菜操作
        if(state_handle->last_temp_average >= 200*10)
        {
            log_debug("高温进入shake");
            state_handle->high_temp_shake_flag = 1;
        }
        else
        {
            state_handle->high_temp_shake_flag = 0;
        }

        return prepare_state;
    }
    else
    {
        ++state_handle->current_tick;
        ++shake_temp_count;
        shake_total_temp += (state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE-1] / 10);
    }

    if (state_handle->current_tick > INPUT_DATA_HZ * 30)
    {
        state_handle->shake_long = 1;
    }
    if (state_handle->last_prepare_state != prepare_state)
    {
        state_handle->last_prepare_state = prepare_state;
        state_handle->last_prepare_state_tick = state_handle->current_tick;
    }

    //记录 预切换状态是翻炒就一直更新时间戳，清零计数值
    if(state_handle->last_prepare_state == STATE_SHAKE)
    {
        state_handle->shake_exit_count = 0;
        state_handle->last_prepare_shake_tick = state_handle->total_tick;
    }

    log_debug("last_prepare_shake_tick:%d,state_handle->total_tick:%d,shake_exit_tick:%d",state_handle->last_prepare_shake_tick,state_handle->total_tick,state_handle->shake_exit_count);

    //如果计数20S没有再次收到 为翻炒的预切换状态 且 过去20s内收到了60个rise_slow或者gentle就手动切换到gentle状态中去,解决离开翻炒时gentle和rise_slowe互相打断对方而无法离开shake的问题
    if(state_handle->last_prepare_shake_tick + 20*INPUT_DATA_HZ >= state_handle->total_tick)
    {
        if(prepare_state == STATE_GENTLE || prepare_state == STATE_RISE_SLOW)
        {
            state_handle->shake_exit_count++;
        }

        if(state_handle->shake_exit_count >= 60)
        {
            log_debug("gentle和rise_slow互相影响，在此处退出shake");
            prepare_state = STATE_GENTLE;
            goto end;
        }
    }
    else
    {
        state_handle->shake_exit_count = 0;
        state_handle->last_prepare_shake_tick = state_handle->total_tick;
    } 
    

    log_debug("[%s],prepare_state:%s,prepare_state_tick:%d",__func__,state_info[state_handle->last_prepare_state],state_handle->last_prepare_state_tick);
    if (prepare_state == STATE_RISE_SLOW)
    {
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 8 > state_handle->current_tick)
            return STATE_SHAKE;
    }
    else if (prepare_state == STATE_DOWN_SLOW)
    {
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 8 > state_handle->current_tick)
            return STATE_SHAKE;
    }
    else if (prepare_state == STATE_DOWN_JUMP)
    {
        if (state_handle->last_prepare_state_tick + 7 > state_handle->current_tick)
            return STATE_SHAKE;
    }
    else if (prepare_state == STATE_RISE_JUMP)
    {
        if (state_handle->last_prepare_state_tick + 7 > state_handle->current_tick)
            return STATE_SHAKE;
    }
    else if (prepare_state == STATE_GENTLE)
    {
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 12 > state_handle->current_tick)
            return STATE_SHAKE;
    }
    else if (prepare_state == STATE_PAN_FIRE)
    {
        log_debug("%s,%s pan_fire:%d", __func__, state_info[STATE_SHAKE], state_handle->pan_fire_state);
        if (state_handle->pan_fire_state == PAN_FIRE_ERROR_CLOSE)
        {
            return STATE_SHAKE;
        }
        //        else
        //        {
        //            if (state_handle->last_prepare_state_tick + STATE_JUDGE_DATA_SIZE/2 > state_handle->current_tick)
        //                return STATE_SHAKE;
        //        }
    }

    end:
    if (prepare_state != STATE_SHAKE)
    {
        state_handle->shake_exit_tick = state_handle->total_tick;
        state_handle->shake_exit_average_temp = state_handle->last_temp_average;
        state_handle->shake_exit_timestamp = state_handle->total_tick;\
        log_debug("shake_exit_timestamp is:%d",state_handle->shake_exit_timestamp);
        state_handle->shake_status_average = 10*(shake_total_temp/shake_temp_count);
        log_debug("shake_status_average is:%d",state_handle->shake_status_average);
    }

    return prepare_state;
}
/***********************************************************
 * 风随烟动跳降处理函数
 ***********************************************************/
static int state_func_down_jump(unsigned char prepare_state, state_handle_t *state_handle) // 跳降
{
    if (state_handle->current_tick == 0)
    {
        state_handle->current_tick = 1;
        state_handle->last_prepare_state_tick=1;
        log_debug("%s,enter state:%s", __func__, state_info[STATE_DOWN_JUMP]);
        if (prepare_state == STATE_PAN_FIRE)
        {
            return prepare_state;
        }

        if (state_handle->state_jump_temp > 1300)
        {
            gear_change(3, 3, state_info[STATE_DOWN_JUMP], state_handle);
            gear_change(GEAR_INVALID, 2, state_info[STATE_DOWN_JUMP], state_handle);
        }
        else if (state_handle->state_jump_temp < 800)
        {
            gear_change(1, 3, state_info[STATE_DOWN_JUMP], state_handle);
        }
        else
        {
            gear_change(GEAR_INVALID, 2, state_info[STATE_DOWN_JUMP], state_handle);
        }
        return prepare_state;
    }
    else
    {
        ++state_handle->current_tick;
    }

    if (state_handle->last_prepare_state != prepare_state)
    {
        if(prepare_state == STATE_SHAKE)
        {
            log_debug("收到shake_state的tick:%d",state_handle->current_tick);
        }  
        state_handle->last_prepare_state = prepare_state;
        state_handle->last_prepare_state_tick = state_handle->current_tick;
    }
    if (prepare_state == STATE_RISE_SLOW)
    {
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 3 > state_handle->current_tick)
            return STATE_DOWN_JUMP;
    }
    else if (prepare_state == STATE_GENTLE)
    {
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 3 > state_handle->current_tick)
            return STATE_DOWN_JUMP;
    }
    else if(prepare_state == STATE_SHAKE)
    {
        if(state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 2 > state_handle->current_tick)
            return STATE_DOWN_JUMP;
    }
    log_debug("last_repapre_state_tick:%d,current_tick:%d",state_handle->last_prepare_state_tick,state_handle->current_tick);
    return prepare_state;
}
/***********************************************************
 * 风随烟动跳升处理函数
 ***********************************************************/
static int state_func_rise_jump(unsigned char prepare_state, state_handle_t *state_handle)
{
    if (state_handle->current_tick == 0)
    {
        state_handle->current_tick = 1;
        state_handle->last_prepare_state = STATE_IDLE;
        state_handle->last_prepare_state_tick = 1;
        log_debug("%s,enter state:%s", __func__, state_info[STATE_RISE_JUMP]);
        if (prepare_state == STATE_PAN_FIRE)
        {
            return prepare_state;
        }
        else if (state_handle->state_jump_temp > 700 && state_handle->state_jump_temp < 1500)
        {
            gear_change(3, 1, state_info[STATE_RISE_JUMP], state_handle);
        }
        else if (state_handle->state_jump_temp > 1500)
        {
            if (state_hood.gear < GEAR_TWO)
                gear_change(2, 1, state_info[STATE_RISE_JUMP], state_handle);
        }

        return prepare_state;
    }
    else
    {
        ++state_handle->current_tick;
    }

    if (state_handle->last_prepare_state != prepare_state)
    {
        state_handle->last_prepare_state = prepare_state;
        state_handle->last_prepare_state_tick = state_handle->current_tick;
    }
    
    if(prepare_state == STATE_SHAKE)           //add by Zhouxc0808
    {
        //持续收到20组rise_slow 或者 当前平均温度大于平缓进入温度8℃以上时收到一次rise_slow就立即切换
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 2 > state_handle->current_tick)     //by Zhouxc0808
            return STATE_RISE_JUMP;
    }
    return prepare_state;
}

/***********************************************************
 * 风随烟动缓升处理函数
 ***********************************************************/
static int state_func_rise_slow(unsigned char prepare_state, state_handle_t *state_handle)
{
    static unsigned char before_state = 0;
    if (state_handle->current_tick == 0)
    {
        state_handle->current_tick = 1;
        state_handle->last_prepare_state = STATE_IDLE;
        state_handle->last_prepare_state_tick = 1;
        log_debug("%s,enter state:%s", __func__, state_info[STATE_RISE_SLOW]);
        before_state = prepare_state;
        return prepare_state;
    }
    else
    {
        ++state_handle->current_tick;
    }

    //传入的prepare状态不再是缓升状态直接到end判断
    if (prepare_state != STATE_RISE_SLOW)
    {
        goto end;
    }

    //处于缓升状态前2s内不进行风机档位的判断调整，不判断是否要离开当前状态
    else if (state_handle->current_tick < INPUT_DATA_HZ * 2)
        return STATE_RISE_SLOW;
        
    

    static unsigned short temp_max = 0, temp_min = 40960;

    //最新温度和最早温度的差值diff
    unsigned short diff = state_handle->last_temp - state_handle->last_temp_data[state_handle->temp_data_size - 10];
    if (diff > temp_max)
    {
        temp_max = diff;
    }
    if (diff < temp_min)
    {
        temp_min = diff;
    }
    log_debug("%s,%s rise value:%d max:%d min:%d ", __func__, state_info[STATE_RISE_SLOW], diff, temp_max, temp_min);

    if (state_handle->last_temp >= 700)
    {
        //十组数据温度上升不超过10℃
        if (diff < 100)
        {
            if (state_hood.gear < GEAR_TWO)
                gear_change(2, 1, state_info[STATE_RISE_SLOW], state_handle);
        }
        else           //十组温度数据上升大于等于10℃
        {
            if (state_handle->last_temp >= 1800)
            {
                if (state_hood.gear < GEAR_TWO)
                    gear_change(2, 1, state_info[STATE_RISE_SLOW], state_handle);
            }
            else
            {
                if (state_hood.gear == GEAR_CLOSE)
                {
                    gear_change(1, 1, state_info[STATE_RISE_SLOW], state_handle);
                }
            }
        }
    }
    else if (state_handle->last_temp >= 500)
    {
        if (state_handle->shake_exit_tick != 0 && state_handle->shake_exit_tick + SHAKE_EXIT_TICK > state_handle->total_tick)
        {
            gear_change(2, 1, state_info[STATE_RISE_SLOW], state_handle);
        }
        else
        {
            gear_change(1, 1, state_info[STATE_RISE_SLOW], state_handle);
        }
    }
    // else
    // {
    //     if (state_hood.gear == GEAR_CLOSE)
    //     {
    //         gear_change(1, 1, state_info[STATE_RISE_SLOW], state_handle);
    //     }
    // }
end:
    //此前最新的准备状态不等于传入的准备状态，记录新的准备状态 和 新的准备状态开始时间
    if (state_handle->last_prepare_state != prepare_state)
    {
        state_handle->last_prepare_state = prepare_state;
        state_handle->last_prepare_state_tick = state_handle->current_tick;
    }

    //如果由 缓升 -> 平缓，持续3s传入的prepare都是state_gentle即可返回平缓
    if (prepare_state == STATE_GENTLE)
    {
        //如果收到的平缓持续3s内仍返回state_rise_slow，超过3s会在最后返回平缓
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 3 > state_handle->current_tick)     //by Zhouxingchen持续3s修改为持续5s
        {
            //state_handle->last_temp_average = ENTER_GENTLE_INIT_VALUE;
            log_debug("%s,return STATE_RISE_SLOW",__func__);
            return STATE_RISE_SLOW;
        }
    }
    else if(prepare_state == STATE_SHAKE)           //add by Zhouxc
    {
        if(state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 2 > state_handle->current_tick)
        {
            return STATE_RISE_SLOW;
        }
    }

    log_debug("%s,return perpare_state:%s",__func__,state_info[prepare_state]);

    //如果收到其他状态，直接切换为其他状态
    return prepare_state;
}
/***********************************************************
 * 风随烟动缓降处理函数
 ***********************************************************/
static int state_func_down_slow(unsigned char prepare_state, state_handle_t *state_handle) // 缓降
{
    log_debug("%s",__func__);
    if (state_handle->current_tick == 0)
    {
        log_debug("%s,enter state:%s", __func__, state_info[STATE_DOWN_SLOW]);
        state_handle->current_tick = 1;
        state_handle->last_prepare_state = STATE_IDLE;
        state_handle->last_prepare_state_tick = 1;
        return prepare_state;
    }
    else
    {
        ++state_handle->current_tick;
    }

    // 预切换状态是移锅小火 && (最新一个温度大于280度 || 最新一个温度与最老的温度差值大于50度)
    if (prepare_state == STATE_PAN_FIRE && (state_handle->last_temp >= 2800 || state_handle->last_temp - state_handle->last_temp_data[0] >= 500))
    {
        return prepare_state;
    }

    if (state_handle->current_tick < INPUT_DATA_HZ * 6)
    {
        return STATE_DOWN_SLOW;
    }
    
    if (prepare_state != STATE_DOWN_SLOW)
    {
        goto end;
    }
   

    if (state_handle->last_temp >= 650)
    {
        if (state_hood.gear > GEAR_TWO)
        {
            gear_change(2, 0, state_info[STATE_DOWN_SLOW], state_handle);
        }
    }
    else
    {
        gear_change(1, 0, state_info[STATE_DOWN_SLOW], state_handle);
    }

    end:
    //此前最新的准备状态不等于传入的准备状态，记录新的准备状态 和 新的准备状态开始时间
    if (state_handle->last_prepare_state != prepare_state)
    {
        state_handle->last_prepare_state = prepare_state;
        state_handle->last_prepare_state_tick = state_handle->current_tick;
    }

    if (prepare_state == STATE_SHAKE)
    {
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 1 > state_handle->current_tick)     //add by Zhouxc 防止炒菜加油误入shake翻炒
        {
            return STATE_DOWN_SLOW;
        }
    }
    return prepare_state;
}
/***********************************************************
 * 风随烟动平缓处理函数
 ***********************************************************/
static int state_func_gentle(unsigned char prepare_state, state_handle_t *state_handle) // 平缓
{
    static unsigned char before_state = 0;
    // static int enter_gentle_average_temp = 0;
    //static unsigned int boiled_time = 0;
   
    if (state_handle->current_tick == 0)
    {
        state_handle->gentle_state_enter_temp = state_handle->last_temp_average;
        log_debug("%s,enter state:%s", __func__, state_info[STATE_GENTLE]);
        state_handle->current_tick = 1;
        state_handle->last_prepare_state = STATE_IDLE;
        state_handle->last_prepare_state_tick = 1;

        before_state = prepare_state;

        //boiled_time = 0;
        log_debug("[%s],before state is:%s",__func__,state_info[before_state]);

        // enter_gentle_average_temp = state_handle->last_temp_average;
        // state_handle->enter_gentle_prepare_temp = ENTER_GENTLE_INIT_VALUE;

#ifdef BOIL_ENABLE
        state_handle->boil_start_tick = 0;
        state_handle->boil_stop_count = 0;
#endif

#ifdef DRYBURN_ENABLE
        //state_handle->dryburn_start_tick = 0;
        //state_handle->dryburn_judge_time = 0;
#endif
        return prepare_state;
    }
    else
    {
        ++state_handle->current_tick;
        //++state_handle->dryburn_start_tick;
    }

    #ifdef DRYBURN_ENABLE
    
    #endif
    
    //如果传入的准备状态不等于当前状态，goto end进行处理
    if (prepare_state != STATE_GENTLE)
    {
        goto end;
    }
    else if (state_handle->current_tick < INPUT_DATA_HZ * 3)
        return STATE_GENTLE;


    int average = 0;
    // int min = state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE-1];
    // int max = state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE-1];
    
 

    if (state_handle->state_jump_temp >= 650 && state_handle->state_jump_temp < 1200)
    {
        gear_change(2, 1, state_info[STATE_GENTLE], state_handle);
    }
    else if (state_handle->state_jump_temp < 600)
    {
        if (state_handle->shake_exit_tick != 0 && state_handle->shake_exit_tick + SHAKE_EXIT_TICK > state_handle->total_tick)
        {
            gear_change(2, 0, state_info[STATE_GENTLE], state_handle);
        }
        else
        {
            gear_change(1, 0, state_info[STATE_GENTLE], state_handle);
        }
    }
    else
    {
    }
#ifdef BOIL_ENABLE
    if (state_handle->state_jump_temp > BOIL_LOW_TEMP && state_handle->state_jump_temp <= BOIL_HIGH_TEMP)       //符合水开的大范围温度
    {
        if ((state_handle->boil_gengle_state >= 2 && state_handle->boil_gengle_state <= 10))            //表示大波动
        {
            if (state_handle->boil_start_tick == 0)             //是否首次进入大波动
            {
                state_handle->boil_start_tick = state_handle->current_tick;
                state_handle->boil_stop_count = 1;
            }
        }
        else if ((state_handle->boil_gengle_small_state >= 8 && state_handle->boil_gengle_small_state <= 10))       //表示小波动
        {
            if (state_handle->boil_start_tick == 0)               //是否首次进入小波动
            {
                state_handle->boil_start_tick = state_handle->current_tick;
            }
        }
        else
        {
            if (state_handle->boil_stop_count == 0)
                state_handle->boil_start_tick = 0;
            if (state_handle->boil_stop_count > 0)
            {
                --state_handle->boil_stop_count;
            }
        }
    }
    else
    {
        state_handle->boil_stop_count = 0;
        state_handle->boil_start_tick = 0;
    }

    log_debug("-----------------%s,%s current_tick:%d boil_start_tick:%d boil_gengle_state:%d boil_gengle_small_state:%d", __func__, state_info[STATE_GENTLE], state_handle->current_tick, state_handle->boil_start_tick, state_handle->boil_gengle_state, state_handle->boil_gengle_small_state);
    if (state_handle->boil_start_tick > 0 && state_handle->current_tick >= state_handle->boil_start_tick + BOIL_START_TICK)     //进入波动9s返回水开状态
    {
        state_handle->boil_start_tick = 0;
       log_debug("success boiled water 1");
        return STATE_BOIL;
    }
    // //如果温度小于90℃，将temp计时器清零；若大于90赋值为当前时间表示开始计数
    // if(state_handle->state_jump_temp > 850)
    // {
    //     if(state_handle->boil_temp_tick == 0)
    //         state_handle->boil_temp_tick = state_handle->current_tick;
    // }
    // else        //90℃以下一直置为0
    // {
    //     state_handle->boil_temp_tick = 0;
    // }

    // //如果温度大于90℃，且维持30秒，表示水开，返回STATE_BOIL
    // if(state_handle->state_jump_temp > 850 && state_handle->current_tick > state_handle->boil_temp_tick+INPUT_DATA_HZ * 30)
    // {
    //    log_debug("success boiled water 2");
    //     state_handle->boil_temp_tick = 0;
    //     return STATE_BOIL;
    // }
#endif
end:        
    // if(abs(state_handle->gentle_state_enter_temp - state_handle->last_temp_average) >= 3)
    // {
    //     log_debug("[%s],jump too much compared to enter temp,return preapare state",__func__);
    //     log_debug("[%s],gentle_state_enter_temp:%d,last_temp_average:%d",__func__,state_handle->gentle_state_enter_temp,state_handle->last_temp_average);
    //      return prepare_state;
    // }

    //传入的准备状态不等于此前最新的准备状态，就对该状态进行更新 同时更新当前更新最新准备状态的时间
    if (state_handle->last_prepare_state != prepare_state)
    {
        log_debug("最新传入的prepare_state是:%s",state_info[prepare_state]);
        state_handle->last_prepare_state = prepare_state;
        state_handle->last_prepare_state_tick = state_handle->current_tick;
    }
    if (prepare_state == STATE_RISE_SLOW)
    {
        //持续收到20组rise_slow 或者 当前平均温度大于平缓进入温度8℃以上时收到一次rise_slow就立即切换
        if ((state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 5 > state_handle->current_tick) \
        /*&& (enter_gentle_average_temp + 80 >= state_handle->last_temp_average)*/)     //by Zhouxc
            return STATE_GENTLE;
    }
    else if (prepare_state == STATE_DOWN_SLOW)
    {
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 10 > state_handle->current_tick)
            return STATE_GENTLE;
    }
    else if(prepare_state == STATE_SHAKE)           //add by Zhouxc
    {
        if(state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 2 > state_handle->current_tick)
        {
            return STATE_GENTLE;
        }
    }


   
    return prepare_state;
}

/***********************************************************
 * 风随烟动空闲处理函数
 ***********************************************************/
static int state_func_idle(unsigned char prepare_state, state_handle_t *state_handle) // 空闲
{
    if (state_handle->current_tick == 0)
    {
        log_debug("%s,enter state:%s", __func__, state_info[STATE_IDLE]);
        state_handle->current_tick = 1;
    }
    return prepare_state;
}
extern void move_pan_fire(int type);
static int state_func_pan_fire(unsigned char prepare_state, state_handle_t *state_handle)
{
    if (!state_handle->pan_fire_switch || state_handle->temp_control_switch)
    {
        goto exit;
    }

    if (state_handle->current_tick == 0)
    {
        log_debug("%s,enter state:%s", __func__, state_info[STATE_PAN_FIRE]);
        state_handle->current_tick = 1;
        state_handle->last_prepare_state = STATE_IDLE;
        state_handle->last_prepare_state_tick = 0;
#ifdef FIRE_CONFIRM_ENABLE
        state_handle->pan_fire_state = PAN_FIRE_CLOSE;
#else
        state_handle->pan_fire_state = PAN_FIRE_ENTER;
        LOGI("mars", "开始关闭外环火(PAN_FIRE_START)");
        move_pan_fire(1);
        set_fire_gear(FIRE_SMALL, state_handle, 0);
#endif
        state_handle->pan_fire_tick = 0;

        return prepare_state;
    }
    else
    {
        ++state_handle->current_tick;
    }
    log_debug("%s,%s pan_fire_state:%d pan_fire_tick:%d current_tick:%d", __func__, state_info[STATE_PAN_FIRE], state_handle->pan_fire_state, state_handle->pan_fire_tick, state_handle->current_tick);

    if (state_handle->pan_fire_state == PAN_FIRE_CLOSE) // 假设移锅小火
    {
        log_debug("%s,%s %s", __func__, state_info[STATE_PAN_FIRE], "set small fire");
        LOGI("mars", "开始关闭外环火(PAN_FIRE_CLOSE)");
        move_pan_fire(1);
        set_fire_gear(FIRE_SMALL, state_handle, 0);
        if (state_handle->total_tick < INPUT_DATA_HZ * 20)
        {
            state_handle->pan_fire_tick = state_handle->current_tick + PAN_FIRE_DOWN_JUMP_EXIT_TICK / 2;
            state_handle->pan_fire_state = PAN_FIRE_ENTER;
        }
        else
        {
            state_handle->pan_fire_tick = state_handle->current_tick;
            state_handle->pan_fire_state = PAN_FIRE_START;
        }
    }
    else if (state_handle->pan_fire_state == PAN_FIRE_ENTER) // 开关小火，温度跳降，确定是移锅小火
    {
        if (state_handle->pan_fire_enter_start_tick < INPUT_DATA_HZ * state_hood.delayoff_time_s)   //modify by zhoubw
        {
            ++state_handle->pan_fire_enter_start_tick;
            if (state_handle->pan_fire_enter_start_tick == INPUT_DATA_HZ * state_hood.delayoff_time_s)  //modify by zhoubw
            {
                if (cook_assist_remind_cb != NULL)
                {
                    move_pan_fire(3);
                    cook_assist_remind_cb(1);
                }
            }
        }
        if (state_handle->pan_fire_tick == 0)
        {
            state_handle->pan_fire_tick = state_handle->current_tick;
            gear_change(1, 0, state_info[STATE_PAN_FIRE], state_handle);
        }
        else if (state_handle->total_tick >= INPUT_DATA_HZ * 20 && state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE - 1] < 900 && state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE - 2] < 900 && state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE - 3] < 900 && state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE - 4] < 900)
        {
            goto exit;
        }
    }
    else if (state_handle->pan_fire_state == PAN_FIRE_START && state_handle->pan_fire_tick + PAN_FIRE_ENTER_TICK < state_handle->current_tick) // 开关小火，规定时间温度没有跳降，不是移锅小火
    {
        log_debug("%s, %s pan_fire_state:%d", __func__, "set small fire errors", state_handle->pan_fire_state);
        // set_fire_gear(FIRE_BIG, state_handle, 0);
        // state_handle->pan_fire_state = PAN_FIRE_ERROR_CLOSE;
        goto exit;
    }

    if (state_handle->last_prepare_state != prepare_state)
    {
        state_handle->last_prepare_state = prepare_state;
        state_handle->last_prepare_state_tick = state_handle->current_tick;
    }
    if (prepare_state == STATE_RISE_SLOW)
    {
        log_debug("%s,%s pan_fire enter total_tick:%d last_prepare_state_tick:%d current_tick:%d", __func__, state_info[STATE_RISE_SLOW], state_handle->total_tick, state_handle->last_prepare_state_tick, state_handle->current_tick);
        if (state_handle->total_tick <= INPUT_DATA_HZ * 12)
        {
            state_handle->last_prepare_state_tick = state_handle->current_tick;
            return STATE_PAN_FIRE;
        }
        else
        {
            if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 4 > state_handle->current_tick)
                return STATE_PAN_FIRE;
        }
        log_debug("%s pan_fire quit total_tick:%d", __func__, state_handle->total_tick);
    }
    else if (prepare_state == STATE_DOWN_SLOW)
    {
        if (state_handle->last_temp > 1800)
        {
            state_handle->last_prepare_state_tick = state_handle->current_tick;
            return STATE_PAN_FIRE;
        }
        else if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 4 > state_handle->current_tick)
            return STATE_PAN_FIRE;
    }
    else if (prepare_state == STATE_GENTLE)
    {
        if (state_handle->last_temp > 1800)
        {
            state_handle->last_prepare_state_tick = state_handle->current_tick;
            return STATE_PAN_FIRE;
        }
        else if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 3 > state_handle->current_tick)
            return STATE_PAN_FIRE;
    }
    else if (prepare_state == STATE_RISE_JUMP)
    {
        if (state_handle->pan_fire_state == PAN_FIRE_ENTER)
        {
            if (state_handle->last_prepare_state_tick + 7 > state_handle->current_tick)
                return STATE_PAN_FIRE;
            if (state_handle->state_jump_diff > 500)
            {
                state_handle->pan_fire_rise_jump_exit_lock_tick = PAN_FIRE_RISE_JUMP_EXIT_LOCK_TICK;
                goto exit;
            }
        }
        else
        {
            if (state_handle->last_prepare_state_tick + 7 >= state_handle->current_tick)
                return STATE_PAN_FIRE;
        }
    }
    else if (prepare_state == STATE_DOWN_JUMP)
    {
        log_debug("%s,pan_fire state_jump_diff:%d", __func__, state_handle->state_jump_diff);
        if (state_handle->pan_fire_state == PAN_FIRE_START && state_handle->pan_fire_tick + PAN_FIRE_ENTER_TICK >= state_handle->current_tick)
        {
            if (state_handle->state_jump_diff > PAN_FIRE_HIGH_TEMP)
            {
                goto exit;
            }
            if (state_handle->pan_fire_enter_type == 1) // 翻炒进入移锅小火
            {
                if (state_handle->last_temp >= 800)
                {
                    state_handle->pan_fire_state = PAN_FIRE_ENTER;
                    state_handle->pan_fire_tick = 0;
                    return STATE_PAN_FIRE;
                }
            }
            else
            {
                state_handle->pan_fire_state = PAN_FIRE_ENTER;
                state_handle->pan_fire_tick = 0;
                return STATE_PAN_FIRE;
            }
        }
        else if (state_handle->pan_fire_state == PAN_FIRE_ENTER)
        {
            /*如果小口径锅把水煮开了，然后移开，火小后立马放锅，此时可能需要30秒左右才大火，看日志可能追踪到这里，但是不需要改，
                因为这里的3秒钟（PAN_FIRE_DOWN_JUMP_EXIT_TICK）是为了考虑大火转小火后，也可能出现的跳降准备的
            */ 
            if (state_handle->pan_fire_tick + PAN_FIRE_DOWN_JUMP_EXIT_TICK > state_handle->current_tick)
                return STATE_PAN_FIRE;
        }
    }
    else if (prepare_state == STATE_SHAKE)
    {
        return STATE_PAN_FIRE;
    }
    else if (prepare_state == STATE_PAN_FIRE)
    {
        return STATE_PAN_FIRE;
    }
    else if (prepare_state == STATE_IDLE)
    {
        return STATE_PAN_FIRE;
    }

    if (state_handle->pan_fire_state == PAN_FIRE_ENTER && state_handle->pan_fire_high_temp_exit_lock_tick == 0)
    {
        if (state_handle->last_temp >= PAN_FIRE_LOW_TEMP)
            state_handle->pan_fire_high_temp_exit_lock_tick = PAN_FIRE_HIGH_TEMP_EXIT_LOCK_TICK;
    }
exit:
    state_handle->pan_fire_state = PAN_FIRE_CLOSE;
    state_handle->pan_fire_enter_start_tick = 0;
    if (state_handle->temp_control_switch == 0)
    {
        LOGI("mars", "开始打开外环火(PAN_FIRE_CLOSE)");
        move_pan_fire(2);
        set_fire_gear(FIRE_BIG, state_handle, 0);       
        log_debug("%s,%s :%s", __func__, state_info[STATE_PAN_FIRE], "set big fire");
    }
    return prepare_state;
}

#ifdef BOIL_ENABLE
//触发水开的时候进入此状态，进入此状态先关火，退出此状态前会开火
static int state_func_boil(unsigned char prepare_state, state_handle_t *state_handle)
{
    if (state_handle->current_tick == 0)
    {
        log_debug("%s,enter state:%s", __func__, state_info[STATE_BOIL]);
        log_debug("首次进入state_func_boil");
        state_handle->current_tick = 1;
        log_debug("这里是state_func_boil调用关外环火");
        set_fire_gear(FIRE_SMALL, state_handle, 0);
        gear_change(2, 0, state_info[STATE_BOIL], state_handle);
        return prepare_state;
    }
    else
    {
        ++state_handle->current_tick;
    }

    if (state_handle->last_prepare_state != prepare_state)
    {
        state_handle->last_prepare_state = prepare_state;
        state_handle->last_prepare_state_tick = state_handle->current_tick;
    }
    log_debug("in boiled function current last_temp:%d",state_handle->last_temp);

    if (state_handle->last_temp <= BOIL_LOW_TEMP + 24)           //最新温度小于86摄氏度的时候就需要开启外环火，返回的是传入的准备状态
    {
        log_debug("温度小于86.4摄氏度，再次开启外环火");
        log_debug("lasttemp:%d",state_handle->last_temp);
        log_debug("这里是state_func_boil调用开外环火,Boiled->Unboiled");
        set_fire_gear(FIRE_BIG, state_handle, 0);
    }
    //从水开小火->干烧状态
    else if(state_handle->last_temp >= BOIL_HIGH_TEMP)
    {
        log_debug("boiled -> jump_state温度大于100摄氏度");
        //set_fire_gear(FIRE_BIG, state_handle, 0);
    }
    
    //下面的else状态都是表示水在开不需要打开通断阀
    else if (prepare_state == STATE_RISE_SLOW)
    {
        //连续传入缓降持续10s才会返回缓升，否则直接继续返回水开
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 10 > state_handle->current_tick)
            return STATE_BOIL;
    }
    else if (prepare_state == STATE_DOWN_SLOW)
    {
        if (state_handle->last_prepare_state_tick + INPUT_DATA_HZ * 2 > state_handle->current_tick)
            return STATE_BOIL;
    }
    else if (prepare_state == STATE_GENTLE)
    {
        return STATE_BOIL;
    }
    return prepare_state;
}
#endif


//防干烧判断函数
static int dryburn_judge(unsigned char prepare_state,state_handle_t *state_handle)
{
    #ifdef DRYBURN_ENABLE

    //防干烧锁定标志位
    static int dryburn_lock_flag = 0;
    
    //高温持续时间计数
    static int high_temp_duration_tick1 = 0;
    static int high_temp_duration_tick2 = 0;
    static int high_temp_duration_tick3 = 0;

    //翻炒后判断干烧的温度幅度
    static int shake_dryburn_temp = 0;

    //当前温度的平缓时间
    static int very_gentle_temp1_tick = 0;

    //当前的平缓温度（该温度和）
    static int very_gentle_temp1 = 0;

    //满足持续时间得到的非常平缓温度
    static unsigned int very_gentle_temp2_tick = 0;
    static int very_gentle_temp2 = 0;
    
    //开始判断防干烧的温度变化幅度
    static int dryburn_judge_temp = 0;

    //连续持续时间最长的平均温度（该平均温度只要发生了变化就会刷新）
    static unsigned int very_very_gentle_temp_longest = 0;

    //最长持续温度的持续时间
    static unsigned int very_very_gentle_temp_longest_tick = 0;
    
     //持续时间最长温度出现的时间戳
    static unsigned int very_very_gentle_temp_longest_timestamp = 0;

    //当前平均正在持续的平均温度
    static unsigned int very_very_gentle_temp = 0;

    //当前持续温度的持续时间
    static unsigned int very_very_gentle_temp_tick = 0;

    //当前持续温度的时间戳
    static unsigned int very_very_gentle_temp_timestamp = 0;

    //非常缓慢下降标志位
    static bool very_slowly_down_flag = 0;

    if(state_handle->dryburn_state == 1)
    {
        //防干烧锁定标志位
        dryburn_lock_flag = 0;
    
        //高温持续时间计数
        high_temp_duration_tick1 = 0;
        high_temp_duration_tick2 = 0;
        high_temp_duration_tick3 = 0;

        //翻炒后判断干烧的温度幅度
        shake_dryburn_temp = 0;

        //当前温度的平缓时间
        very_gentle_temp1_tick = 0;

        //当前的平缓温度（该温度和）
        very_gentle_temp1 = 0;

        //满足持续时间得到的非常平缓温度
        very_gentle_temp2_tick = 0;
        very_gentle_temp2 = 0;
    
        //开始判断防干烧的温度变化幅度
        dryburn_judge_temp = 0;

        //连续持续时间最长的平均温度（该平均温度只要发生了变化就会刷新）
        very_very_gentle_temp_longest = 0;

        //最长持续温度的持续时间
        very_very_gentle_temp_longest_tick = 0;
        
        //持续时间最长温度出现的时间戳
        very_very_gentle_temp_longest_timestamp = 0;

        //当前平均正在持续的平均温度
        very_very_gentle_temp = 0;

        //当前持续温度的持续时间
        very_very_gentle_temp_tick = 0;

        //当前持续温度的时间戳
       very_very_gentle_temp_timestamp = 0;

        //非常缓慢的温度下降标志位重置
       very_slowly_down_flag = 0;

        return prepare_state;
    }

    //如果主动关火了，就不再进行兜底计时触发干烧了
    if(state_handle->fire_status == 0)
    {
        state_handle->tick_for_dryburn = 0;
    }

    //持续开火5小时时间兜底触发防干烧
    if(state_handle->tick_for_dryburn >= 5 * 3600 * INPUT_DATA_HZ)
    {
        state_handle->dryburn_reason = 2;
        return STATE_DRYBURN;
    }
    
    int i;
    //打开此打印会导致系统崩溃
    // char log_str[BURN_DATA_SIZE * 4 + 20] = {0};
    // for(i=0; i<state_handle->arr_size; i++)
    // {
    //     char s[10];
    //     sprintf(s, "%d ", state_handle->last_average_temp_arr[i]);
    //     strcat(log_str, s);
    // }
    // log_debug("last_average_temp_arr： ", log_str);

    for(i=0;i<STATE_JUDGE_DATA_SIZE-1;i++)
    {
        if(state_handle->last_temp_data[i] != state_handle->last_temp_data[i+1])
        {
            break;
        }
    }

    if(i == 8 && state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE - 1] > 1000)
    {
        log_debug("连续10组温度数据不变,steady temp:%d",state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE-1]);
    }
    log_debug("dryburn_lock_flag:%d",dryburn_lock_flag);
    
    int count_jump = 0;
    for(int i = 0; i < STATE_JUDGE_DATA_SIZE - 1; i++)
    {   
        if(abs(state_handle->last_temp_data[i] - state_handle->last_temp_data[i+1]) > 80 )
        {
            log_debug("count_jump跳动");
            count_jump++;
        }

        if(state_handle->last_temp_data[i+1] - state_handle->last_temp_data[i] > 250 && state_handle->last_temp_data[i+1] >= 2300)
        {
            log_debug("温度跳动至230度，锁定判断标志");
            dryburn_lock_flag = 1;
        }
    }

    //当前平均温度大于230摄氏度且没有温度波动
    if(state_handle->last_temp_average >= 2300 && count_jump == 0)
    {
        if(state_handle->last_temp_average >= 2700)
        {
            high_temp_duration_tick3++;
            high_temp_duration_tick2++;
            high_temp_duration_tick1++;
        }
        else if(state_handle->last_temp_average >= 2500)
        {
            high_temp_duration_tick2++;
            high_temp_duration_tick1++;
        }
        else if(state_handle->last_temp_average >= 2300)
        {
            high_temp_duration_tick1++;
        }
    }

    if(state_handle->last_temp_average >= 2300 && count_jump > 0)        //如果有波动
    {
        high_temp_duration_tick1 = 0;
        high_temp_duration_tick2 = 0;
        high_temp_duration_tick3 = 0;
        
        log_debug("处于高温且温度波动，清除高温时间计时");
    }
    
    if(state_handle->last_temp_average < 2300)
    {
        high_temp_duration_tick1 = 0;
        high_temp_duration_tick2 = 0;
        high_temp_duration_tick3 = 0;
    }
    else if(state_handle->last_temp_average < 2500)
    {
        high_temp_duration_tick2 = 0;
        high_temp_duration_tick3 = 0;
    }
    else if(state_handle->last_temp_average < 2700)
    {
        high_temp_duration_tick3 = 0;
    }

    if(state_handle->last_temp_average < 2300)
    {
        dryburn_lock_flag = 0;
    }

    if(dryburn_lock_flag)       //识别为直接开火，防干烧锁定
    {
        log_debug("防干烧判断锁定，函数返回");
        return -1;
    }

    if(high_temp_duration_tick1 >= 30*INPUT_DATA_HZ || high_temp_duration_tick2 >= 20*INPUT_DATA_HZ || high_temp_duration_tick3 >= 10*INPUT_DATA_HZ)
    {
        log_debug("get dryburn from hightemp2");
        state_handle->dryburn_reason = 1;
        return STATE_DRYBURN;
    }

    if(state_handle->pre_state == STATE_SHAKE)
    {
        log_debug("shake_exit_average_temp:%d",state_handle->shake_exit_average_temp);
        if(state_handle->shake_exit_average_temp < 600)
        {
            shake_dryburn_temp = 300;
        }
        else if(state_handle->shake_exit_average_temp >= 600 && state_handle->shake_exit_average_temp < 800)
        {
            shake_dryburn_temp = 250;
        }
        else if(state_handle->shake_exit_average_temp >= 800)
        {
            shake_dryburn_temp = 200;
        }
    }

    if(state_handle->pre_state == STATE_SHAKE ||(state_handle->heat_pot_flag == 1 && state_handle->shake_status_flag == 1 && state_handle->shake_status_average > 0))
    {
        if(state_handle->shake_status_average - state_handle->last_temp_average > 150 )
        {
            log_debug("认为是加水操作或者是加盖，attention!think add water or put pot cover 差值：%d",state_handle->shake_status_average - state_handle->last_temp_average);     //加盖后无法准确判断干烧时机，且加盖的一般需要类似炖煮的烹饪。全部按照加水
            state_handle->shake_down_flag = 1;
        }
    }

    //开火30s内温度达到了100℃以上，且比开火前温度上升了至少50℃,热锅或者冷锅冷油加热阶段
    if(state_handle->total_tick <= 30*INPUT_DATA_HZ && state_handle->last_temp_average >= 1000)
    {
        if(state_handle->ignition_switch_close_temp + 500 < state_handle->last_temp_average)
        {
            log_debug("热锅阶段");
            state_handle->heat_pot_flag = 1;
        }
    }
   
    // for(int i = 0; i < state_handle->arr_size; i++)
    // {
    //     printf("%d ",state_handle->last_average_temp_arr[i]);
    // }
    // printf("\r\n");

   //此if中判断非常缓慢下降，此时平均温度数组中的数值需要满足数量BURN_DATA_SIZE
    if(state_handle->arr_size >= BURN_DATA_SIZE)
    {
        unsigned short slowly_down_count = 0;
        unsigned short rise_count = 0;
        for(int i = 0; i < BURN_DATA_SIZE-1; i++)
        { 
            if(state_handle->last_average_temp_arr[i] >= state_handle->last_average_temp_arr[i+1])
            {
                if(abs(state_handle->last_average_temp_arr[i] - state_handle->last_average_temp_arr[i+1]) <= 15 && state_handle->max_gentle_average_temp > 800)
                {    
                    slowly_down_count++;
                }
            }
            else if(state_handle->last_average_temp_arr[i] < state_handle->last_average_temp_arr[i+1])
            {
                rise_count++;
            }
        }

        //log_debug("temp_count:%d\r\n",temp_count);
        log_debug("slowly_down_count:%d",slowly_down_count);
        if((slowly_down_count > BURN_DATA_SIZE - 40) && (state_handle->last_average_temp_arr[0] - state_handle->last_average_temp_arr[BURN_DATA_SIZE-1] > 150) &&\
        state_handle->last_temp_average > 700)
        {
            log_debug("检测到温度缓慢下降，可能是用户手动调火");
            very_slowly_down_flag = 1;
            
        }
        else if(rise_count >= 50 || abs(state_handle->last_average_temp_arr[BURN_DATA_SIZE -1] - state_handle->last_average_temp_arr[BURN_DATA_SIZE -2]) > 30 )
        {   
            very_slowly_down_flag = 0;
        }

        log_debug("slowly_down_flag:%d\r\n",very_slowly_down_flag);
    }

    //表示首次进入一个新的状态，当前状态tick为1
    if(state_handle->current_tick == 1)
    {
        if(state_handle->state == STATE_GENTLE)
        {
            //初始化相关变量
            //非常平缓时间
            log_debug("进入gentle，very_gentle_temp2清零");
            //非常平缓温度
            very_gentle_temp1 = 0;
            very_gentle_temp1_tick = 0;
            very_gentle_temp2 = 0;
            very_gentle_temp2_tick = 0;

            very_very_gentle_temp_longest = 0;
            very_very_gentle_temp_longest_tick = 0;
            very_very_gentle_temp_longest_timestamp = 0;

            very_very_gentle_temp = 0;
            very_very_gentle_temp_tick = 0;
            very_very_gentle_temp_timestamp = 0;

            //判断防干烧的温度变化幅度
            dryburn_judge_temp = 0;
            log_debug("[%s],state changed to gentle",__func__);
        }
        else if(state_handle->state == STATE_RISE_SLOW && state_handle->pre_state != STATE_GENTLE)
        {
            log_debug("[%s],state changed to rise_slow",__func__);
            log_debug("进入rise_slow，前一个状态不是gentle，very_gentle_temp2清零");
            //非常平缓温度
            very_gentle_temp1 = 0;
            very_gentle_temp1_tick = 0;
            very_gentle_temp2 = 0;
            very_gentle_temp2_tick = 0;

            very_very_gentle_temp_longest = 0;
            very_very_gentle_temp_longest_tick = 0;
            very_very_gentle_temp_longest_timestamp = 0;

            very_very_gentle_temp = 0;
            very_very_gentle_temp_tick = 0;
            very_very_gentle_temp_timestamp = 0;

            //判断防干烧的温度变化幅度
            dryburn_judge_temp = 0;
            log_debug("[%s],state changed to gentle",__func__);
        }
        if(state_handle->pre_state == STATE_SHAKE)
        {
            state_handle->shake_status_flag = 1;
            shake_dryburn_temp = 0;
        }
        log_debug("state change from [%s]->[%s]",state_info[state_handle->pre_state],state_info[state_handle->state]);    
    }
    else if(state_handle->current_tick > 1)
    {
         //110℃以内（因为判断温度高于平缓10℃以内判断所以100+10度内判断，否则清零），温度变化上下不超过3℃认为是非常平缓,当前温度必须大于点火前温度5℃来防止刚开火时候误触（低温误触）或者点火温度本就较高就会上升较快不需要大于点火前5度
        if(state_handle->last_temp_average <= 1200 && state_handle->last_temp_average >= 400 && (state_handle->last_temp_average >= state_handle->ignition_switch_close_temp + 30 || state_handle->ignition_switch_close_temp >= 400))
        {
            if(abs(very_gentle_temp1 - state_handle->last_temp_average) > 60 )      //适配ET70采集温度较为跳动，修改为上下跳动6度
            {
                very_gentle_temp1 = state_handle->last_temp_average;
                very_gentle_temp1_tick = 0;
            }
            else
            {
                very_gentle_temp1_tick++;
            }

            if(state_handle->last_temp_average != very_very_gentle_temp)
            {
                very_very_gentle_temp = state_handle->last_temp_average;
                very_very_gentle_temp_timestamp = state_handle->total_tick;
                very_very_gentle_temp_tick = 0;
            }
            else
            {
                very_very_gentle_temp_tick++;
            }
        }
        else
        {
            log_debug("gentle判断的温度大前提不满足，very_gentle_temp2变量清零");
            very_gentle_temp2 = 0;
            very_gentle_temp1 = 0;
            very_gentle_temp1_tick = 0;
            very_gentle_temp2_tick = 0;
            very_very_gentle_temp_tick = 0;
        } 

        log_debug("[%s],very_gentle_temp1_tick:%d,very_gentle_temp1:%d",__func__,very_gentle_temp1_tick,very_gentle_temp1);
        if((very_gentle_temp1_tick >= 85*INPUT_DATA_HZ && very_gentle_temp1 <= 1030) || (very_gentle_temp1_tick >= 55*INPUT_DATA_HZ && very_gentle_temp1 > 1030 && very_gentle_temp1 <= 1200))
        {
            if(state_handle->arr_size >= BURN_DATA_SIZE)
            {
                int count = 0;
                for(int i = 0;i<state_handle->arr_size;i++)
                {
                    if(state_handle->last_average_temp_arr[i] % 10 == 0)
                    {
                        count++;        //如果是整度的平均值则进行计数
                    }
                }

                log_debug("count整数温度：%d,温度差值：%d last_temp2_tick:%d",count,state_handle->last_average_temp_arr[state_handle->arr_size - 1] - state_handle->last_average_temp_arr[0],very_gentle_temp2_tick);
                //全程小火最新平均数据大于最早平均数据，但大于的值不超过14℃，出现小火陶瓷锅漏掉的情况将-25修改为-50
                if(count >= BURN_DATA_SIZE - 50 && state_handle->last_average_temp_arr[state_handle->arr_size - 1] > state_handle->last_average_temp_arr[0] && \
                abs(state_handle->last_average_temp_arr[state_handle->arr_size - 1] - state_handle->last_average_temp_arr[0]) <= 140) 
                {
                    log_debug("认为此曲线是阶梯上升的曲线，不对very_gentle_temp2赋值");
                }
               //新的平衡时间只要大于上次平衡温度时间-75s则立即刷新，同时限制待刷新温度大于70度(铝盖未干烧时温度小于70℃)。即：平衡温度大于70℃的时候平衡温度取持续时间最长的或者最长-75s的
                else if(very_gentle_temp1_tick + 75 * INPUT_DATA_HZ > very_gentle_temp2_tick && very_gentle_temp1 >= 700)       
                {
                    very_gentle_temp2 = very_gentle_temp1;
                    very_gentle_temp2_tick = very_gentle_temp1_tick;
                    log_debug("[%s],very_gentle_temp2:%d",__func__,very_gentle_temp2);  
                }
                else if(very_gentle_temp1 < 700)
                {
                    if(very_slowly_down_flag == 0)
                    {
                        very_gentle_temp2 = very_gentle_temp1;
                        very_gentle_temp2_tick = very_gentle_temp1_tick;
                        log_debug("[%s],very_gentle_temp2:%d\r\n",__func__,very_gentle_temp2);
                    }
                    else if(very_slowly_down_flag == 1 /*&& very_gentle_temp2 >= 700*/)
                    {
                        log_debug("不赋值，因为温度缓慢下降,可能是用户手动调小火");
                        very_gentle_temp2 = 0;
                        very_gentle_temp2_tick = 0;
                    }
                }  
                else
                {
                    log_debug("不大于最长的持续时间减去75s，不赋值");
                }

            }
            else
            {
                log_debug("time short,not judge\r\n");
            }                
        }
        // //防止103℃以上的温度跳动导致无法获取稳定温度
        // else if(very_gentle_temp1_tick >= 55*INPUT_DATA_HZ && very_gentle_temp1 > 1030 && very_gentle_temp1 <= 1200)
        // {
        //     very_gentle_temp2 = very_gentle_temp1;
        //     very_gentle_temp2_tick = very_gentle_temp1_tick;
        //     log_debug("[%s],very_gentle_temp2:%d",__func__,very_gentle_temp2);
        // }

        //开火250s后才会获取非常非常平缓温度
        if(state_handle->total_tick > 250*INPUT_DATA_HZ && very_very_gentle_temp_tick >= 12*INPUT_DATA_HZ)
        {
            //过去的BURN_DATA_SIZE个平均值的平均值
            unsigned int average_temp = 0;
            log_debug("very_very_gentle_temp:%d,very_very_gentle_temp_tick:%d",very_very_gentle_temp,very_very_gentle_temp_tick);
            
            //出现了更长时间的持续温度，在此温度刚替代上一个最长温度时开始记录时间戳
            if(very_very_gentle_temp_longest_tick < very_very_gentle_temp_tick)      
            {
                log_debug("");
                for(int i = 0;i<state_handle->arr_size;i++)
                {
                    average_temp += state_handle->last_average_temp_arr[i];
                    printf("%d ",state_handle->last_average_temp_arr[i]);
                }
                log_debug("");
                log_debug("average_temp total:%d",average_temp);
                average_temp /= state_handle->arr_size;
                log_debug("average_temp average:%d",average_temp);
                
                int very_gentle_count = 0;
                int max = state_handle->last_temp_average,min = state_handle->last_temp_average;
                for(int i = 0;i<state_handle->arr_size;i++)
                {
                    //正常小火干烧时有温度下降的情况，我们不做限制，只限制温度低于very_very_gentle的平均温度，不能只写very_very_gentle_temp - state_handle->last_average_temp_arr[i] < 10 逻辑不一样
                    if((very_very_gentle_temp - state_handle->last_average_temp_arr[i] < 10 && very_very_gentle_temp > state_handle->last_average_temp_arr[i]) ||(very_very_gentle_temp <= state_handle->last_average_temp_arr[i]))
                    {
                        very_gentle_count++;
                    }
                    if(state_handle->last_average_temp_arr[i] > max)
                    {
                        max = state_handle->last_average_temp_arr[i];
                    }
                    else if(state_handle->last_average_temp_arr[i] < min)
                    {
                        min = state_handle->last_average_temp_arr[i];
                    }    
                }
                log_debug("arr_size:%d,very_gentle_count:%d",state_handle->arr_size,very_gentle_count);
                
                //出现非常平缓之前200s的80组数据必须是比较稳定的温度，允许其中4组数据不满足条件，防止低温小火煮水时出现温度稳定而导致误触的问题
                if(very_gentle_count >= BURN_DATA_SIZE - 20 && max - min <= 10 /*&& state_handle->total_tick - state_handle->very_very_gentle_temp_longest_clear_timestamp > 8*60*INPUT_DATA_HZ*/)
                {
                    very_very_gentle_temp_longest = very_very_gentle_temp;
                    very_very_gentle_temp_longest_tick = very_very_gentle_temp_tick;
                    very_very_gentle_temp_longest_timestamp = state_handle->total_tick - very_very_gentle_temp_longest_tick;    //准确记录该温度首次出现的时间戳要减去前面累计的时间
                    log_debug("flush last_gentle最长时间的温度是：%d,持续时间longest_time是：%d",very_very_gentle_temp_longest,very_very_gentle_temp_longest_tick);
                    
                    //这里增加一处赋值，为了防止获得了very_very_gentle后没有后续12s稳定温度的情况
                    very_gentle_temp2 = very_very_gentle_temp;
                    //very_gentle_temp2_tick = state_handle->total_tick;
                }
                else
                {
                    log_debug("温度不够平稳不对last gentle赋值,待赋值温度是:%d",very_very_gentle_temp);
                }
            }
            else
            {
                log_debug("steady time greater than 12s,but less than longest_temp_tick");
            }

            if(very_very_gentle_temp_longest_timestamp > 0 && very_very_gentle_temp > very_very_gentle_temp_longest + 30 /*&& very_very_gentle_temp_timestamp > very_very_gentle_temp_longest_timestamp + 3*60*INPUT_DATA_HZ */)
            {
                log_debug("get dryburn from very_very_gentle");
                state_handle->dryburn_reason = 4;
                return STATE_DRYBURN;
            } 
        }
        log_debug("now last_gentle temp:%d,longest_time is:%d",very_very_gentle_temp_longest,very_very_gentle_temp_longest_tick);

         //最新的相邻的两个平均温度变化了超过2.5℃，则非常非常平缓温度清零
        if(abs(state_handle->last_average_temp_arr[state_handle->arr_size - 1] - state_handle->last_average_temp_arr[state_handle->arr_size - 2]) >= 25)        
        {
            log_debug("temp change speed up,zero clear very_very_gentle_temp_longest,clear_timestamp:%d",state_handle->very_very_gentle_temp_longest_clear_timestamp);
            state_handle->very_very_gentle_temp_longest_clear_timestamp = state_handle->total_tick;       //记录最长平缓时间满足清零条件的时间
            very_very_gentle_temp_longest = 0;
            very_very_gentle_temp_longest_tick = 0;
            very_very_gentle_temp_longest_timestamp = 0;
        }
        
        if(abs(state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE - 1] - state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE - 2]) > 60)
        {
            very_gentle_temp2 = 0;
            very_gentle_temp2_tick = 0;
            log_debug("温度变化较快very_gentle_temp2清零");
        }

        //温度较低的锅盖干烧时温度上升也比较慢，所以开始判断干烧的温度幅度设置的更小
        //modify 230901为了适配下饺子后温度会在79℃得到一个very_gentle_temp2随后饺子煮开导致误触防干烧调整dryburn_judge_temp的设置范围，炖鸡腿的时候也会得到72℃的very_gentle_temp2
        if(very_gentle_temp2 >= 400 && very_gentle_temp2 < 700)
        {
            dryburn_judge_temp = 90;
        }
        else if(very_gentle_temp2 >= 700 && very_gentle_temp2 <= 900)
        {
            dryburn_judge_temp = 1;                 //此赋值1，作为标志位使用，而不是幅度
        }
        else if(very_gentle_temp2 > 900 && very_gentle_temp2 < 950)
        {
            dryburn_judge_temp = 90;             
        }
        else if(very_gentle_temp2 >= 950 && very_gentle_temp2 <= 1100)
        {
            dryburn_judge_temp = 60;
        }
        else if(very_gentle_temp2 > 1100)
        {
            dryburn_judge_temp = 60;
        }   
        log_debug("[%s],last_temp_average_temp:%d",__func__,state_handle->last_temp_average);
        log_debug("[%s],gentle_state_enter_temp:%d",__func__,state_handle->gentle_state_enter_temp);

        /*进入平缓至少25s后开始判断防干烧。
        前一个状态不是翻炒：温度大于100℃，十组平均数据9次相邻比较中至少8组是上升趋势，且中间降低的那一组降低幅度不超过5℃；
        前一个状态是翻炒：翻炒退出温度小于120℃（认为是炒菜），进行相同的判断；因为翻炒退出温度大于120℃认为是油炸的情况
        */
        if(state_handle->arr_size < 10)
        {
            return prepare_state;
        } 

        int i=0;
        int temp = state_handle->arr_size;
        char last_average_temp_str_arr[4 * 10 + 20] = {0};
        for(i=1; i<=10;i++)
        {
            char s[10];
            sprintf(s, "%d ", state_handle->last_average_temp_arr[temp-i]);
            strcat(last_average_temp_str_arr, s);
        }
        log_debug("last 10 average data: %s", last_average_temp_str_arr);
        log_debug("事件标志位：%d",state_handle->shake_down_flag);
        
        //DRYBURN1 DRYBURN2
        //针对炒菜温度直接上升的情况，玻璃盖烧水的情况，铝盖烧水的情况(只要是非常平缓的情况都会被计算在内,平缓超过85s记录温度，大于记录温度3℃以上小于比记录温度高20度的温度范围内完成判断)
        log_debug("[%s],very_gentle_temp2:%d,very_gentle_temp2_tick:%d,dryburn_judge_temp_tick:%d,last_temp_average:%d",__func__, very_gentle_temp2, very_gentle_temp2_tick, dryburn_judge_temp, state_handle->last_temp_average);
        log_debug("pre_state:%s,pre_pre_state:%s",state_info[state_handle->pre_state],state_info[state_handle->pre_pre_state]);
        
        /*前一个或者前前一个状态时翻炒且翻炒退出温度小于120摄氏度 且 温度上升符合shake后的上升幅度 且 没有发生炒后加水的逻辑接着判断连续上升即为“翻炒防干烧”
        获得非常平缓温度后连续上升认定为防干烧
        */
        if(\
        (state_handle->high_temp_shake_flag == 0 && state_handle->shake_exit_average_temp <= 1500 && \
        (state_handle->pre_state == STATE_SHAKE || state_handle->pre_pre_state == STATE_SHAKE || (state_handle->heat_pot_flag == 1 && state_handle->shake_status_flag == 1)) && \
        state_handle->total_tick >= state_handle->shake_exit_timestamp + 30*INPUT_DATA_HZ && state_handle->last_temp_average >= state_handle->shake_exit_average_temp + shake_dryburn_temp && \
        state_handle->shake_exit_average_temp + shake_dryburn_temp >= 800 && state_handle->shake_down_flag != 1)||\

        (very_gentle_temp2 > 0 && ((state_handle->last_temp_average > very_gentle_temp2 + dryburn_judge_temp && state_handle->last_temp_average < very_gentle_temp2 + 420 && dryburn_judge_temp != 1) \
        || (state_handle->last_temp_average >= BOILED_TEMP  * 10 && dryburn_judge_temp == 1)) && state_handle->last_temp_average > 400) || \

        (state_handle->shake_status_flag == 1 && state_handle->heat_pot_flag == 1 && state_handle->last_temp_average > state_handle->shake_exit_average_temp + 500 && \
        state_handle->total_tick > state_handle->shake_exit_timestamp + 80*INPUT_DATA_HZ && state_handle->last_temp_average >= BOILED_TEMP * 10 + 50 && state_handle->shake_down_flag != 1)\
        )
        {
            log_debug("[%s],satisfy major premise",__func__);
            //可以尝试加一个大于翻炒退出温度指定温度来认定为防干烧
            int average_grow_value = 0;
            int average_reduce_value = 0;
            int count = 0;
            for(i=state_handle->arr_size;i>state_handle->arr_size-9;i--)
            {
                if(state_handle->last_average_temp_arr[i-1] - state_handle->last_average_temp_arr[i-2] >= 0)
                {
                    count++;
                }
                
                //获取相邻两个平均值之间的温度减小值和增加值
                if(state_handle->last_average_temp_arr[i-2] - state_handle->last_average_temp_arr[i-1] > average_reduce_value )
                {
                    average_reduce_value = state_handle->last_average_temp_arr[i-2]-state_handle->last_average_temp_arr[i-1];
                }
                else if(state_handle->last_average_temp_arr[i-1] - state_handle->last_average_temp_arr[i-2] > average_grow_value)
                {
                    average_grow_value = state_handle->last_average_temp_arr[i-1]-state_handle->last_average_temp_arr[i-2];
                }
            }
            //如果中间存在一组降低，降低温度不得大于2℃,升高温度不得超过5℃，同时最新温度和最新独立10个温度的平均温度之间的差值也不能超过5℃，防止铝盖开盖误触 变量修改24.05.09
            if(count >= 7 && average_reduce_value <= 20 &&(state_handle->last_temp_average >BOILED_TEMP * 10 || (average_grow_value <= 50 && state_handle->last_temp - state_handle->last_temp_average <= 50)))
            {
                log_debug("[%s],before state is:%s,very_gentle_temp2:%d",__func__,state_info[state_handle->pre_state],very_gentle_temp2);
                log_debug("dryburn_judge_temp:%d",dryburn_judge_temp);
                log_debug("current state:%s,pre_state:%s,pre_pre_state:%s",state_info[state_handle->state],state_info[state_handle->pre_state],state_info[state_handle->pre_pre_state]);
                
                //此处的if就是上面进入的判断，在这里再次分类是哪种触发方式
                if(very_gentle_temp2 > 0 && state_handle->last_temp_average > very_gentle_temp2 + dryburn_judge_temp && state_handle->last_temp_average < very_gentle_temp2 + 420 && state_handle->last_temp_average > 400)
                {
                    log_debug("get dryburn from 1 gentle");
                    state_handle->dryburn_reason = 3;
                    return STATE_DRYBURN;
                }
                else if(state_handle->high_temp_shake_flag == 0 && state_handle->shake_exit_average_temp <= 1500 && \
                (state_handle->pre_state == STATE_SHAKE || state_handle->pre_pre_state == STATE_SHAKE || (state_handle->heat_pot_flag == 1 && state_handle->shake_status_flag == 1)) && \
                state_handle->total_tick >= state_handle->shake_exit_timestamp + 30*INPUT_DATA_HZ && state_handle->last_temp_average >= state_handle->shake_exit_average_temp + shake_dryburn_temp && \
                state_handle->shake_exit_average_temp + shake_dryburn_temp >= 800 && state_handle->shake_down_flag != 1)
                {
                    log_debug("get dryburn from 2 shake");
                    log_debug("flag:%d",state_handle->shake_down_flag);
                    state_handle->dryburn_reason = 3;
                    return STATE_DRYBURN;
                }
                else if(state_handle->shake_status_flag == 1 && state_handle->heat_pot_flag == 1 && state_handle->last_temp_average > state_handle->shake_exit_average_temp + 500 && \
                state_handle->total_tick > state_handle->shake_exit_timestamp + 60*INPUT_DATA_HZ && state_handle->last_temp_average >= BOILED_TEMP * 10 + 50 && state_handle->shake_down_flag != 1)
                {
                    //翻炒中平缓上升检测到干烧
                    log_debug("get dryburn from 3 shake");
                    state_handle->dryburn_reason = 3;
                    return STATE_DRYBURN;
                }
                state_handle->dryburn_reason = 3;
                //return STATE_DRYBURN;
            }
            //防止使用铝盖时开盖误触，检测到100度以内温度上升较快则清除very_gentle_temp2的数值，重新计数
            else if(state_handle->last_temp_average < BOILED_TEMP * 10 && (state_handle->last_temp_average - state_handle->last_average_temp_arr[state_handle->arr_size - 1] > 50 || average_grow_value > 50))
            {
                log_debug("检测到温度上升过快，very_gentle_temp2变量清零");
                very_gentle_temp2 = 0;
                very_gentle_temp1_tick = 0;
                very_very_gentle_temp_tick = 0;
            }
        }

        //记录gentle中最大的平均温度
        if(state_handle->last_temp_average > state_handle->max_gentle_average_temp && state_handle->state == STATE_GENTLE)
        {
            state_handle->max_gentle_average_temp = state_handle->last_temp_average;
        }
        log_debug("[%s],max_gentle_average_temp:%d",__func__,state_handle->max_gentle_average_temp);

        //针对煲汤，水干之后食材露出时候温度有所下降的情况，且这种情况温度不会再上升只能通过“温降”来判断
        if(state_handle->arr_size >= 20 && state_handle->last_temp_average <= 1000)
        {
            int dry_average[5];
            char dry_average_str_arr[4 * 5 + 20] = {0};
            for(i = 0; i < 5; i++)
            {
                dry_average[i] = state_handle->last_average_temp_arr[4*i+0] + state_handle->last_average_temp_arr[4*i+1] + \
                state_handle->last_average_temp_arr[4*i+2] + state_handle->last_average_temp_arr[4*i+3];
                dry_average[i] /= 4;
                char s[10];
                sprintf(s, "%d ", dry_average[i]);
                strcat(dry_average_str_arr, s);
            }
            log_debug("5 dry_average is: %s", dry_average_str_arr);

            int dry_count = 0;
            for(i = 1; i <= 10; i++)
            {
                //针对干烧后温度下降幅度较大的食物，例如：面条，温度确实下降且在下降中持续一段时间
                if(state_handle->last_average_temp_arr[temp-i] + 50 <= state_handle->max_gentle_average_temp && state_handle->max_gentle_average_temp <= 100 * 10)      //手动设置为100
                {
                    dry_count++;
                }
            }

            if(dry_count >= 10)
            {
                for(i = 0; i < 4; i++)
                {
                    if(dry_average[i]-dry_average[i+1] > 0 && dry_average[i]-dry_average[i+1] < 40)
                    {
                        dry_count++;
                    }
                }
            }
            log_debug("dry_count is:%d",dry_count);

            if(dry_count == 14)
            {
               log_debug("detected water level down");
                state_handle->water_down_flag = 1;
                state_handle->water_down_timestamp = state_handle->total_tick;
            }
        }

        if(state_handle->water_down_flag == 1 && state_handle->last_temp_average > 100 * 10)        //手动设置为100
        {
            log_debug("get dryburn from water_down temp_down");
            state_handle->dryburn_reason = 4;
            return STATE_DRYBURN;
        }
    }
    #endif
    return prepare_state;
}

#ifdef DRYBURN_ENABLE
/**
 * @description: 防干烧状态函数，触发后用户不介入则不退出该函数，移锅小火、风随烟动此时不生效！！！
 * @param {unsigned char} prepare_state
 * @param {state_handle_t} *state_handle
 * @return {*}
 */
static int state_func_dryburn(unsigned char prepare_state, state_handle_t *state_handle)
{
    if(state_handle->current_tick == 0)
    {
        log_debug("first enter dryburn state func");

        if(ANGEL_USER == state_handle->dryburn_user_category)
        {
            log_debug("ANGEL USER wait time on close fire");
        }
        else if(VOLUME_USER == state_handle->dryburn_user_category)     //量产用户不关火
        {
            log_debug("VOLUME USER not close fire");
        }   

        //callback inform enter dryburn state
        cook_dryburn_remind_cb(1, state_handle->dryburn_reason);
        state_handle->current_tick = 1;
        state_handle->dryburn_state = 1;    //进入了干烧状态
    }
    else
    {
        state_handle->current_tick++;
    } 

    //进入后不可直接打开风机3档，等待1s后打开，此时可能是出于电源关闭状态时触发的干烧，需要等待唤起电源开机后操作风机
    if(state_handle->current_tick == INPUT_DATA_HZ)
    {
        if (state_hood.smart_smoke_switch == 1)
        {
            //启动时type必须为1,因为此时烟机可能是关闭的，否则将会导致打不开烟机的情况
            gear_change(3, 1, "发生干烧开启了fsyd,dryburn set hood gear 3", state_handle);
        }
        else
        {
            if (hood_gear_cb != NULL)
            {
                state_hood.gear = 3;
                log_debug("发生干烧未开启fsyd,dryburn set hood gear 3:%d",state_hood.gear);
                hood_gear_cb(state_hood.gear);
            }
        }
    }

    //用户按键介入后通过dryburn_state的判断退出防干烧状态
    if(state_handle->dryburn_state == 0)
    {
        if(cook_dryburn_remind_cb != NULL)
        {
            ca_dryburn_reinit(state_handle);
            cook_dryburn_remind_cb(2,state_handle->dryburn_reason);      //确认要退出当前状态的时候才会通知
            //return prepare_state;
            goto end;
        }
    }
    else if(state_handle->dryburn_state == 2)       
    {
        if(cook_dryburn_remind_cb != NULL)
        {
            //电控关火退出防干烧，此时不用再次通知应用，重置相关参数即可
            log_debug("stove electric close fire");
            ca_dryburn_reinit(state_handle);
        }
    }

    //天使用户关火逻辑
    if(ANGEL_USER == state_handle->dryburn_user_category)
    {
        //温度兜底触发，30s后关火；其他条件触发防干烧，3分钟后关火
        if( (state_handle->dryburn_reason == 1 && state_handle->current_tick > 30 * INPUT_DATA_HZ) || \
        ((state_handle->dryburn_reason >= 2 && state_handle->dryburn_reason <= 4) && state_handle->current_tick > 3 * 60 * INPUT_DATA_HZ) )
        {   
            log_debug("ANGEL USER close fire");
            if(close_fire_cb != NULL)
            {
                close_fire_cb(INPUT_RIGHT);           //给关火回调函数传参
            }
            
            if(close_beeper_cb != NULL)
            {
                close_beeper_cb(1);
            }
            
            //PS:这里不要通知电控板退出防干烧状态，此时是防干烧天使用户下算法进行的关火，图标需要继续闪烁
            goto end;
        }
    }


    log_debug("last 2 temp abs is:%d",state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE - 1] - state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE - 2]);
    //相邻0.25s内温度变化超过设定值算法判断自动退出防干烧，低温时阈值小，高温时阈值大
    int temp_diff = abs(state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE - 1] - state_handle->last_temp_data[STATE_JUDGE_DATA_SIZE - 2]);

    //判断温度下降退出干烧状态时，如果是因为时间兜底进入的干烧，不通过温度下降判断退出
    if(state_handle->dryburn_reason != 2 && temp_diff >= 100)
    {
        log_debug("%s:detect temp change quickly,exit dryburn state",__func__);
        ca_dryburn_reinit(state_handle);
        cook_dryburn_remind_cb(2,state_handle->dryburn_reason);      //通知退出干烧状态
        state_handle->dryburn_state = 0;        //设置为非干烧状态，干烧判断函数继续生效
        goto end;
        
    }
    
    //set_fire_gear(FIRE_SMALL, state_handle, 0);
    //防干烧不会因为判断到其他状态而退出，除非关火、用户按键、算法自动判断
    log_debug("warning dryburn ");
    return STATE_DRYBURN;

    end:
    //goto到这里准备退出防干烧状态函数，确保返回的状态是非干烧状态
    if(prepare_state != STATE_DRYBURN)
        return prepare_state;
    else
        return STATE_IDLE;
}

#endif

static state_func_def g_state_func_handle[] = {state_func_shake, state_func_down_jump, state_func_rise_jump, state_func_rise_slow, state_func_down_slow, state_func_gentle, state_func_idle, state_func_pan_fire
#ifdef BOIL_ENABLE
                                               ,
                                               state_func_boil
#endif
#ifdef DRYBURN_ENABLE
    ,
    state_func_dryburn
#endif

};

void ca_dryburn_reinit(state_handle_t *state_handle)
{
    //如果是时间兜底退出的时候，需要清除时间重新计时
    if(state_handle->dryburn_reason == 2)
    {
        state_handle->tick_for_dryburn = 0;
    }
    state_handle->state = STATE_IDLE;
    state_handle->pre_state = STATE_IDLE;
    state_handle->pre_pre_state = STATE_IDLE;
    state_handle->shake_status_flag = 0;
    state_handle->shake_down_flag = 0;
    state_handle->after_shake_down_temp = 0;
    state_handle->after_shake_down_timestamp = 0;
    state_handle->heat_pot_flag = 0;
    state_handle->very_very_gentle_temp_longest_clear_timestamp = 0;

    state_handle->max_gentle_average_temp = 0;
    state_handle->water_down_flag = 0;
    state_handle->dryburn_gentle_temp = 0;
    state_handle->dryburn_gentle_tick = 0;
    state_handle->dryburn_state = 0;    //用户介入后，初始化防干烧状态为0
    state_handle->dryburn_reason = 0;   //用户介入后，初始化防干烧原因为0
}


//关火的时候重新初始化烹饪助手的相关变量
void cook_assistant_reinit(state_handle_t *state_handle)
{
    //state_handle->dryburn_user_category = VOLUME_USER;       //临时设置

    log_debug("%s,cook_assistant_reinit", __func__);
    state_handle->state = STATE_IDLE;
    state_handle->pre_state = STATE_IDLE;
    state_handle->pre_pre_state = STATE_IDLE;

    state_handle->dryburn_reason = 0;
    state_handle->shake_status_flag = 0;
    state_handle->shake_down_flag = 0;
    state_handle->after_shake_down_temp = 0;
    state_handle->after_shake_down_timestamp = 0;
    state_handle->heat_pot_flag = 0;
    // state_handle->fry_no_shake_flag2 = 0;
    // state_handle->fry_no_shake_timestamp = 0;
    // state_handle->fry_no_shake_down_temp = 0;
    state_handle->very_very_gentle_temp_longest_clear_timestamp = 0;
    state_handle->high_temp_shake_flag = 0;

    state_handle->max_gentle_average_temp = 0;
    state_handle->water_down_flag = 0;

    state_handle->pan_fire_state = PAN_FIRE_CLOSE;
    state_handle->pan_fire_high_temp_exit_lock_tick = 0;
    state_handle->pan_fire_rise_jump_exit_lock_tick = 0;
    state_handle->pan_fire_first_error = 0;
    state_handle->pan_fire_error_lock_tick = 0;

    state_handle->temp_control_first = 0;
    state_handle->temp_control_lock_countdown = 0;
    state_handle->water_down_timestamp = 0;

    state_handle->shake_permit_start_tick = 0;
    state_handle->shake_exit_tick = 0;
    state_handle->shake_long = 0;
    state_handle->shake_exit_average_temp = 0;
    state_handle->shake_exit_timestamp = 0;
    state_handle->shake_status_average = 0;
    //state_handle->enter_gentle_prepare_temp = ENTER_GENTLE_INIT_VALUE;

    // state_handle->ignition_switch = 0;
    // state_handle->ignition_switch_close_temp = 0;


    state_handle->current_tick = 0;
    state_handle->total_tick = 0;
    state_handle->tick_for_dryburn = 0;

    state_handle->judge_flag = 0;
    state_handle->judge_30_temp = 0;
    state_handle->judge_time = 0;
    state_handle->dryburn_fry_uptemp = 0;
    state_handle->shake_state_count = 0;

    log_debug("这里是cook_assistant_reinit调用开外环火");
    set_fire_gear(FIRE_BIG, state_handle, 1);
    state_handle->fire_gear = FIRE_BIG;
    state_handle->hood_gear = GEAR_CLOSE;

    memset(state_handle->last_average_temp_arr,0,sizeof(state_handle->last_average_temp_arr));
    state_handle->arr_size = 0;

    //state_handle->last_arr_average = 0;

    state_handle->dryburn_gentle_temp = 0;
    state_handle->dryburn_gentle_tick = 0;
    state_handle->fire_status = 0;
    //state_handle->dryburn_state = 0;    //不可再此处将状态置为0，否则关火时先置零，在关火后change_state函数中无法判断到1的状态

    // state_hood.gear = GEAR_CLOSE;
    // state_hood.prepare_gear = GEAR_INVALID;
    // state_hood.gear_tick = 0;
}
/***********************************************************
 * 初始化函数
 ***********************************************************/
void cook_assistant_init(enum INPUT_DIR input_dir)
{
    log_debug("%s,cook_assistant_init", __func__);
    state_handle_t *state_handle = get_input_handle(input_dir);
    ring_buffer_init(&state_handle->ring_buffer, STATE_JUDGE_DATA_SIZE, 2);
    state_handle->input_dir = input_dir;
    cook_assistant_reinit(state_handle);
    ca_dryburn_reinit(state_handle);

    state_handle->ignition_switch = 0;
    state_handle->ignition_switch_close_temp = 0;

    //初始化设置此温度为较高温度，档位一直打开的情况下，防止高温烹饪时候通讯板发生复位时，没有得到正确的开火前温度，而直接误触移锅小火逻辑
    state_handle->before_ignition_temp_for_panfire = 380 * 10;        

    //state_handle->last_arr_average = 0;
    //state_handle->dry_burn_switch = 1;      //暂时手动设置为1，打开防干烧
    state_handle->shake_exit_average_temp = 0;

    //state_handle->enter_gentle_prepare_temp = ENTER_GENTLE_INIT_VALUE;
    
    memset(state_handle->last_average_temp_arr,0,sizeof(state_handle->last_average_temp_arr));
    state_handle->arr_size = 0;

    if (input_dir == INPUT_LEFT)
        set_pan_fire_switch(0, input_dir);
    else
        set_pan_fire_switch(0, input_dir);

    set_temp_control_target_temp(150, input_dir);

    state_handle->pan_fire_close_delay_tick = 180;
}


/*
 * @brief 预切换状态判断函数
 * @param 烹饪助手状态结构体
 * @param 最新温度数组
 * @param 数据长度，这里固定取最新的10个温度数据
 * @return 要返回的预切换状态
*/
static int status_judge(state_handle_t *state_handle, const unsigned short *data, const int len)
{
#define START (2)
    log_debug("[%s],total_tick:%d pan_fire_state:%d pan_fire_first_error:%d", __func__, state_handle->total_tick, state_handle->pan_fire_state, state_handle->pan_fire_first_error);
    log_debug("[%s],ignition_switch_close_temp:%d last_temp:%d,last average_temp:%d", __func__, state_handle->ignition_switch_close_temp, state_handle->last_temp,state_handle->last_temp_average);

    // STATE_PAN_FIRE
    //移锅小火的前提：温控开关关闭 且 移锅小火开启 且 移锅小火是正常关闭 且 火焰是大火
    if (!state_handle->temp_control_switch && state_handle->pan_fire_switch && state_handle->pan_fire_state == PAN_FIRE_CLOSE && state_handle->fire_gear == FIRE_BIG)
    {
        //点火1-5s内最新温度数值大于120℃ 且 最新温度数据大于开火前温度50℃以上
        if (state_handle->total_tick >=1 * INPUT_DATA_HZ && state_handle->total_tick <= 7 * INPUT_DATA_HZ && data[len - 1] > 1200 && (data[len - 1] > state_handle->before_ignition_temp_for_panfire + 500))
        //if (state_handle->total_tick <= 5 * INPUT_DATA_HZ && data[len - 1] > 1200 && (data[len - 1] > state_handle->ignition_switch_close_temp + 500))
        {
            log_debug("%s judge STATE_PAN_FIRE", __func__);
            state_handle->pan_fire_enter_type = 0;
            return STATE_PAN_FIRE;
        }
        //如果最新温度大于380℃上限则直接返回移锅小火
        else if (data[len - 1] >= 3800)
        {
            log_debug("%s judge STATE_PAN_FIRE", __func__);
            state_handle->pan_fire_enter_type = 0;
            return STATE_PAN_FIRE;
        }
    }

    int i;
    //10组温度数据的平均值
    int average = 0;
    //最新八组温度数据的平均值
    int gentle_average = 0;
    //前五组温度的平均值
    int gentle_average_before = 0;
    //后五组温度数据的平均值
    int gentle_average_after = 0;

    for (i = 0; i < len; ++i)
    {
        average += data[i];
        //        log_debug("%d ", data[i]);
        if (i < len / 2)
        {
            gentle_average_before += data[i];
        }
        else
        {
            gentle_average_after += data[i];
        }
        if (i >= START)
        {
            gentle_average += data[i];
        }
    }
    average /= len;
    gentle_average /= (len - START);            //最新八组温度数据的平均值
    gentle_average_before /= (len / 2);
    gentle_average_after /= (len / 2);
    //    log_debug("");
    //log_debug("%s,average:%d,gentle_average:%d,gentle_average_before:%d,gentle_average_after:%d", __func__, average, gentle_average, gentle_average_before, gentle_average_after);

    // STATE_PAN_FIRE
    unsigned short min = data[len - 1];
    unsigned short max = data[len - 1];
    //初始化温度方向
    char data_average = data[len - 1] > average;
    //平均值方向变变化计数
    char data_average_count = 0;

    //温度变化趋势
    char data_trend = data[len - 2] > data[len - 1];
    //温度趋势变化计数
    char data_trend_count = 0;

    log_debug("%s,pan_fire_high_temp_exit_lock_tick:%d", __func__, state_handle->pan_fire_high_temp_exit_lock_tick);

    //锁定时间为0
    if (!state_handle->temp_control_switch && state_handle->pan_fire_switch && state_handle->pan_fire_state == PAN_FIRE_CLOSE && state_handle->fire_gear == FIRE_BIG && state_handle->pan_fire_high_temp_exit_lock_tick == 0)
    {
        for (i = len - 2; i > 0; --i)
        {
            if (data[i] < min)
                min = data[i];
            else if (data[i] > max)
                max = data[i];

            //数据变化方向统计
            if (data[i - 1] > data[i])     
            {
                if (data_trend == 0)
                    ++data_trend_count;
                data_trend = 1;
            }
            else
            {
                if (data_trend == 1)
                    ++data_trend_count;
                data_trend = 0;
            }

            //温度数值变化穿过平均值，方向至少变化三次
            if (data_trend_count >= 3)
            {
                log_debug("%s,shake pan_fire max:%d min:%d", __func__, max, min);

                //点火10s内平均温度大于等于170℃ 且 温度差值（10组中最大和最小的差值）大于等于100℃
                if (state_handle->total_tick <= INPUT_DATA_HZ * 10 && state_handle->pan_fire_first_error == 0)
                {
                    if (average < 1700 || max - min < 1000)            //alter by Zhouxc
                        break;
                }
                //点火40s内平均温度大于200℃ 且 温度差值大于100℃
                else if (state_handle->total_tick <= START_FIRST_MINUTE_TICK && state_handle->pan_fire_first_error == 0)
                {
                    if (average < PAN_FIRE_LOW_TEMP || max - min < 1000)
                        break;
                }
                //点火80s内平均温度大于等于230℃ 且 温度差值大于100℃
                else if (state_handle->total_tick <= INPUT_DATA_HZ * 80 && state_handle->pan_fire_first_error == 0)
                {
                    if (average < PAN_FIRE_HIGH_TEMP || max - min < 1000)
                        break;
                }
                else
                {
                    //平均温度大于250℃ 且 温差大于等于100℃
                    if (average < 2500 || max - min < 1000)
                        break;
                }
                // if (max - min < 1500)
                // {

                //不满足break条件才会走到这里返回STATE_PAN_FIRE
                state_handle->pan_fire_enter_type = 1;
                return STATE_PAN_FIRE;


                // }
                // break;
            }
        }
    }

    //STATE_SHAKE
    log_debug("%s,shake_permit_start_tick:%d", __func__, state_handle->shake_permit_start_tick);
    //翻炒前提：移锅小火关闭 且 翻炒允许开始时间不等于0（达到允许翻炒温度） 且 点火开关打开10s后，翻炒允许时间开始后40s内才会判断是否需要返回SHAKE状态
    if (state_handle->pan_fire_state <= PAN_FIRE_ERROR_CLOSE && state_handle->shake_permit_start_tick != 0 && state_handle->total_tick > 10 * INPUT_DATA_HZ && state_handle->total_tick < state_handle->shake_permit_start_tick + INPUT_DATA_HZ * 40)
    {
        min = data[len - 1];
        max = data[len - 1];
        //去头去尾取中间8组数据
        for (i = len - 2; i > 0; --i)
        {
            if (data[i] < min)
                min = data[i];
            else if (data[i] > max)
                max = data[i];

            //温度数据变化方向，与平均值比较搭配标志位来对温度变化方向进行计数
            if (data[i] > average)
            {
                if (data_average == 0)
                    ++data_average_count;
                data_average = 1;
            }
            else
            {
                if (data_average > 0)
                    ++data_average_count;
                data_average = 0;
            }
            //log_debug("[%s],shake data_direction:%d",__func__,data_average_count);
            //温度数据变化4次及以上，无论是否满足返回SHAKE的条件都将结束对shake的判断
            if (data_average_count >= 4)
            {
                log_debug("%s,shake max:%d min:%d", __func__, max, min);
                //平均温度大于43℃ 且 温差大于8摄氏度  remark by Zhouxc 这里判断较为简单会导致容易误入翻炒状态
                if (average > 430)
                {
                    if (max - min > 120)            //alter by Zhouxc解决误入翻炒的问题，将80修改为110
                    {
                        state_handle->shake_permit_start_tick = state_handle->total_tick;
                        state_handle->state_jump_temp = average;
                        return STATE_SHAKE;
                    }
                }
                else
                {
                    break;
                }
            }
        }
    }

    // STATE_DOWN_JUMP
    signed short diff0_0, diff0_1, diff0_2, diff1_0, diff1_1, diff1_2, diff2_0, diff2_1, diff2_2;
    signed short before, after;
    signed short JUMP_RISE_VALUE = 250, JUMP_DOWN_VALUE = -250;

    if ((state_handle->state == STATE_GENTLE || (state_handle->state == STATE_RISE_SLOW && data[len - 1] - data[0] < 50 && data[len - 1] - data[0] > 0)) && state_handle->current_tick >= INPUT_DATA_HZ * 2 && state_handle->last_temp < 1000 && state_handle->last_temp > 650)
    {
#ifndef BOIL_ENABLE
        // JUMP_DOWN_VALUE = -150;
        JUMP_RISE_VALUE = 90;               //alter by Zhouxc 干烧时刻防止误入RISE_JUMP 30修改为50 50再次修改为90
#endif
    }
    else if (state_handle->state == STATE_SHAKE)
    {
        JUMP_DOWN_VALUE = -350;
        JUMP_RISE_VALUE = 350;
    }

    for (i = len - 1; i >= 6; --i)
    {
        // if (state_handle->pan_fire_state <= PAN_FIRE_ERROR_CLOSE && state_handle->state != STATE_GENTLE)
        if (0) // JUMP_RISE_VALUE <= 50
        {
            before = data[i - 4];
            after = data[i - 1];
            diff0_0 = data[i] - data[i - 3];
            diff0_1 = data[i] - data[i - 4];
            diff0_2 = data[i] - data[i - 5];

            diff1_0 = data[i - 1] - data[i - 3];
            diff1_1 = data[i - 1] - data[i - 4];
            diff1_2 = data[i - 1] - data[i - 5];

            diff2_0 = data[i - 2] - data[i - 3];
            diff2_1 = data[i - 2] - data[i - 4];
            diff2_2 = data[i - 2] - data[i - 5];
            // signed short diff_jump = abs(diff2_0);
            // if (abs(data[i - 3] - data[i - 5]) >= diff_jump * 0.7)
            // {
            //     continue;
            // }
            // else if (abs(data[i] - data[i - 2]) >= diff_jump * 0.6)
            // {
            //     continue;
            // }
            // if (diff0_2 < 0)
            // {
            //     if (abs(data[i] - data[i - 5]) >= diff_jump * 1.7)
            //     {
            //         continue;
            //     }
            // }
            // else
            // {
            //     if (state_handle->state == STATE_RISE_SLOW || state_handle->state == STATE_RISE_JUMP)
            //     {
            //         if (abs(data[i] - data[i - 5]) >= diff_jump * 1.3)
            //         {
            //             continue;
            //         }
            //     }
            //     else
            //     {
            //         if (abs(data[i] - data[i - 5]) >= diff_jump * 1.5)
            //         {
            //             continue;
            //         }
            //     }
            // }
        }
        else
        {
            before = data[i - 5];
            after = data[i - 1];
            diff0_0 = data[i] - data[i - 4];
            diff0_1 = data[i] - data[i - 5];
            diff0_2 = data[i] - data[i - 6];

            diff1_0 = data[i - 1] - data[i - 4];
            diff1_1 = data[i - 1] - data[i - 5];
            diff1_2 = data[i - 1] - data[i - 6];

            diff2_0 = data[i - 2] - data[i - 4];
            diff2_1 = data[i - 2] - data[i - 5];
            diff2_2 = data[i - 2] - data[i - 6];

            // signed short diff_jump = abs(diff2_0);
            // if (fabs(data[i - 4] - data[i - 6]) >= diff_jump * 0.7)
            // {
            //     continue;
            // }
            // else if (fabs(data[i] - data[i - 2]) >= diff_jump * 0.5)
            // {
            //     continue;
            // }
            // else if (fabs(data[i] - data[i - 6]) >= diff_jump * 1.7)
            // {
            //     continue;
            // }
        }
        log_debug("%s,i:%d JUMP_DOWN_VALUE:%d JUMP_RISE_VALUE:%d diff2_0:%d", __func__, i, JUMP_DOWN_VALUE, JUMP_RISE_VALUE, diff2_0);
        state_handle->state_jump_diff = abs(diff2_0);
        if (diff2_0 < 0)
        {
            unsigned char jump_down = diff0_0 < JUMP_DOWN_VALUE && diff0_1 < JUMP_DOWN_VALUE && diff0_2 < JUMP_DOWN_VALUE;
            jump_down += diff1_0 < JUMP_DOWN_VALUE && diff1_1 < JUMP_DOWN_VALUE && diff1_2 < JUMP_DOWN_VALUE;
            jump_down += diff2_0 < JUMP_DOWN_VALUE && diff2_1 < JUMP_DOWN_VALUE && diff2_2 < JUMP_DOWN_VALUE;

            if (jump_down >= 3)
            {
                state_handle->state_jump_temp = before;
                log_debug("%s,judge STATE_DOWN_JUMP state_jump_temp:%d", __func__, state_handle->state_jump_temp);

                if (state_handle->pan_fire_state == PAN_FIRE_ERROR_CLOSE)
                    state_handle->pan_fire_state = PAN_FIRE_CLOSE;

                return STATE_DOWN_JUMP;
            }
        }
        else
        {
            // STATE_RISE_JUMP
            unsigned char jump_rise = diff0_0 >= JUMP_RISE_VALUE && diff0_1 >= JUMP_RISE_VALUE && diff0_2 >= JUMP_RISE_VALUE;
            jump_rise += diff1_0 >= JUMP_RISE_VALUE && diff1_1 >= JUMP_RISE_VALUE && diff1_2 >= JUMP_RISE_VALUE;
            jump_rise += diff2_0 >= JUMP_RISE_VALUE && diff2_1 >= JUMP_RISE_VALUE && diff2_2 >= JUMP_RISE_VALUE;
            if ((JUMP_RISE_VALUE <= 50 && jump_rise >= 2) || jump_rise >= 3)
            {
                state_handle->state_jump_temp = after;
                log_debug("%s,i:%d judge STATE_RISE_JUMP state_jump_temp:%d %d %d", __func__, i, state_handle->state_jump_temp, state_handle->fire_gear, state_handle->pan_fire_rise_jump_exit_lock_tick);

                if (!state_handle->temp_control_switch && state_handle->pan_fire_switch && state_handle->pan_fire_state == PAN_FIRE_CLOSE && state_handle->fire_gear == FIRE_BIG && state_handle->pan_fire_rise_jump_exit_lock_tick == 0)
                {
                    if (abs(diff2_0) > 500)
                    {

                        // if (state_handle->total_tick <= INPUT_DATA_HZ * 10 && after < 1200)  //1500 1350
                        //     continue;
                        // else if (state_handle->total_tick <= INPUT_DATA_HZ * 40 && after < 1200) //1800 1600
                        //     continue;
                        // else if (state_handle->total_tick <= INPUT_DATA_HZ * 80 && after < 1200)
                        //     continue;
                        // else if (after < 1200)  //2200
                        //     continue;

                        if (state_handle->total_tick <= INPUT_DATA_HZ * 10)  //1500 1350                            
                        {
                            if (after < 1300)  //1500
                                continue;
                        }
                        else if (state_handle->total_tick <= INPUT_DATA_HZ * 40) //1800 1600
                        {
                            if (after < 1800)
                                continue;
                        }
                        else if (state_handle->total_tick <= INPUT_DATA_HZ * 80 )
                        {
                            if (after < 2000)
                                continue;
                        }
                        else if (after < 2200)  //2200
                            continue;

                        if (state_handle->pan_fire_state == PAN_FIRE_ERROR_CLOSE)
                            state_handle->pan_fire_state = PAN_FIRE_CLOSE;

                        state_handle->pan_fire_enter_type = 0;
                        return STATE_PAN_FIRE;
                    }
                }
                signed short diff_jump = abs(diff2_0);
                if (diff_jump > 6 && abs(data[i] - data[i - 6]) > diff_jump * 2.5)
                {
                    continue;
                }
                if (after > 650)
                {
                    if (state_handle->pan_fire_state == PAN_FIRE_ERROR_CLOSE)
                        state_handle->pan_fire_state = PAN_FIRE_CLOSE;

                    if ((state_hood.gear > GEAR_CLOSE))
                        return STATE_RISE_JUMP;
                }
            }
        }
    }
    // STATE_RISE_SLOW
#ifdef BOIL_ENABLE
    state_handle->boil_gengle_state = 0;
    state_handle->boil_gengle_small_state = 0;
#endif
#define STEP (3)
    signed short slow_rise_value = -200;

    signed short diff0 = data[0] - data[len - 1];
    signed short diff1 = data[START] - data[START + STEP];
    signed short diff2 = data[START + STEP] - data[START + STEP * 2];
    //signed short diff3 = data[START + STEP * 3];

    // signed short diff3 = data[START+STEP * 2] - data[START+STEP * 3];
    if (diff0 < 0)
    {
        // if (state_handle->pan_fire_state <= PAN_FIRE_ERROR_CLOSE)
        // {
        unsigned char slow_rise = diff0 <= -10 && diff0 > slow_rise_value;
        slow_rise += diff1 <= 0;
        slow_rise += diff2 <= 0;
        //slow_rise += diff3 <= 0;
        if (slow_rise >= 3)
        {
#ifdef BOIL_ENABLE
            // state_handle->boil_gengle_state=0x0f;
            // state_handle->boil_gengle_small_state = 0x0f;
#endif
            //最新七组数据中第一组是最小值，最后一组是最大值
            for (i = START + 1; i < len - 1; ++i)
            {
                if (data[START] > data[i])
                    break;
                if (data[len - 1] < data[i])
                    break;
            }
            if (i >= len - 1)
                return STATE_RISE_SLOW;
        }
        // }
        // else
        // {
        //     log_debug("%s,status_judge RISE_SLOW pan_fire_state:%d", __func__, state_handle->pan_fire_state);
        //     if (data[len-1] > data[START])
        //     {
        //         for (i = START; i < len-1; ++i)
        //         {
        //             if (data[i] > data[i + 1])
        //                 break;
        //         }
        //         if (i >= len-1)
        //             return STATE_RISE_SLOW;
        //     }
        // }
    }
    else
    {
        // STATE_DOWN_SLOW
        if (state_hood.gear > GEAR_CLOSE)
        {
            unsigned char slow_down = diff0 > 10 && diff0 < 100;
            slow_down += diff1 >= 0;
            slow_down += diff2 >= 0;
            // slow_down += diff3 >= 0;
            //满足以上三个条件，第三组数据大于最后一组数据1-10；第三组数据大于第五组数据；第五组数据大于第七组数据
            if (slow_down >= 3)
            {
                //第三组开始
                for (i = START + 1; i < len - 1; ++i)
                {
                    if (data[START] < data[i])      //第三组数据需要始终大于等于第四组++
                        break;
                    if (data[len - 1] > data[i])    //最后一组需要始终小于等于第四组++
                        break;
                }
                if (i >= len - 1)
                    return STATE_DOWN_SLOW;
            }
        }
    }
    // STATE_GENTLE
    //烟机开启或者移锅小火
    if (state_hood.gear > GEAR_CLOSE || state_handle->pan_fire_state == PAN_FIRE_ENTER)
    {
#ifdef BOIL_ENABLE
        for (i = len - 1; i >= 0; --i)
        {
            if (abs(data[i] - average) >= 15)           //温度波大于1.5℃的数量放到boil_gengle_state
                ++state_handle->boil_gengle_state;
            if (abs(data[i] - average) < 9)             //温度波动小于0.9℃的数量放到boil_gengle_small_state
                ++state_handle->boil_gengle_small_state;
        }
#endif
        char gentle_range;

        //是否在移锅小火状态中平缓的条件是不一样的
        if (state_handle->pan_fire_state == PAN_FIRE_ENTER)
        {
            gentle_range = 13;      //移锅小火时是1.3
        }
        else
        {
            gentle_range = 17;      //否则是1.7
        }

        for (i = len - 1; i >= START; --i)
        {
            if (abs(data[i] - gentle_average) >= gentle_range)
            {
                break;
            }
        }
        //最新八组数据中每组数据与平均值的差值的绝对值都小于gentle_range则满足gentle状态
        if (i < START)
        {
            state_handle->state_jump_temp = gentle_average;
            log_debug("get gentle 1");
            return STATE_GENTLE;
        }
    }
    if (state_hood.gear > GEAR_CLOSE && state_handle->pan_fire_state <= PAN_FIRE_ERROR_CLOSE && state_handle->state != STATE_SHAKE)
    {
        if (abs(gentle_average_before - gentle_average_after) < 10)
        {
#ifdef BOIL_ENABLE
            state_handle->boil_gengle_state = 10;
#endif
            //取出前五组后五组中平均值较大的那组
            state_handle->state_jump_temp = gentle_average_before > gentle_average_after ? gentle_average_before : gentle_average_after;
            log_debug("get gentle 2");
            return STATE_GENTLE;
        }
    }

    return STATE_IDLE;
}
static void temp_control_func(state_handle_t *state_handle)
{
    int i, average;

    if (state_handle->temp_control_lock_countdown < TEMP_CONTROL_LOCK_TICK)
    {
        ++state_handle->temp_control_lock_countdown;
    }
    // if (state_handle->pan_fire_state > PAN_FIRE_ERROR_CLOSE)
    // {
    //     return;
    // }
    // if (state_handle->last_temp < state_handle->temp_control_target_value / 2)
    // {
    //     for (i = 0; i < INPUT_DATA_HZ; ++i)
    //     {
    //         if (state_handle->last_temp_data[state_handle->temp_data_size - 1 - i] > state_handle->temp_control_target_value / 2)
    //         {
    //             break;
    //         }
    //     }
    //     if (i < INPUT_DATA_HZ)
    //     {
    //         return;
    //     }
    // }
    average = state_handle->last_temp;
    for (i = 1; i < 4; ++i)
    {
        average += state_handle->last_temp_data[state_handle->temp_data_size - 1 - i];
    }
    average /= 4;
    log_debug("%s,temp_control_func average:%d", __func__, average);
    if (state_handle->temp_control_first == 0)
    {
        if (state_handle->temp_control_target_value - 40 < average)
        {
            state_handle->temp_control_enter_start_tick = INPUT_DATA_HZ * 60 * 2;
            state_handle->temp_control_first = 1;
            LOGI("mars", "精准控温1: 温度首次到达(%d %d)", state_handle->temp_control_target_value, average);
            set_fire_gear(FIRE_SMALL, state_handle, 0);
            mars_beer_control(26);
            state_handle->temp_control_lock_countdown = 0;
        }
        else
        {
            LOGI("mars", "精准控温2: 这里是temp_control_func调用开外环火");
            set_fire_gear(FIRE_BIG, state_handle, 0);
            state_handle->temp_control_lock_countdown = 0;
        }
    }
    else
    {
        if (state_handle->temp_control_target_value + 10 < average)
        {
            if (state_handle->temp_control_lock_countdown >= TEMP_CONTROL_LOCK_TICK)
            {
                LOGI("mars", "精准控温3: 这里是temp_control_func调用关外环火");
                set_fire_gear(FIRE_SMALL, state_handle, 0);
                state_handle->temp_control_lock_countdown = 0;
            }
        }
        else if (state_handle->temp_control_target_value - 40 > average)
        {
            //必须满足达到锁定时间 或者 最新平均温度小于目标温度的差值 大于目标温度的10分之一就立即切换开大火
            if (state_handle->temp_control_lock_countdown >= TEMP_CONTROL_LOCK_TICK || abs(state_handle->temp_control_target_value - average) > state_handle->temp_control_target_value * 0.1)
            {
                LOGI("mars", "精准控温4: 这里是temp_control_func调用开外环火");
                set_fire_gear(FIRE_BIG, state_handle, 0);
                state_handle->temp_control_lock_countdown = 0;
            }
        }
    }

    if (state_handle->temp_control_enter_start_tick < INPUT_DATA_HZ * 60 * 2)
    {
        ++state_handle->temp_control_enter_start_tick;
        if (state_handle->temp_control_enter_start_tick == INPUT_DATA_HZ * 60 * 2)
        {
            if (cook_assist_remind_cb != NULL)
                cook_assist_remind_cb(0);
        }
    }
}


void save_average_arr(state_handle_t *state_handle)
{
    int i;
    if(state_handle->arr_size < BURN_DATA_SIZE)
    {
        state_handle->last_average_temp_arr[state_handle->arr_size] = state_handle->last_temp_average;
        state_handle->arr_size++;
    }
    else if(state_handle->arr_size == BURN_DATA_SIZE)
    {
        for(i=0;i<BURN_DATA_SIZE-1;i++)
        {
            state_handle->last_average_temp_arr[i] = state_handle->last_average_temp_arr[i+1];
        }
        state_handle->last_average_temp_arr[i] = state_handle->last_temp_average;
    }
}

//两种情况通知退出防干烧，0：用户点击任意键，1：灶具因为其他原因：定时关火时间到关火、一键烹饪相关逻辑关火等，包括天使用户自己触发干烧后时间到关火
void exit_dryburn_status(enum INPUT_DIR input_dir, uint8_t reason) //0:按键退出  1:指令退出
{
    
    state_handle_t *state_handle = NULL;
    state_handle = get_input_handle(input_dir);

    //Reset Dryburn judgement variable

    //set status IDLE
    //state_handle->state = STATE_IDLE;

    if(cook_dryburn_remind_cb != NULL)
    {
        
        //进入了防干烧后用户点击任意键
        if(reason == 0)
        {
            log_debug("user press button exit dryburn state");
            state_handle->dryburn_state = 0;
        }
        //天使用户时间到自行关火的时候dryburn_state=0，关火动作完成后，这时候不需要再次赋值，所以多一个判断dryburn_state
        else if(reason == 1 && state_handle->dryburn_state == 1)    
        {
            state_handle->dryburn_state = 2;
        }
        
    }

}

/***********************************************************
 * 风随烟动逻辑状态切换函数，250ms执行一次
 ***********************************************************/
static void change_state(state_handle_t *state_handle)
{
    unsigned char prepare_state, next_state;

    //将数据读取到数组中
    state_handle->temp_data_size = ring_buffer_peek(&state_handle->ring_buffer, state_handle->last_temp_data, STATE_JUDGE_DATA_SIZE);
    state_handle->last_temp = state_handle->last_temp_data[state_handle->temp_data_size - 1];
    state_handle->last_temp_average = 0;

    char peek_data_str[4 * STATE_JUDGE_DATA_SIZE + 20] = {0};  // 把一次温度的数组组装成字符串  4是因为一个温度最长4个数字  用来替换下面的 MLOG_INT_log_debug("ring_buffer_peek:", state_handle->last_temp_data, STATE_JUDGE_DATA_SIZE, 2);
    for(int i=0; i<10; i++)
    {
        state_handle->last_temp_average += state_handle->last_temp_data[i];
        // 日志统一新加的
        char s[10];
        sprintf(s, "%d ", state_handle->last_temp_data[i]);
        strcat(peek_data_str, s);
    }
    state_handle->last_temp_average /= 10;

    log_debug("ring_buffer_peek: %s", peek_data_str);
    

    // 点火开关判断
    if (!state_handle->ignition_switch)
    {
        #ifdef DRYBURN_ENABLE
        //if(state_handle->dryburn_state == 1)          //确保关火的时候及时通知，不再判断是否处于干烧状态
        {
            if(cook_dryburn_remind_cb != NULL)
            {
                //进入了防干烧后用户关火
                log_debug("user close fire,exit dryburn status");
                ca_dryburn_reinit(state_handle);
                cook_dryburn_remind_cb(2,state_handle->dryburn_reason);
            } 
        }
        #endif
        log_debug("change_state 函数中点火开关关闭");

        state_handle->temp_control_enter_start_tick = 0;
        state_handle->pan_fire_enter_start_tick = 0;
        //处于关火时会一直更新当前的温度，直到开火的前一瞬间
        state_handle->ignition_switch_close_temp = state_handle->last_temp_data[state_handle->temp_data_size - 5];
        state_handle->before_ignition_temp_for_panfire = state_handle->last_temp_data[state_handle->temp_data_size - 5];
        return;
    }
#ifdef DRYBURN_ENABLE
    // else if(state_handle->state == STATE_DRYBURN)       //fire open and state is Dryburn
    // {
    //     return;
    // }
#endif
    else
    {
        ++state_handle->total_tick;
        ++state_handle->tick_for_dryburn;
    }

    if (state_hood.gear == GEAR_CLOSE)
    {
        if (state_handle->last_temp > 500 && state_handle->state != STATE_PAN_FIRE && state_handle->total_tick > INPUT_DATA_HZ * 20)
            gear_change(1, 1, "init gear_change", state_handle);
    }
    // 控温
    if (state_handle->temp_control_switch && state_handle->temp_control_target_value > 0)
    {
        temp_control_func(state_handle);
    }
    if (state_hood.smart_smoke_switch == 0 && state_handle->pan_fire_switch == 0 && state_handle->dry_burn_switch == 0)
    {
        state_handle->state = STATE_IDLE;
        return;
    }
    // 翻炒允许温度
    if (state_handle->last_temp > SHAKE_PERMIT_TEMP)
    {
        state_handle->shake_permit_start_tick = state_handle->total_tick;
    }
    // 高温时，移锅小火退出后，锁定时间
    if (state_handle->pan_fire_high_temp_exit_lock_tick > 0)
    {
        --state_handle->pan_fire_high_temp_exit_lock_tick;
    }
    // 跳升退出移锅小火后，锁定时间
    if (state_handle->pan_fire_rise_jump_exit_lock_tick > 0)
    {
        --state_handle->pan_fire_rise_jump_exit_lock_tick;
    }
    // 移锅小火判断错误
    if (state_handle->pan_fire_state == PAN_FIRE_ERROR_CLOSE)
    {
        // 开始一分钟内,移锅小火第一次判断错误
        if (state_handle->total_tick < START_FIRST_MINUTE_TICK && state_handle->pan_fire_first_error == 0)
        {
            state_handle->pan_fire_first_error = 1;
        }
        // 移锅小火判断错误，锁定时间
        if (state_handle->pan_fire_error_lock_tick == 0)
            state_handle->pan_fire_error_lock_tick = state_handle->total_tick;
        else
        {
            if (state_handle->pan_fire_error_lock_tick + INPUT_DATA_HZ * 20 < state_handle->total_tick)
            {
                state_handle->pan_fire_error_lock_tick = 0;
                state_handle->pan_fire_state = PAN_FIRE_CLOSE;
            }
        }
    }

    //每隔2.5s采集一次最近十组数据的平均值
    if(state_handle->total_tick % BURN_DATA_FREQUENCY == 0)
    {
        save_average_arr(state_handle);
    } 
    log_debug("last_average_temp_size:%d",state_handle->arr_size);

    // 下一个状态判断
    prepare_state = status_judge(state_handle, &state_handle->last_temp_data[state_handle->temp_data_size - STATE_JUDGE_DATA_SIZE], STATE_JUDGE_DATA_SIZE);
    log_debug("[%s],prepare_state:%s", __func__, state_info[prepare_state]);

#ifdef DRYBURN_ENABLE
if(state_handle->dry_burn_switch != 0)
{
   if(dryburn_judge(prepare_state,state_handle) == STATE_DRYBURN)
   {
        prepare_state = STATE_DRYBURN;
   }
}
#endif

    /*当前状态非移锅小火且预切换状态是空闲（非移锅小火->空闲直接返回即可，移锅小火->空闲需要执行空闲函数）
    且当前不是干烧状态（若干烧中恰好返回可能导致state_func_dryburn函数中退出干烧的逻辑无法得到执行）*/
    if (prepare_state == STATE_IDLE && state_handle->state != STATE_PAN_FIRE && state_handle->state != STATE_DRYBURN)
    {
        if (state_handle->current_tick != 0)
        {
            ++state_handle->current_tick;
            if(state_handle->last_prepare_state == STATE_SHAKE)
            {
                state_handle->last_prepare_state = STATE_IDLE;
                state_handle->last_prepare_state_tick = state_handle->current_tick;
            }
        }
        return;
    }




    // 状态发生了变化就进行切换，执行当前的状态函数，prepare_state是根据温度得到的预切换状态，将该状态传入当前状态的状态函数，在状态函数中判断是否需要切换到prepare_state
    next_state = g_state_func_handle[state_handle->state](prepare_state, state_handle);

    
    // //开火30s内如果如果预切换状态时干烧，且温度小于230℃，不认为是干烧
    // if(state_handle->total_tick <= 120*INPUT_DATA_HZ && next_state == STATE_DRYBURN && state_handle->last_temp_average < 2300)
    // {
    //     next_state = state_handle->state;
    // }
    


    //log_debug("[%s],prepare_state:%s",__func__,state_info[prepare_state]);
    if (state_handle->state != next_state)
    {
        switch(next_state)
        {
            case STATE_SHAKE:
                state_handle->shake_state_count++;
            default:;
                //LOG("next state is ERROR");
        }
        log_debug("[%s],warning now state is:%s next_state changed 状态切换为:%s",__func__,state_info[state_handle->state],state_info[next_state]);
        state_handle->current_tick = 0;

        //第一次执行刚切换到的函数，由于必须有传入的参数所以将切换前的状态作为prepare传入，不接收返回结果
        g_state_func_handle[next_state](state_handle->state, state_handle);
        state_handle->pre_pre_state = state_handle->pre_state;
        state_handle->pre_state = state_handle->state;
        state_handle->state = next_state;
    }
}
/***********************************************************
 * 烟机预档位切换函数
 * 1.主要完成升降档延时切换
 ***********************************************************/
void prepare_gear_change_task()
{
    if (!state_hood.work_mode)
        return;
    log_debug("%s,prepare_gear:%d gear_tick:%d", __func__, state_hood.prepare_gear, state_hood.gear_tick);
    if (state_hood.prepare_gear != GEAR_INVALID)
    {
        gear_change(state_hood.prepare_gear, 0, "", NULL);
    }
    if (state_hood.gear_tick <= g_gear_delay_time)
    {
        ++state_hood.gear_tick;
    }

    if (state_hood.close_delay_tick > 0)
    {
        --state_hood.close_delay_tick;
        log_debug("%s,close_delay_tick:%d", __func__, state_hood.close_delay_tick);
        if (state_hood.close_delay_tick == 0)
        {
            gear_change(0, 3, "ignition switch close", NULL);
        }
    }
    log_debug("exit gear change task");
}

/***********************************************************
 * 输入函数，当前250ms被执行一次
 ***********************************************************/
void cook_assistant_input(enum INPUT_DIR input_dir, unsigned short temp, unsigned short environment_temp)
{
    log_debug("Rtemp:%d,Rentemp:%d",temp,environment_temp);

    state_handle_t *state_handle = get_input_handle(input_dir);
    // 重写的烹饪助手
    if (state_handle->ignition_switch != 0 && g_quick_cooking_app_is_open == 1)
    {
        void signal_module_start(unsigned short t);
        signal_module_start(temp);
    }
    // 中途用户关闭灶具时，重置所有重写的烹饪助手信息
    void reset_signal_module();
    if ((state_handle->ignition_switch == 0 && g_quick_cooking_app_is_open == 1) || (state_handle->ignition_switch == 1 && g_quick_cooking_app_is_open == 0))
        reset_signal_module();

    if (oil_temp_cb != NULL)
        oil_temp_cb(temp, input_dir);

    if (!state_hood.work_mode)
        return;

    //环境温度大于750摄氏度，档位锁定
    if (environment_temp > 750)
    {
        if (state_hood.lock == 0)
            gear_change(3, 4, "environment_temp lock", state_handle);
    }
    else
    {
        //温度小于750摄氏度且风机再运行则解除锁定
        if (state_hood.lock > 0)
            gear_change(2, 5, "environment_temp unlock", state_handle);
    }

    // 中途用户关闭灶具时，重置一键烹饪
    // if (!state_handle->ignition_switch)
    // {   
    //     reset_quick_cooking_value();
    //     return;
    // }

    ring_buffer_push(&state_handle->ring_buffer, &temp);
    if (!ring_buffer_is_full(&state_handle->ring_buffer))
    {
        log_debug("valid data less than 10");
        return;
    }
    
    log_debug("%s,%s input_dir:%d temp:%d dryburn_switch:%d total_tick:%d current_tick:%d current status:%s current gear:%d ignition_switch:%d environment_temp:%d ", get_current_time_format(), __func__, input_dir, temp, state_handle->dry_burn_switch, state_handle->total_tick, state_handle->current_tick, state_info[state_handle->state], state_hood.gear, state_handle->ignition_switch, environment_temp);
    log_debug("%s,%s,fire_status:%d,state_hood lock:%d,ca smart_smoke_switch:%d,ca hood gear:%d",get_current_time_format(),__func__,state_handle->fire_status,state_hood.lock,state_hood.smart_smoke_switch,state_hood.gear);
    change_state(state_handle);
}

