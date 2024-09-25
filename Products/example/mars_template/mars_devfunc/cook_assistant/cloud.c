/*
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-07-29 11:47:02
 * @LastEditors: Zhouxc
 * @LastEditTime: 2024-06-17 16:27:13
 * @FilePath     : /alios-things/Products/example/mars_template/mars_devfunc/cook_assistant/cloud.c
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cloud.h"
#include "fsyd.h"

#define log_module_name "fsyd"
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)
#define log_info(...)  LOGI(log_module_name, ##__VA_ARGS__)
#define log_warn(...)  LOGW(log_module_name, ##__VA_ARGS__)
#define log_error(...) LOGE(log_module_name, ##__VA_ARGS__)
#define log_fatal(...) LOGF(log_module_name, ##__VA_ARGS__)

static int (*work_mode_cb)(const unsigned char);
void register_work_mode_cb(int (*cb)(const unsigned char))
{
    work_mode_cb = cb;
}
static int (*smart_smoke_switch_cb)(const unsigned char);
void register_smart_smoke_switch_cb(int (*cb)(const unsigned char))
{
    smart_smoke_switch_cb = cb;
}
static int (*pan_fire_switch_cb)(const unsigned char, enum INPUT_DIR);
void register_pan_fire_switch_cb(int (*cb)(const unsigned char, enum INPUT_DIR))
{
    pan_fire_switch_cb = cb;
}

static int (*temp_control_switch_cb)(const unsigned char, enum INPUT_DIR);
void register_temp_control_switch_cb(int (*cb)(const unsigned char, enum INPUT_DIR))
{
    temp_control_switch_cb = cb;
}
static int (*temp_control_target_temp_cb)(const unsigned short, enum INPUT_DIR);
void register_temp_control_target_temp_cb(int (*cb)(const unsigned short, enum INPUT_DIR))
{
    temp_control_target_temp_cb = cb;
}
static int (*temp_control_p_cb)(const float);
void register_temp_control_p_cb(int (*cb)(const float))
{
    temp_control_p_cb = cb;
}
static int (*temp_control_i_cb)(const float);
void register_temp_control_i_cb(int (*cb)(const float))
{
    temp_control_i_cb = cb;
}
static int (*temp_control_d_cb)(const float);
void register_temp_control_d_cb(int (*cb)(const float))
{
    temp_control_d_cb = cb;
}

void set_work_mode(unsigned char mode)
{
    state_hood_t *hood_handle = get_hood_handle();
    log_debug("%s,set_work_mode:%d", get_current_time_format(), mode);
    if (hood_handle->work_mode == mode)
        return;
    hood_handle->work_mode = mode;

    if (hood_handle->work_mode)
    {
    }
    else
    {
        hood_handle->lock = 0;
        hood_handle->close_delay_tick = 0;
        hood_handle->prepare_gear = GEAR_INVALID;
        hood_handle->gear_tick = 0;
        state_handle_t *state_handle = get_input_handle(INPUT_LEFT);
        cook_assistant_reinit(state_handle);
        state_handle = get_input_handle(INPUT_RIGHT);
        cook_assistant_reinit(state_handle);
    }

    if (work_mode_cb != NULL)
        work_mode_cb(mode);
}

void set_smart_smoke_switch(unsigned char smart_smoke_switch)
{
    state_hood_t *hood_handle = get_hood_handle();
    log_debug("%s,set_smart_smoke_switch:%d", get_current_time_format(), smart_smoke_switch);
    hood_handle->smart_smoke_switch = smart_smoke_switch;
    if (smart_smoke_switch)
    {
    }
    else
    {
        hood_handle->lock = 0;
        hood_handle->close_delay_tick = 0;
        hood_handle->prepare_gear = GEAR_INVALID;
        hood_handle->gear_tick = 0;

        state_handle_t *state_handle = get_input_handle(INPUT_LEFT);
        state_handle->hood_gear = GEAR_CLOSE;
        state_handle->shake_permit_start_tick = 0;
        state_handle->shake_exit_tick = 0;
        state_handle->shake_long = 0;
        state_handle = get_input_handle(INPUT_RIGHT);
        state_handle->hood_gear = GEAR_CLOSE;
        state_handle->shake_permit_start_tick = 0;
        state_handle->shake_exit_tick = 0;
        state_handle->shake_long = 0;
    }
    if (smart_smoke_switch_cb != NULL)
        smart_smoke_switch_cb(smart_smoke_switch);
}

void set_pan_fire_delayofftime(unsigned int delayoff_time)
{
    state_hood_t *hood_handle = get_hood_handle();
    if (delayoff_time >= 60 && delayoff_time <= 300){
        hood_handle->delayoff_time_s = delayoff_time;
    }else{
        hood_handle->delayoff_time_s = 90;
    }
}

void set_pan_fire_switch(unsigned char pan_fire_switch, enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    log_debug("%s,set_pan_fire_switch:%d", get_current_time_format(), pan_fire_switch);
    state_handle->pan_fire_switch = pan_fire_switch;
    if (pan_fire_switch)
    {
    }
    else
    {
        if (state_handle->temp_control_switch == 0)
        {
            state_handle->pan_fire_state = PAN_FIRE_CLOSE;
            state_handle->pan_fire_high_temp_exit_lock_tick = 0;
            state_handle->pan_fire_rise_jump_exit_lock_tick = 0;
            state_handle->pan_fire_first_error = 0;
            state_handle->pan_fire_error_lock_tick = 0;
            log_debug("这里是set_pan_fire_switch调用开外环火");
            set_fire_gear(FIRE_BIG, state_handle, 0);
        }
    }
    if (pan_fire_switch_cb != NULL)
        pan_fire_switch_cb(pan_fire_switch, input_dir);
}
void set_pan_fire_close_delay_tick(unsigned short tick, enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    state_handle->pan_fire_close_delay_tick = tick;
}
void set_temp_control_switch(unsigned char temp_control_switch, enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    log_debug("%s,set_temp_control_switch:%d", get_current_time_format(), temp_control_switch);
    state_handle->temp_control_switch = temp_control_switch;
    if (temp_control_switch)
    {
        mars_AuxiliaryTemp(1);
    }
    else
    {
        state_handle->temp_control_target_value = 0;
        mars_AuxiliaryTemp(0);
        state_handle->temp_control_first = 0;
        state_handle->temp_control_lock_countdown = 0;
        if (state_handle->pan_fire_state <= PAN_FIRE_ERROR_CLOSE)
        {
            log_debug("这里是set_temp_control_switch调用开外环火");  
            set_fire_gear(FIRE_BIG, state_handle, 1);
        }
    }
    state_handle->temp_control_enter_start_tick = 0;
    if (temp_control_switch_cb != NULL)
        temp_control_switch_cb(temp_control_switch, input_dir);
}

void set_temp_control_target_temp(unsigned short temp, enum INPUT_DIR input_dir)
{
    state_handle_t *state_handle = get_input_handle(input_dir);
    state_handle->temp_control_target_value = temp * 10;
    log_debug("%s,set_temp_control_target_temp:%d", get_current_time_format(), state_handle->temp_control_target_value);
    if (temp_control_target_temp_cb != NULL)
        temp_control_target_temp_cb(temp, input_dir);
    state_handle->temp_control_enter_start_tick = 0;
}

/**
 * @description: 设置防干烧开关和防干烧用户类型
 * @param {unsigned char} fire_dry_switch   防干烧开关 0：关闭 1：打开
 * @param {enum INPUT_DIR} input_dir        设置对象：左右灶具，目前仅支持右灶
 * @param {enum DRYBURN_USER} dryburn_user_category 0：量产用户 1：天使用户
 * @return {*}
 */
//void set_dry_fire_switch(unsigned char fire_dry_switch,enum INPUT_DIR input_dir,enum DRYBURN_USER dryburn_user_category)

void set_dry_fire_switch(unsigned char fire_dry_switch, enum INPUT_DIR input_dir, uint8_t dryburn_user_category)
{
    printf("set dryburn switch:%d\r\n",fire_dry_switch);
    if(INPUT_RIGHT != input_dir)
    {
        log_debug("just support right stove,dryburn and user category switch set fail");
        return;
    }
    state_handle_t *state_handle = get_input_handle(input_dir);
    state_handle->dryburn_user_category = dryburn_user_category;

    if(fire_dry_switch != 0)
    {
        state_handle->dry_burn_switch = 1;
    }
    else
    {
        state_handle->dry_burn_switch = fire_dry_switch;
    }
}


/**
 * @application description:
 * 说明：并不能准确的判断火焰状态，默认旋钮打开的时候火焰是存在的，电控主动关火时候，通讯板通过电控上报的关火动作来调用此函数设定火焰状态。
 * 为了解决场景问题：用户开火后离开，随后设定定时关火，定时关火之后，用户一直没有回来将旋钮复位。随时间推移，满足了防干烧的时间兜底，这时候会
 * 触发防干烧提醒。其实定时关火早已关闭火焰，不符合用户认知的场景。且目前防干烧的警报只能本地解除，会一直蜂鸣，造成用户困扰。
 * 此外，不仅是定时关火，其他的报警或者故障关闭火焰之后，这时候时间兜底都不应该再次触发干烧提醒了。
 * 
 * @description: 火焰状态设置函数，
 * @param {enum INPUT_DIR} input_dir 左灶还是右灶
 * @param {uint8_t} status  火焰状态 0：火焰已经熄灭 1：火焰燃烧中
 * @return {*}
 */
void set_fire_status(enum INPUT_DIR input_dir,uint8_t status)
{
    if(status < 0 || status > 1)
    {
        log_debug("Error!set fire status(status=%d) is wrong!!!return",status);
        return;
    }

    if(input_dir != INPUT_RIGHT)
    {
        log_debug("Error!now only support right stove");
    }

    log_debug("set fire status is %d",status);
    state_handle_t *state_handle = get_input_handle(input_dir);
    state_handle->fire_status = status;
    
    return;
}


