/*
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-06-06 17:18:46
 * @LastEditors: Zhouxc
 * @LastEditTime: 2024-06-18 16:15:24
 * @FilePath     : /et70-ca3/Products/example/mars_template/mars_devfunc/mars_devmgr.c
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <cJSON.h>

#include "../dev_config.h"
#include "app_entry.h"

#include "../mars_driver/mars_uartmsg.h"
#include "../mars_driver/mars_cloud.h"

#include "mars_devmgr.h"

#if MARS_STOVE
#include "mars_stove.h"
#endif

#if MARS_STEAMOVEN
#include "mars_steamoven.h"
#endif
#if MARS_DISHWASHER
#include "mars_dishwasher.h"
#endif
#if MARS_STERILIZER

#include "mars_display.h"

#include "mars_factory.h"
#endif

#define         VALID_BIT(n)            ((uint16_t)1 << (n - prop_ElcSWVersion))
static mars_template_ctx_t g_user_example_ctx = {0};
mars_cook_assist_t  g_user_cook_assist = {0};
static aos_timer_t period_query_timer;
int mars_ble_awss_state = 0;

bool is_asssist_change = true;
bool is_report_finish  = false;

mars_template_ctx_t *mars_dm_get_ctx(void)
{
    return &g_user_example_ctx;
}

void mars_beer_control(uint8_t val)
{
    uint8_t buf[] = {prop_Beer, val};
    Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf, sizeof(buf), 3);
    LOGI("mars", "下发蜂鸣器: %d", val);
}

void mars_sync_tempture(uint16_t tar_temp, uint16_t env_temp)
{
    uint8_t buf[] = {prop_ROilTemp, tar_temp/256, tar_temp%256, prop_REnvTemp, env_temp/256, env_temp%256};
    Mars_uartmsg_send(cmd_store, uart_get_seq_mid(),  buf, sizeof(buf), 0);
    //LOGI("mars", "下发右灶温度: 目标温度=%d℃ 环境温度=%d℃", tar_temp, env_temp);
}

void mars_store_netstatus()
{
    char* net_des[] = {"未联网", "已联网", "配网中"};
    uint8_t buff[] = {prop_NetState, mars_dm_get_ctx()->status.NetState};
    Mars_uartmsg_send(cmd_store, uart_get_seq_mid(),  buff, sizeof(buff), 3);
    LOGI("mars", "下发网络状态: %s", net_des[mars_dm_get_ctx()->status.NetState]);
}

void mars_store_swversion(void)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    uint8_t buf[] = {prop_NetFwVer, mars_template_ctx->status.WifiSWVersion[0]};
    Mars_uartmsg_send(cmd_store, uart_get_seq_mid(), &buf, sizeof(buf), 3);
    LOGI("mars", "下发通讯板软件版本: 0x%02X", mars_template_ctx->status.WifiSWVersion[0]);
}

void mars_devmngr_getstatus(void *arg1, void *arg2)
{    
    uint8_t buf[] = {0};
    Mars_uartmsg_send(cmd_get, uart_get_seq_mid(),  &buf, 1, 3);
    LOGI("mars", "请求电控板全属性");
}

void mars_store_dry_fire(uint8_t val)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    //如果发生了干烧且电源是关闭的，则需要先打开电源，非干烧通知不打开电源
    if(val == 1 && 0 == mars_template_ctx->status.SysPower){
        uint8_t buf1[] = {prop_SysPower,0x01};
        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(), &buf1, sizeof(buf1), 3);
        LOGI("mars","now systemPower is off,inform on");
    }
    
    uint8_t buf2[] = {0x2E, val};
    Mars_uartmsg_send(cmd_store, uart_get_seq_mid(), &buf2, sizeof(buf2), 3);
    LOGI("mars", "下发干烧状态: %d", val);
}

static void process_property_diff(dev_status_t* p1, dev_status_t* p2)
{
    // if (memcmp(p1, p2, sizeof(mars_template_ctx_t)) == 0)
    // {
    //     LOGE("mars", "解析后, 全属性结构体没有发生变化!!!");
    //     return;
    // }    
}

bool mars_uart_prop_process(uartmsg_que_t *msg)
{
    if (cmd_keypress == msg->cmd)
    {
        Mars_uartmsg_send(cmd_ack, msg->seq, NULL, 0, 0);
    }
    else if (cmd_event == msg->cmd)
    {
        Mars_uartmsg_send(cmd_ack, msg->seq, NULL, 0, 0);
    }
    else if (cmd_getack == msg->cmd)
    {
        mars_uartmsg_ackseq_by_seq(msg->seq);
    }
    else
    {
        LOGE("mars", "未知命令码");
    }

    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();//烹饪助手温度上报消息直接处理 
    dev_status_t dev_status_last = g_user_example_ctx.status;
    uint8_t nak    = 0;
    bool report_en = false;
    is_asssist_change = false;
    for (uint16_t i=0; i<msg->len; ++i)
    {
        LOGD("mars", "开始解析属性 0x%02X", msg->msg_buf[i]);
        if (msg->msg_buf[i] >= PROP_SYS_BEGIN && msg->msg_buf[i] <= PROP_SYS_END)
        {
            switch (msg->msg_buf[i])
            {
                case prop_ElcSWVersion:
                {
                    if (mars_template_ctx->status.ElcSWVersion != msg->msg_buf[i+1])
                    {
                        LOGW("mars", "解析属性0x%02X: 显示板软件变化(0x%02X - 0x%02X)", msg->msg_buf[i], mars_template_ctx->status.ElcSWVersion, msg->msg_buf[i+1]);
                        report_en = true;                        
                        mars_template_ctx->status.ElcSWVersion = msg->msg_buf[i+1];

                        extern bool mars_ota_inited(void);
                        if (mars_ota_inited())
                        {
                           mars_ota_module_1_init(); 
                        }
                    }

                    mars_template_ctx->common_reportflg |= VALID_BIT(msg->msg_buf[(i)]);  //通用属性上报标记置1
                    i+=1;
                    break;
                }
                case prop_ElcHWVersion:
                {
                    if (mars_template_ctx->status.ElcHWVersion != msg->msg_buf[i+1])
                    {
                        LOGW("mars", "解析属性0x%02X: 显示板硬件变化(0x%02X - 0x%02X)", msg->msg_buf[i], mars_template_ctx->status.ElcHWVersion, msg->msg_buf[i+1]);
                        report_en = true;                        
                        mars_template_ctx->status.ElcHWVersion = msg->msg_buf[i+1];
                    }

                    mars_template_ctx->common_reportflg |= VALID_BIT(msg->msg_buf[(i)]);
                    i+=1;
                    break;
                }
                case prop_PwrSWVersion:
                {   
                    if (mars_template_ctx->status.PwrSWVersion != msg->msg_buf[i+1])
                    {
                        LOGW("mars", "解析属性0x%02X: 电源板软件变化(0x%02X - 0x%02X)", msg->msg_buf[i], mars_template_ctx->status.PwrSWVersion, msg->msg_buf[i+1]);
                        report_en = true;                        
                        mars_template_ctx->status.PwrSWVersion = msg->msg_buf[i+1];
                    }  

                    mars_template_ctx->common_reportflg |= VALID_BIT(msg->msg_buf[(i)]);
                    i+=1;
                    break;
                }
                case prop_PwrHWVersion:
                {
                    if (mars_template_ctx->status.PwrHWVersion != msg->msg_buf[i+1])
                    {
                        LOGW("mars", "解析属性0x%02X: 电源板硬件变化(0x%02X - 0x%02X)", msg->msg_buf[i], mars_template_ctx->status.PwrHWVersion, msg->msg_buf[i+1]);
                        report_en = true;                        
                        mars_template_ctx->status.PwrHWVersion = msg->msg_buf[i+1];
                    } 

                    mars_template_ctx->common_reportflg |= VALID_BIT(msg->msg_buf[(i)]);
                    i+=1;
                    break;
                }
                case prop_SysPower:
                {
                    //LOGW("mars", "系统开关(%d - %d)", mars_template_ctx->status.SysPower, msg->msg_buf[i+1]);
                    if(mars_template_ctx->status.SysPower != msg->msg_buf[i+1])
                    {
                        LOGW("mars", "解析属性0x%02X: 系统开关变化(%d - %d)", msg->msg_buf[i], mars_template_ctx->status.SysPower, msg->msg_buf[i+1]);
                        report_en = true;
                        mars_template_ctx->status.SysPower = msg->msg_buf[i+1];

                        if (mars_template_ctx->status.SysPower == 0)
                        {
                            g_user_cook_assist.ZnpyOneExit = 0;

                            //关闭电源的时候单次退出清零，重新设置烹饪助手端的智能排烟开关
                            set_smart_smoke_switch(g_user_cook_assist.ZnpySwitch);
                            is_asssist_change = true;
                        }
                        
                        if (mars_template_ctx->status.SysPower == 0)
                        {
                            if (g_user_cook_assist.RAuxiliarySwitch == 1)
                            {
                                g_user_cook_assist.RAuxiliarySwitch = 0;
                                g_user_cook_assist.RAuxiliaryTemp   = 150;
                                set_temp_control_switch(g_user_cook_assist.RAuxiliarySwitch, 1);
                                set_temp_control_target_temp(g_user_cook_assist.RAuxiliaryTemp, 1);
                                is_asssist_change = true;
                                LOGI("mars", "辅助控火: 系统关闭 ==> 辅助控火开关=0");
                            }

                            if (g_user_cook_assist.ROneKeyRecipeSwitch == 1)
                            {
                                g_user_cook_assist.ROneKeyRecipeSwitch   = 0;
                                g_user_cook_assist.ROneKeyRecipeType     = 0;
                                g_user_cook_assist.ROneKeyRecipeTimeSet  = 0;
                                g_user_cook_assist.ROneKeyRecipeTimeLeft = 0;
                                quick_cooking_switch(g_user_cook_assist.ROneKeyRecipeSwitch, g_user_cook_assist.ROneKeyRecipeType, g_user_cook_assist.ROneKeyRecipeTimeSet*60, 15*60); //g_user_cook_assist.ROneKeyRecipeTimeSet*60
                                is_asssist_change = true;
                                LOGI("mars", "辅助控火: 系统关闭 ==> 一键烹饪开关=0");
                            }

                            //电源关闭，退出干烧状态，按照规范做法此处应该在电控上报的C1字段的低字节bit0处判断，但是开机状态下关机电控没有置位对应bit0，放到这里判断退出干烧状态
                            // extern uint8_t dry_fire_state;
                            // if (dry_fire_state == 0x01)
                            // {
                            //     mars_beer_control(0x00);
                            //     LOGE("mars","inform CA user press button(close SystemPower)");
                            //     exit_dryburn_status(1, 0);
                            // }
                        }

                        // if (g_user_cook_assist.ZnpyOneExit == 1)
                        // {
                        //     g_user_cook_assist.ZnpyOneExit = 0; //无论开机还是关机,都退出单次退出状态
                        //     is_asssist_change = true;
                        // }

                        if (mars_template_ctx->status.SysPower == 0x00)//判断是否要ota重启
                        {
                            extern void mars_ota_reboot(void);
                            mars_ota_reboot();
                        }
                    }

                    mars_template_ctx->common_reportflg |= VALID_BIT(msg->msg_buf[(i)]);                    
                    i+=1;
                    break;
                }
                case prop_Netawss:
                {
                    if(msg->msg_buf[i+1] == 0x01)
                    {
                        LOGW("mars", "串口命令: 启动蓝牙配网");
                        if (!is_awss_state())
                        {
                            mars_dm_get_ctx()->status.NetState = NET_STATE_CONNECTING;
                            mars_store_netstatus();
                            del_bind_flag();
                            do_awss_reset();
                            do_awss_reboot();
                        }
                        else
                        {
                            LOGE("mars", "已处于配网,本命令忽略");
                        }
                    }
                    else if(msg->msg_buf[i+1] == 0x00)
                    {
                        extern void timer_func_awss_finish(void *arg1, void *arg2);
                        LOGW("mars", "串口命令: 退出蓝牙配网");
                        if (is_awss_state())
                            timer_func_awss_finish(NULL, NULL);                        
                        else                        
                            LOGE("mars", "未处于配网,本命令忽略");                    
                    }
                    i+=1;
                    break;
                }
                case prop_ErrCode:
                {
                    uint32_t ErrCodeNow = (uint32_t)(msg->msg_buf[i+1] << 24) | (uint32_t)(msg->msg_buf[i+2] << 16) |  (uint32_t)(msg->msg_buf[i+3] << 8)  | (uint32_t)(msg->msg_buf[i+4]);
                    if (mars_template_ctx->status.ErrCode != ErrCodeNow)
                    {
                        LOGW("mars", "解析属性0x%02X: 故障代码变化(0x%08X - 0x%08X)", msg->msg_buf[i], mars_template_ctx->status.ErrCode, ErrCodeNow);
                        report_en = true;
                        mars_template_ctx->status.ErrCode = ErrCodeNow;
                    }

                    mars_template_ctx->common_reportflg |= VALID_BIT(msg->msg_buf[(i)]);                    
                    i+=4;
                    break;
                }
                case prop_ErrCodeShow:
                {
                    if (mars_template_ctx->status.ErrCodeShow != msg->msg_buf[i+1])
                    {
                        LOGW("mars", "解析属性0x%02X: 当前故障码变化(%d - %d)", msg->msg_buf[i], mars_template_ctx->status.ErrCodeShow, msg->msg_buf[i+1]);
                        report_en = true; 
                        mars_template_ctx->status.ErrCodeShow = msg->msg_buf[i+1];

                        if (msg->msg_buf[i+1] != 0)
                        {
                            user_post_event_json(msg->msg_buf[i+1]);
                            LOGE("mars", "推送事件: 故障发生推送 (历史值=%d 最新值=%d)", mars_template_ctx->status.ErrCodeShow, msg->msg_buf[i+1]);
                        }
                        else
                        {
                            user_post_event_json(msg->msg_buf[i+1]);
                            LOGI("mars", "推送事件: 故障恢复推送 (历史值=%d 最新值=%d)", mars_template_ctx->status.ErrCodeShow, msg->msg_buf[i+1]);
                        }
                    }

                    mars_template_ctx->common_reportflg |= VALID_BIT(msg->msg_buf[(i)]);
                    i+=1;
                    break;
                }

                //增加产品型号的处理
                case prop_Prodcutname:
                {
                    LOGW("mars", "解析属性calc prop:0x%02X", msg->msg_buf[i]);
                    int length = msg->msg_buf[i+1];
                    LOGI("mars","Product name lenght is:%d",length);
                    i += (length+1);
                    break;
                }

                default:
                    LOGE("mars", "error 收到未知属性,停止解析!!! (属性 = 0x%02X)", msg->msg_buf[i]);
                    return false;
                    break;
            }
        } 
        else if ((msg->msg_buf[i] >= PROP_INTEGRATED_STOVE_BEIGN && msg->msg_buf[i] <= PROP_INTEGRATED_STOVE_END) || (msg->msg_buf[i] >= prop_AuxCook && msg->msg_buf[i] <= prop_AuxCookLeftTime))
        {
            mars_stove_uartMsgFromSlave(msg, mars_template_ctx, &i, &report_en, &nak);
        }
        else if (msg->msg_buf[i] >= PROP_PARA_BEGIN && msg->msg_buf[i] <= PROP_PARA_END)
        {
            mars_steamoven_uartMsgFromSlave(msg, mars_template_ctx, &i, &report_en, &nak);
        }
        else if(msg->msg_buf[i] >= PROP_SENSOR_BEGIN && msg->msg_buf[i] <= PROP_SENSOR_END)
        {
            mars_sensor_uartMsgFromSlave(msg, mars_template_ctx, &i, &report_en, &nak);
        }
        else if (msg->msg_buf[i] == prop_LSteamGear || msg->msg_buf[i] == prop_LMicroWaveGear) //0x80
        {
            #define  STOV_SPEC_VALID_BIT(n)  ((uint16_t)1 << (n - prop_LSteamGear))
            if (msg->msg_buf[i] == prop_LSteamGear)
            {
                if (mars_template_ctx->status.LSteamGear != msg->msg_buf[i+1])
                {
                    LOGI("mars", "解析属性0x%02X: 左腔蒸汽档位变化(%d - %d)", msg->msg_buf[i], mars_template_ctx->status.LSteamGear, msg->msg_buf[i+1]);
                    report_en = true;
                    mars_template_ctx->status.LSteamGear = msg->msg_buf[i+1];
                }

                mars_template_ctx->steamoven_spec_reportflg |= STOV_SPEC_VALID_BIT(msg->msg_buf[(i)]);
                i+=1;
                LOGI("mars", "收到蒸汽档位 (档位 = %d)", mars_template_ctx->status.LSteamGear);
            }
            else if (msg->msg_buf[i] == prop_LMicroWaveGear)
            {
                mars_template_ctx->status.LMicroGear = msg->msg_buf[i+1];
                report_en = true;
                mars_template_ctx->steamoven_spec_reportflg |= STOV_SPEC_VALID_BIT(msg->msg_buf[(i)]);
                i+=1;
                LOGI("mars", "收到微波档位 (档位 = %d)", mars_template_ctx->status.LMicroGear);
            }
        }
        else if (msg->msg_buf[i] >= prop_ChickenSoupDispSwitch && msg->msg_buf[i] <= prop_DisplayHWVersion)
        {
            mars_display_uartMsgFromSlave(msg, mars_template_ctx, &i, &report_en, &nak);
        }
        else if(msg->msg_buf[i] >= PROP_SPECIAL_BEGIN && msg->msg_buf[i] <= PROP_SPECIAL_END)//special property begin
        {
            switch(msg->msg_buf[i])
            {
                case prop_LocalAction:
                {
                    mars_factory_uartMsgFromSlave(msg->msg_buf[i+1], mars_template_ctx);
                    i+=1;
                    break;
                }
                case prop_DataReportReason:
                {
                    #define  SPECIAL_VALID_BIT(n)  ((uint16_t)1 << (n - prop_LocalAction))
                    mars_template_ctx->status.DataReportReason = msg->msg_buf[i+1];
                    mars_template_ctx->special_reportflg |= SPECIAL_VALID_BIT(msg->msg_buf[i]);
                    //report_en = true;
                    i+=1;
                    break;
                }
                default:
                    LOGE("mars", "system error 收到未知属性,停止解析!!! (属性 = 0x%02X)", msg->msg_buf[i]);
                    return false;
                    break;
            }
        }
        else if(msg->msg_buf[i] >= PROP_CTRL_LOG_BEGIN && msg->msg_buf[i] <= PROP_CTRL_LOG_END)
        {
            extern uint8_t dry_fire_state;
            //LOGW("mars", "收到日志上报属性: 0x%02X", msg->msg_buf[i]);
            //printf("dry_fire_state:%d\r\n",dry_fire_state);
            if (msg->msg_buf[i] == 0xC0)
                i+=2;
            else if (msg->msg_buf[i] == 0xC1)
            {     
                printf("msg1:%d,msg2:%d\r\n",msg->msg_buf[i+1],msg->msg_buf[i+2]);          
                if (dry_fire_state == 0x01 && (msg->msg_buf[i+1] != 0x00 || msg->msg_buf[i+2] != 0x00))
                {
                    mars_beer_control(0x00);
                    LOGE("mars","inform CA user press button");
                    exit_dryburn_status(1, 0);
                }
                i+=2;
            }                
            else if (msg->msg_buf[i] == 0xC2)
                i+=2;
            else if (msg->msg_buf[i] == 0xC3)
                i+=15;
            else if (msg->msg_buf[i] == 0xC4)
                i+=1;
            else if (msg->msg_buf[i] == 0xC5)
                i+=1;
            else if (msg->msg_buf[i] == 0xC6)
                i+=2;
            else if (msg->msg_buf[i] == 0xC7)
                i+=2;            
            else if (msg->msg_buf[i] == 0xC8)
                i+=12;
            else if (msg->msg_buf[i] == 0xC9)
            {
                printf("msg1:%d\r\n",msg->msg_buf[i+1]);
                if (dry_fire_state == 0x01 && msg->msg_buf[i+1] != 0x00)
                {
                    mars_beer_control(0x00);
                    LOGE("mars","inform CA user press button");
                    exit_dryburn_status(1, 0);
                }
                i+=1;
            }
        }
        else
        {
            LOGE("mars", "收到未知属性,停止解析!!! (位置=%d 属性=0x%02X)", i, msg->msg_buf[i]);
            report_en = false;
            break;
        }  
    }

    process_property_diff(&dev_status_last, &g_user_example_ctx.status);

    if (is_asssist_change)
    {
        mars_ca_para_save();
        sync_wifi_property();
        report_wifi_property(NULL, NULL);
    }

    if (mars_template_ctx->status.HoodSpeed == 0x14 && g_user_cook_assist.ZnpySwitch == 0x01 && g_user_cook_assist.ZnpyOneExit == 0x00)
    {
        uint8_t buf[] = {prop_HoodSpeed, 2};
        Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf, sizeof(buf), 3);
        LOGI("mars", "从爆炒档切换到智能排烟: 下发烟机档位2档");
    }

    LOGW("mars", "解析结束: report_en = %d", report_en);


    if (!is_report_finish)
    {
        report_en = true;
        LOGW("mars", "首次上报未完成,本次report_en强制为true", report_en);
    }
    return report_en;
}

bool is_ca_para = false;
int Mars_property_set_callback(cJSON *root, cJSON *item, void *msg)
{
    if (NULL == root || NULL == msg){
        return -1;
    }

    uint8_t buf_setmsg[UARTMSG_BUFSIZE] = {0};
    uint16_t buf_len = 0;
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    is_ca_para = false;

    if ((item = cJSON_GetObjectItem(root, "SysPower")) != NULL && cJSON_IsNumber(item)) 
    {
        buf_setmsg[buf_len++] = prop_SysPower;
        buf_setmsg[buf_len++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "OtaCmdPushType")) != NULL && cJSON_IsNumber(item)) 
    {
        mars_template_ctx->status.OTAbyAPP = item->valueint;
        LOGI("mars", "app下发升级类型: %d (0-静默升级 1-app触发)", item->valueint);
    }

#if MARS_STOVE
    mars_stove_setToSlave(root, item, mars_template_ctx, buf_setmsg, &buf_len);
#endif

#if MARS_STEAMOVEN
    mars_steamoven_setToSlave(root, item, mars_template_ctx, buf_setmsg, &buf_len);
#endif

#if MARS_DISHWASHER
   mars_dishwasher_setToSlave(root, item, mars_template_ctx, buf_setmsg, &buf_len);
#endif

#if MARS_DISPLAY
    mars_display_setToSlave(root, item, mars_template_ctx, buf_setmsg, &buf_len);
#endif

    buf_setmsg[buf_len++] = prop_DataReportReason;
    buf_setmsg[buf_len++] = TRIGGER_SOURCE_APP;   //0：本地  1：APP  2：语音  3：小程序

    Mars_uartmsg_send(cmd_set, uart_get_seq_mid(),  buf_setmsg, buf_len, 3);

    if (is_ca_para)
    {
        LOGI("mars", "下发包含烹饪助手的指令");
        mars_ca_handle();
    }

    return 0;
}

void Mars_property_get_callback(char *property_name, cJSON *response)
{
    if (NULL == property_name || NULL == response){
        return -1;
    }
    
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();

    if (strcmp("WifiMac", property_name) == 0)
    {
        cJSON_AddStringToObject(response, "WifiMac", mars_template_ctx->macStr);
    }
    else if (strcmp("SysPower", property_name) == 0) 
    {
        cJSON_AddNumberToObject(response, "SysPower", mars_template_ctx->status.SysPower);
    }
    else if (strcmp("ElcSWVersion", property_name) == 0)
    {
        char tmpstr[8] = {0};
        sprintf(tmpstr, "%X.%X", \
                (uint8_t)(mars_template_ctx->status.ElcSWVersion >> 4), \
                (uint8_t)(mars_template_ctx->status.ElcSWVersion & 0x0F));
        cJSON_AddStringToObject(response, "ElcSWVersion", tmpstr);
    }
    else if (strcmp("ElcHWVersion", property_name) == 0)
    {
        char tmpstr[8] = {0};
        sprintf(tmpstr, "%X.%X", \
                (uint8_t)(mars_template_ctx->status.ElcHWVersion >> 4), \
                (uint8_t)(mars_template_ctx->status.ElcHWVersion & 0x0F));
        cJSON_AddStringToObject(response, "ElcHWVersion", tmpstr);
    }
    else if (strcmp("PwrSWVersion", property_name) == 0)
    {
        char tmpstr[8] = {0};
        sprintf(tmpstr, "%X.%X", \
                (uint8_t)(mars_template_ctx->status.PwrSWVersion >> 4), \
                (uint8_t)(mars_template_ctx->status.PwrSWVersion & 0x0F));
        cJSON_AddStringToObject(response, "PwrSWVersion", tmpstr);
    }
    else if (strcmp("PwrHWVersion", property_name) == 0)
    {
        char tmpstr[8] = {0};
        sprintf(tmpstr, "%X.%X", \
                (uint8_t)(mars_template_ctx->status.PwrHWVersion >> 4), \
                (uint8_t)(mars_template_ctx->status.PwrHWVersion & 0x0F));
        cJSON_AddStringToObject(response, "PwrHWVersion", tmpstr);
    }
}

void mars_property_data(char* msg_seq, char **str_out)
{
    mars_template_ctx_t *t_mars_template_ctx = mars_dm_get_ctx();
    cJSON *proot = cJSON_CreateObject();
    if(NULL != proot)
    {
        //LOGI("mars", "mars_property_data: 1");
        if (NULL != msg_seq)
        {
            cJSON *item_csr = cJSON_CreateObject();
            if (item_csr == NULL) 
            {
                cJSON_Delete(proot);
                return;
            }
            cJSON_AddStringToObject(item_csr, "seq", msg_seq);
            cJSON_AddItemToObject(proot, "CommonServiceResponse", item_csr);
        }
        //LOGI("mars", "mars_property_data: 2");
        // cJSON_AddStringToObject(proot, "WifiMac", t_mars_template_ctx->macStr);

        if (t_mars_template_ctx->steamoven_spec_reportflg & STOV_SPEC_VALID_BIT(prop_LSteamGear))
        {
            //LOGI("mars", "上报左腔蒸汽档位: %d", t_mars_template_ctx->status.LSteamGear);
            cJSON_AddNumberToObject(proot, "LSteamGear",  t_mars_template_ctx->status.LSteamGear);
        }
        if (t_mars_template_ctx->steamoven_spec_reportflg & STOV_SPEC_VALID_BIT(prop_LMicroWaveGear))
        {
            //LOGI("mars", "上报左腔微波档位: %d", t_mars_template_ctx->status.LMicroGear);
            cJSON_AddNumberToObject(proot, "LMicroWaveGear",  t_mars_template_ctx->status.LMicroGear);
        }
        t_mars_template_ctx->steamoven_spec_reportflg = 0;

        for (uint8_t index=prop_ElcSWVersion; index<=prop_ErrCodeShow; ++index)
        {
            if (t_mars_template_ctx->common_reportflg & VALID_BIT(index))
            {
                switch (index)
                {
                    case prop_ElcSWVersion:
                    {
                        char tmpstr[8] = {0};
                        sprintf(tmpstr, "%X.%X", \
                                (uint8_t)(t_mars_template_ctx->status.ElcSWVersion >> 4), \
                                (uint8_t)(t_mars_template_ctx->status.ElcSWVersion & 0x0F));
                        cJSON_AddStringToObject(proot, "ElcSWVersion", tmpstr);
                        break;
                    }
                    case prop_ElcHWVersion:
                    {
                        char tmpstr[8] = {0};
                        sprintf(tmpstr, "%X.%X", \
                                (uint8_t)(t_mars_template_ctx->status.ElcHWVersion >> 4), \
                                (uint8_t)(t_mars_template_ctx->status.ElcHWVersion & 0x0F));
                        cJSON_AddStringToObject(proot, "ElcHWVersion", tmpstr);
                        break;
                    }
                    case prop_PwrSWVersion:
                    {
                        char tmpstr[8] = {0};
                        sprintf(tmpstr, "%X.%X", \
                                (uint8_t)(t_mars_template_ctx->status.PwrSWVersion >> 4), \
                                (uint8_t)(t_mars_template_ctx->status.PwrSWVersion & 0x0F));
                        cJSON_AddStringToObject(proot, "PwrSWVersion", tmpstr);
                        break;
                    }
                    case prop_PwrHWVersion:
                    {
                        char tmpstr[8] = {0};
                        sprintf(tmpstr, "%X.%X", \
                                (uint8_t)(t_mars_template_ctx->status.PwrHWVersion >> 4), \
                                (uint8_t)(t_mars_template_ctx->status.PwrHWVersion & 0x0F));
                        cJSON_AddStringToObject(proot, "PwrHWVersion", tmpstr);
                        break;
                    }
                    case prop_SysPower:
                    {
                        //LOGI("mars", "上报: 系统开关status.SysPower: %d", t_mars_template_ctx->status.SysPower);
                        cJSON_AddNumberToObject(proot, "SysPower", t_mars_template_ctx->status.SysPower);
                        break;
                    }
                    case prop_ErrCode:
                    {
                        //LOGI("mars", "上报: 警报状态status.ErrCode: %d", t_mars_template_ctx->status.ErrCode);
                        cJSON_AddNumberToObject(proot, "ErrorCode", t_mars_template_ctx->status.ErrCode);
                        break;
                    }
                    case prop_ErrCodeShow:
                    {
                        //LOGI("mars", "上报: 显示报警status.ErrorCodeShow: %d", t_mars_template_ctx->status.ErrCodeShow);
                        cJSON_AddNumberToObject(proot, "ErrorCodeShow", t_mars_template_ctx->status.ErrCodeShow);
                        break;
                    }

                    default:
                        break;
                }
            }
        }
        t_mars_template_ctx->common_reportflg = 0;
        
#if MARS_STOVE
        mars_stove_changeReport(proot, t_mars_template_ctx);
#endif

#if MARS_STEAMOVEN
        mars_steamoven_changeReport(proot, t_mars_template_ctx);
#endif

#if MARS_DISHWASHER
        mars_dishwasher_changeReport(proot, t_mars_template_ctx);
#endif

#if MARS_DISPLAY
        mars_display_changeReport(proot, t_mars_template_ctx);
#endif

        if (t_mars_template_ctx->special_reportflg & SPECIAL_VALID_BIT(prop_DataReportReason))
        {
            LOGI("mars", "本次上报触发源: %d (0=本地 1=APP 2=语音 3=微信)", t_mars_template_ctx->status.DataReportReason);
            cJSON_AddNumberToObject(proot, "DataReportReason",  t_mars_template_ctx->status.DataReportReason);
        }
        t_mars_template_ctx->special_reportflg = 0;

        *str_out = cJSON_Print(proot); //cJSON_PrintUnformatted cJSON_Print
        if (*str_out == NULL)
        {
            LOGE("mars", "error: cJSON_Print() failed !!!");
        }
    }

    if(NULL != proot)
    {
        cJSON_Delete(proot);
        proot = NULL;
    }
}

int mars_dev_event_callback(input_event_t *event_id, void *param)
{
    LOGI("mars", "产测收到子事件: code = %d (0:发送版本 2:wifi扫描 3:wifi连接  4:http上报)", event_id->code);
	switch(event_id->code)
	{
        case MARS_EVENT_SEND_SWVERSION:
        {
            LOGI("mars", "产测: 1-开始下发通讯板版本");
            mars_store_swversion();
	        aos_msleep(500);
            aos_post_event(EV_DEVFUNC, MARS_EVENT_FACTORY_WIFITEST, 0);
            break;
        }
        case MARS_EVENT_FACTORY_WIFITEST:
        {
            mars_factory_wifiTest();
            break;
        }
        case MARS_EVENT_FACTORY_WASS:
        {
            mars_factory_awss();
            break;
        }
        case MARS_EVENT_FACTORY_LINKKEY:
        {
            mars_factory_linkkey();
            break;
        }
        case MARS_EVENT_FACTORY_RESET:
        {            
            break;
        }
        default:
            break;
	}

	return 0;
}

void report_wifi_property(void *arg1, void *arg2)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ComSWVersion",           aos_get_app_version());
    cJSON_AddStringToObject(root, "WifiMac",                g_user_example_ctx.macStr);
    cJSON_AddNumberToObject(root, "SmartSmokeSwitch",       g_user_cook_assist.ZnpySwitch);
    int SmartSmokeValid = g_user_cook_assist.ZnpyOneExit? 0 : g_user_cook_assist.ZnpySwitch;
    cJSON_AddNumberToObject(root, "SmartSmokeValid",        SmartSmokeValid);
    cJSON_AddNumberToObject(root, "RAuxiliarySwitch",       g_user_cook_assist.RAuxiliarySwitch);
    cJSON_AddNumberToObject(root, "RAuxiliaryTemp",         g_user_cook_assist.RAuxiliaryTemp);
    cJSON_AddNumberToObject(root, "RMovePotLowHeatSwitch",  g_user_cook_assist.RMovePotLowHeatSwitch);
    cJSON_AddNumberToObject(root, "RMovePotLowHeatOffTime", g_user_cook_assist.RMovePotLowHeatOffTime);
    cJSON_AddNumberToObject(root, "OilTempSwitch",          g_user_cook_assist.ROilTempSwitch);
    cJSON_AddNumberToObject(root, "CookingCurveSwitch",     g_user_cook_assist.CookingCurveSwitch);
    cJSON_AddNumberToObject(root, "RDryFireSwitch",         g_user_cook_assist.RDryFireSwitch);
    cJSON_AddNumberToObject(root, "RDryFireState",          g_user_cook_assist.RDryFireState);
    cJSON_AddNumberToObject(root, "RDryFireTempCurveSwitch",g_user_cook_assist.RDryFireTempCurveSwitch);
    cJSON_AddNumberToObject(root, "DryFireUserSwitch",      g_user_cook_assist.RDryFireUserType);



    cJSON* sub = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(root, "ROneKeyRecipeState", sub);
    cJSON_AddNumberToObject(sub, "RecipeSwitch",            g_user_cook_assist.ROneKeyRecipeSwitch);
    cJSON_AddNumberToObject(sub, "RecipeType",              g_user_cook_assist.ROneKeyRecipeType);
    cJSON_AddNumberToObject(sub, "RecipeTimeSet",           g_user_cook_assist.ROneKeyRecipeTimeSet);
    cJSON_AddNumberToObject(sub, "RecipeTimeLeft",          g_user_cook_assist.ROneKeyRecipeTimeLeft);

    char* str = cJSON_Print(root);
    if (str != NULL)
    {
        user_post_property_json(str);
        cJSON_free(str);
        str = NULL;
    }

    if (root != NULL)
    {
        cJSON_Delete(root);
        root = NULL;
    }
}

void sync_wifi_property(void)
{
    uint8_t   send_buf[32] = {0};
    uint16_t  buf_len = 0;
    send_buf[buf_len++] = prop_FsydSwitch;
    send_buf[buf_len++] = g_user_cook_assist.ZnpySwitch;
    send_buf[buf_len++] = prop_FsydValid;
    send_buf[buf_len++] = g_user_cook_assist.ZnpyOneExit? 0 : g_user_cook_assist.ZnpySwitch; // (mars_dm_get_ctx()->status.SysPower)? (g_user_cook_assist.ZnpyOneExit? 0 : g_user_cook_assist.ZnpySwitch) : 0;
    send_buf[buf_len++] = prop_RAuxiliarySwitch;
    send_buf[buf_len++] = g_user_cook_assist.RAuxiliarySwitch;
    send_buf[buf_len++] = prop_RAuxiliaryTemp;
    send_buf[buf_len++] = (g_user_cook_assist.RAuxiliaryTemp >> 8);
    send_buf[buf_len++] = (g_user_cook_assist.RAuxiliaryTemp & 0xFF);
    send_buf[buf_len++] = prop_RMovePotLowHeatSwitch;
    send_buf[buf_len++] = g_user_cook_assist.RMovePotLowHeatSwitch;
    send_buf[buf_len++] = prop_OilTempSwitch;
    send_buf[buf_len++] = g_user_cook_assist.ROilTempSwitch;
    send_buf[buf_len++] = prop_RONEKEYRECIPE;
    send_buf[buf_len++] = g_user_cook_assist.ROneKeyRecipeSwitch;
    send_buf[buf_len++] = g_user_cook_assist.ROneKeyRecipeType;
    send_buf[buf_len++] = g_user_cook_assist.ROneKeyRecipeTimeSet;
    send_buf[buf_len++] = prop_RONEKEYRECIPELeftTime;
    send_buf[buf_len++] = g_user_cook_assist.ROneKeyRecipeSwitch? (g_user_cook_assist.ROneKeyRecipeTimeLeft? 1 : 2) : 0;
    send_buf[buf_len++] = g_user_cook_assist.ROneKeyRecipeTimeLeft;
    send_buf[buf_len++] = 0x2D;
    send_buf[buf_len++] = g_user_cook_assist.RDryFireSwitch;
    send_buf[buf_len++] = 0x2E;
    send_buf[buf_len++] = g_user_cook_assist.RDryFireState;
    Mars_uartmsg_send(cmd_store, uart_get_seq_mid(), send_buf, buf_len, 3);
    LOGI("mars", "下发烹饪助手状态: [智能排烟开关=%d 智能排烟运行=%d] [辅助控温开关=%d 辅助控温温度=%d] [移锅小火开关=%d 移锅小火时间=%d] [油温显示开关=%d 烹饪曲线开关=%d]  [一键菜谱开关=%d 一键菜谱类别=%d 一键菜谱设置时间=%d 一键菜谱剩余时间=%d] [防干烧开关=%d 防干烧状态=%d]",
    g_user_cook_assist.ZnpySwitch,
    g_user_cook_assist.ZnpyOneExit? 0 : g_user_cook_assist.ZnpySwitch,
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
    g_user_cook_assist.RDryFireState);
}

#define MARS_PROPGET_INTERVAL     (5 * 60 * 1000)
static aos_timer_t mars_propget = {.hdl = NULL};
void report_property_timer_start(void)
{
    if (mars_propget.hdl == NULL)
    {
        LOGI("mars", "上报通讯板板当前状态 定时器启动......(周期=%d分钟)", MARS_PROPGET_INTERVAL/(60 * 1000));
        aos_timer_new_ext(&mars_propget, report_wifi_property, NULL, MARS_PROPGET_INTERVAL, 1, 1);
    }
}

void mars_devmgr_afterConnect(void)
{   
    mars_devmngr_getstatus(NULL, NULL);

    get_cloud_time(NULL, NULL);
    get_cloud_time_timer_start();

    report_wifi_property(NULL, NULL);
  //report_property_timer_start();

    get_mcook_weather();
    get_mcook_weather_timer_start();
}

int decimal_bcd_code(int decimal)
{
	int sum = 0;  //sum返回的BCD码
 
	for (int i = 0; decimal > 0; i++)
	{
		sum |= ((decimal % 10 ) << ( 4*i)); 
		decimal /= 10;
	}
 
	return sum;
}

int mars_devmgr_init(void)
{
    aos_register_event_filter(EV_DEVFUNC, mars_dev_event_callback, NULL);

    //转换wifi模组版本号
    // char tmp_version[10] ={0};
    // strncpy(tmp_version, aos_get_app_version(), sizeof(tmp_version)); //like this: "0.1.4"
    // tmp_version[1] = 0;
    // tmp_version[3] = 0;
    // tmp_version[5] = 0;
    // g_user_example_ctx.status.WifiSWVersion[0] = (uint8_t)atoi(tmp_version);
    // g_user_example_ctx.status.WifiSWVersion[1] = (uint8_t)atoi(tmp_version+2);
    // g_user_example_ctx.status.WifiSWVersion[2] = (uint8_t)atoi(tmp_version+4); 

    char tmp_version[10] ={0};
    strncpy(tmp_version, aos_get_app_version(), sizeof(tmp_version)); //like this: "1.4"
    tmp_version[1] = 0;
    tmp_version[3] = 0;
    int ver = decimal_bcd_code((uint8_t)atoi(tmp_version)*10 + (uint8_t)atoi(tmp_version+2));
    g_user_example_ctx.status.WifiSWVersion[0] = ver;
    LOGI("mars", "app ver: %s (%02d 0x%02X)", aos_get_app_version(), g_user_example_ctx.status.WifiSWVersion[0], g_user_example_ctx.status.WifiSWVersion[0]);

    mars_irtInit();
    mars_ca_init();
    sync_wifi_property();
    mars_devmngr_getstatus(NULL, NULL);
    
    return 0;
}