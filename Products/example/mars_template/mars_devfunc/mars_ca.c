/*
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-07-28 15:42:59
 * @LastEditors: Zhouxc
 * @LastEditTime: 2024-06-17 14:13:15
 * @FilePath     : /et70-ca3/Products/example/mars_template/mars_devfunc/mars_ca.c
 */


#include <aos/aos.h>

#include "cook_assistant/cook_wrapper.h"
#include "cook_assistant/aux_api.h"
#include "cook_assistant/auxiliary_cook.h"
#include "mars_devmgr.h"
#include "../mars_driver/mars_uartmsg.h"
#include "mars_ca.h"

#define log_module_name "cook_assistant"
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)
#define log_info(...)  LOGI(log_module_name, ##__VA_ARGS__)
#define log_warn(...)  LOGW(log_module_name, ##__VA_ARGS__)
#define log_error(...) LOGE(log_module_name, ##__VA_ARGS__)
#define log_fatal(...) LOGF(log_module_name, ##__VA_ARGS__)

#define ONE_KEY_RECIPE_KV   "onekey_recipe"
#define PAN_FIRE_KV         "move_pan_fire"
#define DRY_BURN_KV         "dryburn_state"
#define AUXILIARY_KV        "auxiliary_temp"

static int Set_MutiValve_gear(enum INPUT_DIR input_dir,int gear);
static int set_beep_type(int beep_type);
static int aux_set_remaintime(unsigned char remaintime);
static int aux_set_exit_cb(int type);

extern mars_cook_assist_t  g_user_cook_assist;
static int cook_assistant_close_fire(enum INPUT_DIR input_dir)
{
    uint8_t buf_setmsg[10] = {0};
    uint16_t buf_len = 0;
    buf_setmsg[buf_len++] = prop_HoodFireTurnOff;  //灶具关火

    if (input_dir == INPUT_RIGHT){
        buf_setmsg[buf_len++] = 0x01;
    }
    
    Mars_uartmsg_send(cmd_set, uart_get_seq_mid(), buf_setmsg, buf_len, 3);
    return 0;
}

//临时设置的烟机档位回调函数
static int hood_gear_change(int gear)
{
    uint8_t buf_setmsg[10] = {0};
    uint16_t buf_len = 0;

    buf_setmsg[buf_len++] = prop_HoodLight;     //烟机照明
    buf_setmsg[buf_len++] = gear;

    Mars_uartmsg_send(cmd_set,uart_get_seq_mid(),buf_setmsg,buf_len,3);
    return 0;
}

/*
static int cook_assistant_hood_speed(const int gear)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();    
    uint8_t buf_setmsg[10] = {0};
    uint16_t buf_len = 0;
    //gear为-1，关闭风随烟动
    if(-1 == gear){
        if (fsyd_run_flag == 1)
        {
            mars_template_ctx->status.FsydStart = 0;
            mars_template_ctx->status.FsydValid = 0;
            set_smart_smoke_switch(mars_template_ctx->status.FsydStart);
            mars_set_ca_state();
            mars_propget_fun();
            mprintf_d("智能排烟: 手动调节档位,智能排烟单次退出 ==> 未开启 未运行");
        }
        else
        {
            mprintf_d("智能排烟: 输出-1 (未实际输出过,不响应)");
        }
    }else{
        if (gear >= mars_template_ctx->status.fsydHoodMinSpeed){    //正常控制
            mars_template_ctx->status.HoodSpeed = (uint8_t)gear;
            buf_setmsg[buf_len++] = prop_HoodSpeed;
            buf_setmsg[buf_len++] = mars_template_ctx->status.HoodSpeed;
            Mars_uartmsg_send(cmd_set, uart_get_seq_mid(), buf_setmsg, buf_len, 3);
            mprintf_d("智能排烟: 自动调节档位到 %d 档", gear);
            fsyd_run_flag = 1;
        }else{
            mprintf_d("智能排烟: 自动调节档位错误,过低. %d < %d", gear, mars_template_ctx->status.fsydHoodMinSpeed);
            //recv_ecb_gear(mars_template_ctx->status.fsydHoodMinSpeed);
        }
    }

    return 0;
}
*/

static uint8_t  lower_gear_last   = 0;
static aos_timer_t ca_delay_timer = {.hdl = NULL};
static void send_gear_work(void *p)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();  
    if (mars_template_ctx->status.RStoveStatus == 0 || mars_template_ctx->status.SysPower == 0)
    {
        LOGW("mars", "智能排烟: 不执行延迟的降档指令(因为右灶关闭或者系统关闭)");
    }
    else if (lower_gear_last == mars_template_ctx->status.HoodSpeed)
    {
        LOGW("mars", "智能排烟: 不执行延迟的降档指令(因为烟机档位就是%d档)", lower_gear_last);
    }
    else
    {
        uint8_t  buf_msg[8] = {0};
        uint16_t buf_len = 0;
        buf_msg[buf_len++] = prop_HoodSpeed;
        buf_msg[buf_len++] = lower_gear_last;
        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(), buf_msg, buf_len, 3);
        LOGI("mars", "智能排烟: 执行延迟的降档指令(%d档)", lower_gear_last);
    }

    aos_timer_stop(&ca_delay_timer);
    aos_timer_free(&ca_delay_timer);
}

/*
    烟极档位切换回调函数
*/
static int cook_assistant_hood_speed(int gear)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();    
    uint8_t buf_setmsg[8] = {0};
    uint16_t buf_len = 0;
    
    if (mars_template_ctx->status.RStoveStatus == 0 || mars_template_ctx->status.SysPower == 0)
    {
        LOGI("mars", "智能排烟: 调节烟机档位 = %d (由于右灶关闭或者系统关闭,忽略本次调节!!!)", gear);
        return 0;
    }

    if (gear == mars_template_ctx->status.HoodSpeed)
    {
        LOGI("mars", "智能排烟: 调节烟机档位 = %d (由于烟机已处于本档位,忽略本次调节!!!)", gear);
        return 0;
    }

    if (gear < mars_template_ctx->status.HoodSpeed)
    {
        LOGI("mars", "智能排烟: 降档 (当前档位=%d 调节档位=%d)", mars_template_ctx->status.HoodSpeed, gear);
        if (ca_delay_timer.hdl == NULL)
        {
            lower_gear_last = gear;
            aos_timer_new_ext(&ca_delay_timer, send_gear_work, NULL, 20*1000, 0, 0);
            aos_timer_start(&ca_delay_timer);
            LOGI("mars", "智能排烟: 20秒后调节到烟机新档位%d", gear);
        }
        else
        {
            LOGI("mars", "智能排烟: 更新已经存在的降档指令(原有降档=%d 最新降档=%d)", lower_gear_last, gear);
            lower_gear_last = gear;
        }

        return 0;
    }
    else
    {
        LOGI("mars", "智能排烟: 升档 (当前档位=%d 调节档位=%d)", mars_template_ctx->status.HoodSpeed, gear);
        buf_setmsg[buf_len++] = prop_HoodSpeed;
        buf_setmsg[buf_len++] = gear;
        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(), buf_setmsg, buf_len, 3);

        if (ca_delay_timer.hdl == NULL)
        {
            aos_timer_stop(&ca_delay_timer);
            aos_timer_free(&ca_delay_timer);
        }
    }

    return 0;
}

extern int(*multi_valve_cb)(enum INPUT_DIR input_dir, int gear);
static int cook_assistant_fire_speed(const int gear, enum INPUT_DIR input_dir)
{
    // mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();  
    // uint8_t buf_setmsg[10] = {0};
    // uint16_t buf_len = 0;

    // // if (mars_template_ctx->status.RStoveStatus == 0)
    // // {
    // //     LOGI("mars", "烹饪助手: 操作通断阀 = %d (由于右灶关闭,忽略本次调节!!!)", gear);
    // //     return 0;
    // // }
    
    // buf_setmsg[buf_len++] = prop_RStoveSwitch;//右灶通断阀
    // buf_setmsg[buf_len++] = gear;

    if(gear == FIRE_SMALL)
    {
        //最小火
        if(multi_valve_cb != NULL)
            multi_valve_cb(INPUT_RIGHT,0x07);
    }
    else if(gear == FIRE_BIG)
    {
        //最大火
        if(multi_valve_cb != NULL)
            multi_valve_cb(INPUT_RIGHT,0x00);
    }

    

    // Mars_uartmsg_send(cmd_set, uart_get_seq_mid(), buf_setmsg, buf_len, 3);
    //LOGI("mars", "烹饪助手: 操作通断阀 = %d", gear);  
    LOGI("mars", "烹饪助手: 操作八段阀 = %d", (gear?0x00:0x07));   
    return 0;
}

static int cook_work_mode_cb(const unsigned char mode)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    
    // mars_template_ctx->status.RStOvMode = mode;

    // if (IOT_DevStateReport(&g_ecb_dev_status_buff) != CMN_RET_OK)
    // {
    //     ESP_LOGE(TASK_TAG, "report_temp IOT_DevStateReport error\n");
    // }
    return 0;
}
static int cook_pan_fire_switch_cb(const unsigned char pan_fire_switch, enum INPUT_DIR input_dir)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();

    if (input_dir == INPUT_LEFT)
    {
    }
    else
    {
        // mars_template_ctx->status.right_pan_fire = pan_fire_switch;
        // g_ecb_dev_status_buff.valid_bits |= VALID_BIT(CMN_DSVB_RIGHT_PAN_FIRE);

        // if (IOT_DevStateReport(&g_ecb_dev_status_buff) != CMN_RET_OK)
        // {
        //     ESP_LOGE(TASK_TAG, "report_temp IOT_DevStateReport error\n");
        // }
    }
    return 0;
}
static int cook_dry_switch_cb(const unsigned char dry_switch, enum INPUT_DIR input_dir)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();

    if (input_dir == INPUT_LEFT)
    {
    }
    else
    {
        // mars_template_ctx->status.right_dry = dry_switch;
        // g_ecb_dev_status_buff.valid_bits |= VALID_BIT(CMN_DSVB_RIGHT_DRY);

        // if (IOT_DevStateReport(&g_ecb_dev_status_buff) != CMN_RET_OK)
        // {
        //     ESP_LOGE(TASK_TAG, "report_temp IOT_DevStateReport error\n");
        // }
    }
    return 0;
}
static int cook_temp_control_switch_cb(const unsigned char temp_control_switch, enum INPUT_DIR input_dir)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();

    if (input_dir == INPUT_LEFT)
    {
    }
    else
    {
        g_user_cook_assist.RAuxiliarySwitch = temp_control_switch;
        // g_ecb_dev_status_buff.valid_bits |= VALID_BIT(CMN_DSVB_RIGHT_TEMP_CONTROL);

        // if (IOT_DevStateReport(&g_ecb_dev_status_buff) != CMN_RET_OK)
        // {
        //     ESP_LOGE(TASK_TAG, "report_temp IOT_DevStateReport error\n");
        // }
    }
    return 0;
}
static int cook_temp_control_target_temp_cb(const unsigned short target_temp, enum INPUT_DIR input_dir)
{
    if (input_dir == INPUT_LEFT)
    {
    }
    else
    {
        // sprintf(g_ecb_dev_status_buff.right_tar_temp, "%d", target_temp);
        // g_ecb_dev_status_buff.valid_bits |= VALID_BIT(CMN_DSVB_RIGHT_TAR_TEMP);

        // if (IOT_DevStateReport(&g_ecb_dev_status_buff) != CMN_RET_OK)
        // {
        //     ESP_LOGE(TASK_TAG, "report_temp IOT_DevStateReport error\n");
        // }
    }
    return 0;
}

static int cook_assist_remind_cb(int value)
{
    if (1 == value)
    {
        //移锅小火超时关火
        uint8_t buf_setmsg[10] = {0};
        uint16_t buf_len = 0;
        
        buf_setmsg[buf_len++] = prop_HoodFireTurnOff;  //灶具关火
        buf_setmsg[buf_len++] = 0x01;       //右灶关火

        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(), buf_setmsg, buf_len, 3);
        LOGI("mars", "烹饪助手: 发送右灶关火");

        LOGI("mars", "推送事件: 移锅关火");
        user_post_event_json(EVENT_RIGHT_STOVE_MOVE_POT);
    }
    else if (2 == value)
    {
        uint8_t buf_setmsg[10] = {0};
        uint16_t buf_len = 0;
        
        buf_setmsg[buf_len++] = prop_HoodFireTurnOff;  //灶具关火
        buf_setmsg[buf_len++] = 0x01;       //右灶关火

        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(), buf_setmsg, buf_len, 3);
        LOGI("mars", "烹饪助手: 发送右灶关火(安全冗余措施)");
    }
    return 0;
}

static int one_key_recipe_info_cb(cook_model_t *cook_model)
{
    static unsigned short remaining_time_last = 0;

    LOGI("mars", "一键菜谱: 回调... remaining_time=%d  cook_event=%d", cook_model->remaining_time, cook_model->cook_event);

    if (g_user_cook_assist.ROneKeyRecipeSwitch == 1 && g_user_cook_assist.ROneKeyRecipeType > 0 &&g_user_cook_assist.ROneKeyRecipeTimeSet > 0)
    {
        unsigned short remaining_time = (cook_model->remaining_time%60? (cook_model->remaining_time/60+1) : (cook_model->remaining_time/60));
        if (remaining_time != remaining_time_last)
        {
            LOGI("mars", "一键菜谱: 一键菜谱剩余时间更新 剩余%d分钟", remaining_time);
            g_user_cook_assist.ROneKeyRecipeTimeLeft = remaining_time;
            remaining_time_last = remaining_time;
            report_wifi_property(NULL, NULL);
            sync_wifi_property();
        }

        if (cook_model->cook_event == WATER_FIRST_BOIL)
        {
            if (g_user_cook_assist.ROneKeyRecipeType == 1)
                mars_beer_control(27);
            else
                mars_beer_control(26);
            LOGI("mars", "一键菜谱: 回调首次水开");
        }
        else if (cook_model->cook_event == TEMP_NOT_DOWN_AFTER_30_SECONDS_LOOP)
        {
            LOGI("mars", "一键菜谱: 回调水开后30秒未温降");
            mars_beer_control(26);
        }
        else if (cook_model->cook_event == TEMP_HAVE_DOWN_THEN_WATER_BOIL)
        {
            LOGI("mars", "一键菜谱: 回调温降后再次水开");
            mars_beer_control(27);
        }
        else if (cook_model->cook_event == COOK_END) //(cook_model->remaining_time == 0)
        {
            LOGI("mars", "一键菜谱: 一键菜谱烹饪完成");
            user_post_event_json(EVENT_RIGHT_STOVE_ONEKEY_RECIPE);
            mars_beer_control(29);

            g_user_cook_assist.ROneKeyRecipeSwitch   = 0;
            g_user_cook_assist.ROneKeyRecipeType     = 0;
            g_user_cook_assist.ROneKeyRecipeTimeSet  = 0;
            g_user_cook_assist.ROneKeyRecipeTimeLeft = 0;
            LOGI("mars", "一键菜谱: 一键菜谱参数清零");

            aos_kv_del(ONE_KEY_RECIPE_KV);
            LOGI("mars", "清除一键菜谱标志(烹饪完成)");

            report_wifi_property(NULL, NULL);
            sync_wifi_property();
        }
    }
}
uint8_t dry_fire_state = 0x00;
static int dry_fire_info_cb(int type, int dryburn_reason)
{
    /*
    0：未干烧
    1：已干烧
    2：干烧退出
    3：状态不明
    */
    g_user_cook_assist.RDryFireState = type;
    if (dry_fire_state != type)
    {
        //上报属性 上报事件
        LOGI("mars", "烹饪助手上报干烧状态: %d (0-未干烧 1-已干烧 2-干烧退出 3-状态不明),干烧原因:%d", type, dryburn_reason);
        char property_payload[64] = {0};
        sprintf(property_payload, "{\"RDryFireState\": %d, \"DryburnReason\": %d}", type, dryburn_reason);
        user_post_property_json(property_payload);
        if(type == 1)
        {
            user_post_event_json(EVENT_RIGHT_STOVE_DRY_BURN);
        }
        
        mars_store_dry_fire(type);

        // if (type == 1)
        // {
        //     LOGI("mars", "推送事件: 干烧了!!!!!!");
        //     user_post_event_json(EVENT_RIGHT_STOVE_MOVE_POT);
        // }

        if (type == 1)
        {
            unsigned char flag = 1;
            int ret = aos_kv_set(DRY_BURN_KV, &flag, 1, 1);
            LOGI("mars", "置位防干烧标记(干烧触发)");
            
            mars_beer_control(28);
            LOGI("mars", "进入干烧");
        }
        else
        {
            aos_kv_del(DRY_BURN_KV);
            LOGI("mars", "清除防干烧标记(移锅复位)");
            mars_beer_control(0x00);
            LOGI("mars", "退出干烧");
        }

        dry_fire_state = type;
    }


}

/**
 * @description: 关闭蜂鸣器回调，(暂时用于防干烧退出时，不关闭图标但是关闭蜂鸣器)
 * @param {int} type 1：关闭蜂鸣器 否则不响应
 * @return {*}
 */
static int close_beeper_cb(int type)
{
    if(type != 1)
    {    
        return -1;
    }
    mars_beer_control(0x00);        //电控停止蜂鸣

    return 1;

}

static int one_key_recipe_fire_cb(int fire_state, bool is_first_small)
{

}

void mars_ca_para_save(void)
{
    int ret = aos_kv_set("COOK_ASSIST_PARA", &g_user_cook_assist, sizeof(mars_cook_assist_t), 1);
    if (ret == 0)
    {
        LOGI("mars", "烹饪助手参数保存成功");
    } 
    else
    {
        LOGE("mars", "烹饪助手参数保存失败!!!(ret=%d)", ret);
    }
}

//临时设置的烟机档位回调函数
static int hood_light_change(int gear)
{
    uint8_t buf_setmsg[10] = {0};
    uint16_t buf_len = 0;

    buf_setmsg[buf_len++] = prop_HoodLight;     //烟机照明
    buf_setmsg[buf_len++] = gear;

    Mars_uartmsg_send(cmd_set,uart_get_seq_mid(),buf_setmsg,buf_len,3);
    return 0;
}

void move_pan_fire(int type)
{   
    aux_handle_t *aux_handle = get_aux_handle(INPUT_RIGHT); 
    if (type == 1) //移锅小火
    {
        aux_handle->pan_fire_status = 1;
        
        aos_kv_del(PAN_FIRE_KV);
        aos_msleep(100);
        unsigned char flag = 1;
        int ret = aos_kv_set(PAN_FIRE_KV, &flag, 1, 1);
        LOGI("mars", "进入移锅小火状态，置位移锅小火标记(移锅触发)");
    }
    else if (type == 2) //移锅复位
    {
        aux_handle->pan_fire_status = 0;
        aos_kv_del(PAN_FIRE_KV);
        LOGI("mars", "退出移锅小火状态，清除移锅小火标记(移锅复位)");
    }
    else if (type == 3) //移锅关火
    {
        aux_handle->pan_fire_status = 0;
        aos_kv_del(PAN_FIRE_KV);
        LOGI("mars", "移锅后关火时间到，清除移锅小火标记(移锅关火)");
    }
    else if(type == 4)
    {
        aux_handle->pan_fire_status = 0;
        aos_kv_del(PAN_FIRE_KV);
        LOGI("mars","移锅小火中用户手动关火，清除移锅小火标记(移锅中关火)");
    }
}

void mars_AuxiliaryTemp(int type)
{
    if (type == 0)
    {
        aos_kv_del(AUXILIARY_KV);
        LOGI("mars", "清除辅助控温标记");
    }
    else
    {
        aos_kv_del(AUXILIARY_KV);
        aos_msleep(100);
        unsigned char flag = 1;
        int ret = aos_kv_set(AUXILIARY_KV, &flag, 1, 1);
        LOGI("mars", "置位辅助控温标记");
    }
}

void mars_ca_reset(void)
{
    LOGE("mars", "烹饪助手所有参数恢复出厂!!!!!!");
    g_user_cook_assist.ZnpySwitch               = 1;
    g_user_cook_assist.ZnpyOneExit              = 0;
    g_user_cook_assist.RAuxiliarySwitch         = 0;
    g_user_cook_assist.RAuxiliaryTemp           = 150;
    g_user_cook_assist.RMovePotLowHeatSwitch    = 1;
    g_user_cook_assist.RMovePotLowHeatOffTime   = 90;    
    g_user_cook_assist.ROilTempSwitch           = 1;//油温显示
    g_user_cook_assist.CookingCurveSwitch       = 1;//温度曲线
    g_user_cook_assist.ROneKeyRecipeSwitch      = 0;
    g_user_cook_assist.ROneKeyRecipeType        = 0;
    g_user_cook_assist.ROneKeyRecipeTimeSet     = 0;
    g_user_cook_assist.ROneKeyRecipeTimeLeft    = 0;
    g_user_cook_assist.RDryFireSwitch           = 1;
    g_user_cook_assist.RDryFireState            = 0;
    g_user_cook_assist.RDryFireTempCurveSwitch  = 0;
    g_user_cook_assist.RDryFireUserType         = 0;//默认量产用户


    mars_ca_para_save();
}

void mars_ca_init(void)
{
    memset(&g_user_cook_assist, 0xFF, sizeof(mars_cook_assist_t));
    
    int len = sizeof(mars_cook_assist_t);
    int ret = aos_kv_get("COOK_ASSIST_PARA", &g_user_cook_assist, &len);
    if (ret == 0)
    {
        g_user_cook_assist.ZnpyOneExit              = 0;
        g_user_cook_assist.RAuxiliarySwitch         = 0;
        g_user_cook_assist.RAuxiliaryTemp           = 150;
        g_user_cook_assist.ROneKeyRecipeSwitch      = 0;
        g_user_cook_assist.ROneKeyRecipeType        = 0;
        g_user_cook_assist.ROneKeyRecipeTimeSet     = 0;
        g_user_cook_assist.ROneKeyRecipeTimeLeft    = 0;
        g_user_cook_assist.RDryFireState            = 0;

        if(g_user_cook_assist.RDryFireSwitch == 0xFF)
        {
            g_user_cook_assist.RMovePotLowHeatSwitch    = 1;    //默认打开移锅小火开关
            g_user_cook_assist.RDryFireSwitch           = 1;    //默认打开防干烧开关
            g_user_cook_assist.RDryFireState            = 0;    //默认设置干烧状态为未干烧
            g_user_cook_assist.RDryFireUserType         = 0;    //默认设置用户类型为量产用户
            g_user_cook_assist.RDryFireTempCurveSwitch  = 0;    //默认关闭温度上报开关
            LOGI("mars","failed read kv about dryburn,默认设置防干烧相关变量\r\n");
        }

        LOGI("mars", "KV初始化烹饪助手参数:  \
        [智能排烟开关=%d 单次退出=%d]      \
        [精准控温开关=%d 精准控温温度=%d]   \
        [移锅小火开关=%d 移锅小火时间=%d]   \
        [油温显示开关=%d 温度曲线开关=%d]   \
        [一键菜谱开关=%d 一键菜谱类型=%d 一键菜谱设置时间=%d 一键菜谱剩余时间=%d] \
        [防干烧开关=%d 防干烧状态=%d 防干烧曲线开关=%d 防干烧用户=%d]",
        g_user_cook_assist.ZnpySwitch,
        g_user_cook_assist.ZnpyOneExit,
        g_user_cook_assist.RAuxiliarySwitch,
        g_user_cook_assist.RAuxiliaryTemp,
        g_user_cook_assist.RMovePotLowHeatSwitch,
        g_user_cook_assist.RMovePotLowHeatOffTime,
        g_user_cook_assist.ROilTempSwitch,
        g_user_cook_assist.CookingCurveSwitch,
        g_user_cook_assist.ROneKeyRecipeSwitch,
        g_user_cook_assist.ROneKeyRecipeType,
        g_user_cook_assist.ROneKeyRecipeTimeSet,
        g_user_cook_assist.ROneKeyRecipeTimeLeft,
        g_user_cook_assist.RDryFireSwitch,
        g_user_cook_assist.RDryFireState,
        g_user_cook_assist.RDryFireTempCurveSwitch,
        g_user_cook_assist.RDryFireUserType);
    }
    else
    {
        LOGE("mars", "初始化烹饪助手参数失败!!!!!!手动初始化");
        g_user_cook_assist.ZnpySwitch               = 1;
        g_user_cook_assist.ZnpyOneExit              = 0;
        g_user_cook_assist.RAuxiliarySwitch         = 0;
        g_user_cook_assist.RAuxiliaryTemp           = 150;
        g_user_cook_assist.RMovePotLowHeatSwitch    = 1;
        g_user_cook_assist.RMovePotLowHeatOffTime   = 90;    
        g_user_cook_assist.ROilTempSwitch           = 1;//油温显示    
        g_user_cook_assist.CookingCurveSwitch       = 1;//温度曲线
        g_user_cook_assist.ROneKeyRecipeSwitch      = 0;
        g_user_cook_assist.ROneKeyRecipeType        = 0;
        g_user_cook_assist.ROneKeyRecipeTimeSet     = 0;
        g_user_cook_assist.ROneKeyRecipeTimeLeft    = 0;
        g_user_cook_assist.RDryFireSwitch           = 1;
        g_user_cook_assist.RDryFireState            = 0;
        g_user_cook_assist.RDryFireTempCurveSwitch  = 0;
        g_user_cook_assist.RDryFireUserType         = 0;//默认量产用户
    }

    unsigned char value = 0;
    len = sizeof(value);
    ret = aos_kv_get(ONE_KEY_RECIPE_KV, &value, &len);
    if (ret == 0 && len > 0) 
    {
        if (value == 1)
        {
            LOGW("mars", "警告: 检测到上次一键菜谱异常退出!!!!!!");
            cook_assist_remind_cb(2);

            aos_kv_del(ONE_KEY_RECIPE_KV);
            LOGI("mars", "清除一键菜谱标志(异常退出取消)");
        }
    }

    len = sizeof(value);
    ret = aos_kv_get(PAN_FIRE_KV, &value, &len);
    if (ret == 0 && len > 0) 
    {
        if (value == 1)
        {
            LOGW("mars", "警告: 检测到上次移锅小火异常退出!!!!!!");
            cook_assist_remind_cb(2);

            aos_kv_del(PAN_FIRE_KV);
            LOGI("mars", "清除移锅小火标志(异常退出取消)");
        }
    }

    len = sizeof(value);
    ret = aos_kv_get(DRY_BURN_KV,&value,&len);
    if(ret == 0 && len > 0)
    {
        if(value == 1)
        {
            LOGW("mars","警告：检测到上次防干烧异常退出!!!!!!");
            dry_fire_state = 0x01;
            g_user_cook_assist.RDryFireState = 0x01;        //需要图标继续闪烁

            cook_assist_remind_cb(2);
            aos_kv_del(DRY_BURN_KV);
            LOGI("mars","清除干烧状态标志(异常退出取消)");
        }
    }


    len = sizeof(value);
    ret = aos_kv_get(AUXILIARY_KV, &value, &len);
    if (ret == 0 && len > 0) 
    {
        if (value == 1)
        {
            LOGW("mars", "警告: 检测到上次辅助控温异常退出!!!!!!");
            cook_assistant_fire_speed(1, 1);

            aos_kv_del(AUXILIARY_KV);
            LOGI("mars", "清除辅助控温标志(异常退出取消)");
        }
    }

    //烹饪助手回调
    set_work_mode(1);
    register_close_fire_cb(cook_assistant_close_fire); //关火回调
    register_hood_gear_cb(cook_assistant_hood_speed);  //风机回调
    register_fire_gear_cb(cook_assistant_fire_speed);
 // register_thread_lock_cb(semaphore_take);
 // register_thread_unlock_cb(semaphore_give);
 // register_work_mode_cb(cook_work_mode_cb);
 // register_pan_fire_switch_cb(cook_pan_fire_switch_cb);
 // register_dry_switch_cb(cook_dry_switch_cb);
 // register_temp_control_switch_cb(cook_temp_control_switch_cb);
 // register_temp_control_target_temp_cb(cook_temp_control_target_temp_cb);
    register_cook_assist_remind_cb(cook_assist_remind_cb);
    cook_assistant_init(INPUT_RIGHT);

    cook_aux_init(INPUT_RIGHT);

    set_smart_smoke_switch(g_user_cook_assist.ZnpySwitch);
    set_hood_min_gear(1);
    // set_temp_control_switch(g_user_cook_assist.RAuxiliarySwitch, INPUT_RIGHT);
    // set_temp_control_target_temp(g_user_cook_assist.RAuxiliaryTemp, INPUT_RIGHT);
    set_pan_fire_switch(g_user_cook_assist.RMovePotLowHeatSwitch, INPUT_RIGHT);
    set_pan_fire_delayofftime(g_user_cook_assist.RMovePotLowHeatOffTime);

    register_beep_cb(set_beep_type);                            //控制蜂鸣器的回调函数
    register_multivalve_cb(Set_MutiValve_gear);                 //控制八段阀的回调函数
    register_auxclose_fire_cb(cook_assistant_close_fire);
    register_auxcount_down(aux_set_remaintime);                 //设置辅助烹饪的剩余时间
    register_aux_exit_cb(aux_set_exit_cb);


    //一键烹饪回调
    ca_api_register_close_fire_cb(cook_assistant_close_fire); //关火回调
    ca_api_register_hood_gear_cb(cook_assistant_hood_speed);  //风机回调
    ca_api_register_fire_gear_cb(cook_assistant_fire_speed);  //调节火力挡位
    ca_api_register_fire_state_cb(one_key_recipe_fire_cb);
    ca_api_register_quick_cooking_info_cb(one_key_recipe_info_cb);
    ca_api_register_hood_light_cb(hood_light_change);
    //quick_cooking_switch(g_user_cook_assist.ROneKeyRecipeSwitch, g_user_cook_assist.ROneKeyRecipeType, g_user_cook_assist.ROneKeyRecipeTimeSet*60, g_user_cook_assist.ROneKeyRecipeTimeSet*60);

    register_dryburn_remind_cb(dry_fire_info_cb);
    register_close_beeper_cb(close_beeper_cb);
    set_dry_fire_switch(g_user_cook_assist.RDryFireSwitch, 1, g_user_cook_assist.RDryFireUserType);

    char buf_setmsg[8] = {0};
    uint8_t buf_len = 0;
    buf_setmsg[buf_len++] = prop_RadarGear;                    //开机设置一次雷达档位为中档
    buf_setmsg[buf_len++] = 0x02;

    Mars_uartmsg_send(cmd_set, uart_get_seq_mid(), &buf_setmsg, sizeof(buf_setmsg), 3);
}


/**
 * @brief: 八段阀火力设置档位
 * @param {enum INPUT_DIR} input_dir
 * @return {*}
 */
static int Set_MutiValve_gear(enum INPUT_DIR input_dir,int gear)
{   
    if(gear > 7 || gear < 0)
    {
        LOGI("mars","八段阀不支持此档位设置");
        return -1;
    }

    LOGI("mars","回调函数中设置八段阀:%d",gear);
    char buf_setmsg[8] = {0};
    int buf_len = 0;
    buf_setmsg[buf_len++] = prop_MultiValveGear;
    buf_setmsg[buf_len++] = gear;
    Mars_uartmsg_send(cmd_set,uart_get_seq_mid(),buf_setmsg,buf_len,3);
    return 0;
}


/**
 * @brief: 设置蜂鸣器警报模式
 * @return {*}
 */
static int set_beep_type(int beep_type)
{
    if(beep_type < 0 || beep_type > 4)
    {
        LOGI("mars","不支持的蜂鸣器命令类型");
        return -1;
    }
    LOGI("mars","回调函数中设置蜂鸣器:%d",beep_type);
    char buf_setmsg[10] = {0};
    int buf_len = 0;
    buf_setmsg[buf_len++] = prop_Beer;
    buf_setmsg[buf_len++] = beep_type;
    Mars_uartmsg_send(cmd_set,uart_get_seq_mid(),buf_setmsg,buf_len,3);
    return 0;
}


/**
 * @brief: 辅助烹饪煮模式
 * @param {unsigned char} remaintime
 * @return {*}
 */
static int aux_set_remaintime(unsigned char remaintime)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();

    LOGI("mars","回调函数中设置剩余运行时间:%d",remaintime);
    char buf_setmsg[10] = {0};
    int buf_len = 0;
    buf_setmsg[buf_len++] = prop_AuxCookLeftTime;
    buf_setmsg[buf_len++] = remaintime;
    Mars_uartmsg_send(cmd_store,uart_get_seq_mid(),buf_setmsg,buf_len,3);
    return 0;
}


/**
 * @brief: 通知电控辅助烹饪的退出状态，头部显示板用于取消显示
 * @return {*}
 */
static int aux_set_exit_cb(int type)
{
    if(type > 2 || type < 0)
    {
        LOGI("mars","ERROR!error setting about exit type");
    }

    char buf_setmsg[10] = {0};
    int buf_len = 0;
    buf_setmsg[buf_len++] = prop_AuxCookStatus;
    buf_setmsg[buf_len++] = type;
    Mars_uartmsg_send(cmd_set,uart_get_seq_mid(),buf_setmsg,buf_len,3);
    return 0;    
}   


