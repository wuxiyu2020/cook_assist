/*
 * @Description  :
 * @Author       : zhoubw
 * @Date         : 2022-08-17 16:57:42
 * @LastEditors: Zhouxc
 * @LastEditTime: 2024-07-01 17:31:55
 * @FilePath     : /et70-ca3/Products/example/mars_template/mars_devfunc/mars_stove.c
 */

#include <stdio.h>
#include <math.h>
#include "cJSON.h"
#include "iot_export.h"
#include <hal/wifi.h>

#include "../dev_config.h"
#include "../mars_driver/mars_cloud.h"
#include "cook_assistant/cook_wrapper.h"
#include "cook_assistant/auxiliary_cook/aux_api.h"
#include "mars_stove.h"
#include "mars_httpc.h"
#if 1
#include "mars_devfunc/irt102m/drv_sensor_irt102m.h"
#include "mars_devfunc/irt102m/Lib_Irt102mDataCalibra.h"
static uint8_t i2c_temp_display_cont = 0;   // i2C烹饪助手，下发头显温度值计数，4次发1次
#endif

#if MARS_STOVE

#define VALID_BIT(n)    ((uint64_t)1 << (n - prop_LStoveStatus))
#define SVALID_BIT(n)   ((uint64_t)1 << (n - prop_RadarGear))
#define ONE_KEY_RECIPE_KV  "onekey_recipe"

#pragma pack(1)
#define TEMP_PACK_SUM       24
#define TEMP_FREQ           40

char *AuxMode[]={"未设定","炒模式","煮模式","煎模式","炸模式"};
aos_task_t AuxTime;
static aos_timer_t light_close_timer;

void timer_func_close_light(void *arg1, void *arg2)
{
    char buf_setmsg[8] = {0};
    uint8_t len = 0;
    buf_setmsg[len++] = prop_HoodLight;                    //临时根据雷达信号设置烟机照明方便测试
    buf_setmsg[len++] = 0;
    Mars_uartmsg_send(cmd_set, uart_get_seq_mid(), &buf_setmsg, sizeof(buf_setmsg), 3);

    if (light_close_timer.hdl != NULL)
    {
        aos_timer_stop(&light_close_timer);
        aos_timer_free(&light_close_timer);
        LOGI("mars", "关闭照明定时器 (time=%d秒)", 2);
    }
}

void AuxTimer_function(void *arg)
{
    while(1)
    {
        mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
        aos_msleep(1000);
        LOGI("mars","lefttime:%d",mars_template_ctx->status.AuxCookLeftTime_s);
        if(mars_template_ctx->status.AuxCookLeftTime_s != 0)
        {
            mars_template_ctx->status.AuxCookLeftTime_s -= 1;
        }
        if(mars_template_ctx->status.AuxCookLeftTime_s < 0)
        {
            mars_template_ctx->status.AuxCookLeftTime_s = 0;
        }
        if(mars_template_ctx->status.AuxCookLeftTime_s % 60 == 0 && mars_template_ctx->status.AuxCookLeftTime_s != 0)
        {
            LOGI("mars","一键烹饪模式时间设置为time send:%d",mars_template_ctx->status.AuxCookLeftTime_s/60);
            char buf_setmsg[8] = {0};
            int buf_len = 0;
            buf_setmsg[buf_len++] = prop_AuxCookLeftTime;
            buf_setmsg[buf_len++] = mars_template_ctx->status.AuxCookLeftTime_s/60;
            Mars_uartmsg_send(cmd_store,uart_get_seq_mid(),buf_setmsg,buf_len,3);
        }
        else if(mars_template_ctx->status.AuxCookLeftTime_s == 0 && mars_template_ctx->status.AuxCookSetTime != 0)
        {
            LOGI("mars","一键烹饪模式时间设置为time send:0");
            char buf_setmsg[8] = {0};
            int buf_len = 0;
            buf_setmsg[buf_len++] = prop_AuxCookLeftTime;
            buf_setmsg[buf_len++] = 0;
            Mars_uartmsg_send(cmd_store,uart_get_seq_mid(),buf_setmsg,buf_len,3);
        }
    }
    aos_task_exit(0);
}

// typedef struct M_STOVE_TEMP{
//     uint64_t time;
//     int r_temp;
//     // int l_temp;
// }m_stovetemp_st;

// static m_stovetemp_st g_stove_temp[TEMP_PACK_SUM+1] = {0};

#pragma pack()



#define STR_TEMPERINFO "{\"temp\":\"%d\",\"time\":\"%ld\"},"
#define DRY_TEMPERINFO  "{\\\"temp\\\":\\\"%d\\\",\\\"time\\\":\\\"%ld\\\"},"
#define STR_TEMPERPOST  "{\"deviceMac\":\"%s\",\"iotId\":\"%s\",\"token\":\"%s\",\"curveKey\":\"%d\",\"cookCurveTempDTOS\":[%s]}"
#define STR_TEMPERPOST_HEAD  "{\"deviceMac\":\"%s\",\"iotId\":\"%s\",\"token\":\"%s\",\"curveKey\":\"%d\",\"cookCurveTempDTOS\":["
#define STR_TEMPERPOST_TAIL "]}"

static char g_stove_tempstr[2048] = {0};
static uint8_t g_stove_temp_cnt = 0;
uint8_t RAuxiliaryBeer = 0;
extern mars_cook_assist_t  g_user_cook_assist;
extern bool is_asssist_change;

// #define STR_TEMPERPOST  "{\"deviceMac\":\"%s\",\"iotId\":\"%s\",\"Temp\":\"%d\",\"curveKey\":\"%d\",\"creteTime\":\"%d\"}"
// #define URL_TEMPERPOST "http://mcook.dev.marssenger.net/menu-anon/addCookCurve"
// #define URL_TEMPERPOST "192.168.1.108:12300"
#if 1
void mars_temper_report(uint16_t msg_len)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    if (0 == mars_template_ctx->cloud_connected || !mars_getTimestamp_ms()){
        return;
    }
#if 0
    int ret = -1;
    char http_url[] = URL_TEMPERPOST;

    //为加快处理速度，使用字符串打印组件json包
    httpc_req_t http_req = {
        .type = HTTP_POST,
        .version = HTTP_VER_1_1,
        .resource = NULL,
        .content = NULL,
        .content_len = 0,
    };

    mprintf_d("Temppost[%d]:%s", msg_len, g_stove_tempstr);

    http_req.content = (char *)aos_malloc(msg_len + 200);
    if (http_req.content){
        sprintf(http_req.content, STR_TEMPERPOST, \
                mars_template_ctx->macStr, \
                mars_template_ctx->device_name, \
                mars_cloud_tokenget(), \
                mars_template_ctx->status.CurKeyvalue, \
                g_stove_tempstr);

        http_req.content_len = strlen(http_req.content);

        mars_http_getpost(HTTP_DEFAULT, &http_url, &http_req, NULL, 0, 0);

        aos_free(http_req.content);
        http_req.content = NULL;
    }else{
        http_req.content = NULL;
        mprintf_e("malloc failed.");
    }
#else
    http_msg_t temp_http = {0};
    char *http_temp_msg = (char *)aos_malloc(msg_len + 250);
    if (http_temp_msg){
        sprintf(http_temp_msg, \
                STR_TEMPERPOST, \
                mars_template_ctx->macStr, \
                mars_template_ctx->device_name, \
                mars_cloud_tokenget(), \
                mars_template_ctx->status.CurKeyvalue, \
                g_stove_tempstr);


        LOGI("mars", "烹饪曲线http上报: %s", http_temp_msg);
        temp_http.http_method = HTTP_TEMPCURVE;
        temp_http.msg_str = http_temp_msg;
        temp_http.msg_len = msg_len+250;
        send_http_to_queue(&temp_http);
    }
#endif
}
#endif

void static m_tempreport_pre(int16_t i16_tempvalue, mars_template_ctx_t *mars_template_ctx)
{
    static uint64_t time_temp_mqtt = 0;
    static int16_t  temp_last = 0;
    static uint8_t temp_report_cnt = 40;
    static int16_t i16_tempvalue_last = 0;
    if (mars_template_ctx->cloud_connected && mars_getTimestamp_ms() && mars_template_ctx->status.RStoveStatus)
    {
        if (TEMP_FREQ <= ++temp_report_cnt)
        {
            temp_report_cnt = 0;
            char tmp_str[50] = {0};
            mars_template_ctx->status.ROilTemp = i16_tempvalue;
            if (g_user_cook_assist.CookingCurveSwitch)
            {
                sprintf(tmp_str, STR_TEMPERINFO, (uint32_t)(mars_getTimestamp_ms()/1000), i16_tempvalue);
                strcat(g_stove_tempstr, tmp_str);
                g_stove_temp_cnt++;
            }
            else
            {
                memset(g_stove_tempstr, 0, sizeof(g_stove_tempstr));
                g_stove_temp_cnt = 0;
            }
        }
    }

    if (g_user_cook_assist.CookingCurveSwitch &&
         ((mars_template_ctx->status.RStoveStatus && TEMP_PACK_SUM <= g_stove_temp_cnt) || (!mars_template_ctx->status.RStoveStatus && 0 != g_stove_temp_cnt)))
    {
        //如果右灶打开，并且获取温度数量达到每包上限，则整包上报
        //如果右灶关闭，并且还有温度数据没上报，则将剩下数据上报
        int temp_len = strlen(g_stove_tempstr);
        g_stove_tempstr[temp_len-1] = 0;
        // strcat(g_stove_tempstr, STR_TEMPERPOST_TAIL);
        mars_temper_report(temp_len);
        memset(g_stove_tempstr, 0, sizeof(g_stove_tempstr));
        g_stove_temp_cnt = 0;
    }

    static uint32_t Send_temp = 0;          //用于计算采集到平均温度发送给显示板
    static uint8_t Send_temp_count = 0;     //当前采集到的平均温度
    //if (g_user_cook_assist.ROilTempSwitch)
    {
/*         //3s下发一次当前的温度，为了方式跳动偏差，此处采取平均值的方式
        if ((time_temp_mqtt == 0) || (aos_now_ms() - time_temp_mqtt) >= (1*5000))
        {
            // char property_payload[64] = {0};
            // sprintf(property_payload, "{\"ROilTemp\": %d}", mars_template_ctx->status.ROilTemp);
            // user_post_property_json(property_payload);
            Send_temp /= Send_temp_count;       //取过去3s的温度平均

            if(time_temp_mqtt == 0)
            {
                mars_sync_tempture(i16_tempvalue, mars_template_ctx->status.REnvTemp);
                LOGI("mars","首次上报，瞬时温度:%d，此时平均温度尚未计算\r\n",i16_tempvalue);
            }
            else
            {
                mars_sync_tempture(i16_tempvalue, mars_template_ctx->status.REnvTemp);
                LOGI("mars","瞬时Send_temp:%d,平均后Send_time:%d\r\n",i16_tempvalue,Send_temp);
            }

            //mars_sync_tempture(Send_temp, mars_template_ctx->status.REnvTemp);
            Send_temp = 0;
            Send_temp_count = 0;
            time_temp_mqtt = aos_now_ms();
        }
        else
        {
            Send_temp += i16_tempvalue;
            Send_temp_count++;
        } */

        if ((i16_tempvalue != temp_last) && ((aos_now_ms() - time_temp_mqtt) > (1*1000)))
        {
            mars_sync_tempture(i16_tempvalue, mars_template_ctx->status.REnvTemp);
            temp_last = i16_tempvalue;
            time_temp_mqtt = aos_now_ms();
        }
    }
}

static char g_stove_tempfgs[2048] = {0};
void static m_tempreport_fgs(int16_t i16_tempvalue, mars_template_ctx_t *mars_template_ctx)
{
    //曲线上报开关打开后 右灶打开 或 右灶关闭但还有剩余数据未上报
    if (g_user_cook_assist.RDryFireTempCurveSwitch &&  (mars_getTimestamp_ms() > 0) && (mars_template_ctx->status.RStoveStatus || \
    (mars_template_ctx->status.RStoveStatus == 0 && strlen(g_stove_tempfgs) != 0) ))
    {
        char tmp_str[5] = {0};
        //sprintf(tmp_str, DRY_TEMPERINFO, i16_tempvalue, (uint32_t)(mars_getTimestamp_ms()/1000));
        sprintf(tmp_str,"%d,",i16_tempvalue);
        strcat(g_stove_tempfgs, tmp_str);

        //缓冲区满了或者关火后有剩余数据未上报
        if (strlen(g_stove_tempfgs) >= (sizeof(g_stove_tempfgs) - 10) || (mars_template_ctx->status.RStoveStatus == 0 && strlen(g_stove_tempfgs) != 0) )
        {
            int len = strlen(g_stove_tempfgs);
            if (len > 0 && g_stove_tempfgs[len - 1] == ',') {
                g_stove_tempfgs[len - 1] = '\0'; // 将最后一个逗号替换为字符串结束符
            }

            LOGI("mars", "dryburn curve buffer full防干烧曲线数据缓冲区已满: %d %d", sizeof(g_stove_tempfgs), strlen(g_stove_tempfgs));
            int http_temp_msg_len = (strlen(g_stove_tempfgs) + 250);
            char* http_temp_msg = (char*)aos_malloc(http_temp_msg_len);             //在http处理线程中释放
            if (mars_template_ctx->cloud_connected && (http_temp_msg != NULL))
            {
                #define STR_FGSTEMPERPOST  "{\"deviceMac\":\"%s\",\"iotId\":\"%s\",\"curveTempDtos\":\"[%s]\"}"
                memset(http_temp_msg, 0x00, http_temp_msg_len);

                sprintf(http_temp_msg, \
                    STR_FGSTEMPERPOST, \
                    mars_template_ctx->macStr, \
                    mars_template_ctx->device_name, \
                    g_stove_tempfgs);

                http_msg_t temp_http = {0};
                temp_http.http_method = HTTP_TEMPCURVEFGS;
                temp_http.msg_str = http_temp_msg;
                temp_http.msg_len = http_temp_msg_len;
                LOGI("mars", "dryburn curve防干烧曲线http上报: %s", http_temp_msg);
                send_http_to_queue(&temp_http);
                memset(g_stove_tempfgs, 0x00, sizeof(g_stove_tempfgs));
            }
            else
            {
                LOGW("mars", "设备未联网,本次防干烧曲线数据未上报!!!");
                memset(g_stove_tempfgs, 0x00, sizeof(g_stove_tempfgs));
            }
        }
    }
    else
    {
        //LOGW("mars", "dryburn curve close!!!");
        memset(g_stove_tempfgs, 0x00, sizeof(g_stove_tempfgs));
    }
}

void mars_stove_cookassistant(uartmsg_que_t *msg,
                                mars_template_ctx_t *mars_template_ctx)
{
    int16_t i16_tempvalue = (int16_t)(msg->msg_buf[1] << 8) | (int16_t)msg->msg_buf[2];
    mars_template_ctx->status.ROilTemp = i16_tempvalue;
    i16_tempvalue = (int16_t)(msg->msg_buf[4] << 8) | (int16_t)msg->msg_buf[5];
    mars_template_ctx->status.REnvTemp = i16_tempvalue;
    cook_assistant_input(INPUT_RIGHT,
                        mars_template_ctx->status.ROilTemp * 10,
                        mars_template_ctx->status.REnvTemp * 10);
    prepare_gear_change_task();

    if ((g_user_cook_assist.RAuxiliarySwitch == 1) && (RAuxiliaryBeer == 0))
    {
        if (mars_template_ctx->status.ROilTemp >= g_user_cook_assist.RAuxiliaryTemp)
        {
            mars_beer_control(1);
            RAuxiliaryBeer = 1;
            LOGI("mars", "辅助控火: 蜂鸣器鸣叫(实时温度=%d 控火温度=%d)", mars_template_ctx->status.ROilTemp, g_user_cook_assist.RAuxiliaryTemp);
        }
    }

    m_tempreport_pre(mars_template_ctx->status.ROilTemp, mars_template_ctx);
}

void mars_stove_uartMsgFromSlave(uartmsg_que_t *msg,
                                mars_template_ctx_t *mars_template_ctx,
                                uint16_t *index, bool *report_en, uint8_t *nak)
{
    switch (msg->msg_buf[(*index)])
    {
        case prop_LStoveStatus:
        {
            if (mars_template_ctx->status.LStoveStatus != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 左灶状态变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.LStoveStatus, msg->msg_buf[(*index)+1]);
                *report_en = true;
                mars_template_ctx->status.LStoveStatus = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_RStoveStatus:
        {
            if (mars_template_ctx->status.RStoveStatus != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 右灶状态变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.RStoveStatus, msg->msg_buf[(*index)+1]);
                *report_en = true;
                mars_template_ctx->status.RStoveStatus = msg->msg_buf[(*index)+1];

                if (msg->msg_buf[(*index)+1] == 0x01)  //打开右灶
                {
                    mars_template_ctx->status.CurKeyvalue = rand();

                    //打开右灶通知烹饪助手算法
                    set_fire_status(INPUT_RIGHT,1);
                    LOGI("mars", "右灶状态改变: 右灶打开 (生成随机数=%d)", mars_template_ctx->status.CurKeyvalue);
                }

                if (msg->msg_buf[(*index)+1] == 0x00)   //关闭右灶
                {
                    if (g_user_cook_assist.RAuxiliarySwitch == 1)
                    {
                        g_user_cook_assist.RAuxiliarySwitch = 0;
                        g_user_cook_assist.RAuxiliaryTemp   = 0;
                        set_temp_control_switch(g_user_cook_assist.RAuxiliarySwitch, INPUT_RIGHT);
                        set_temp_control_target_temp(g_user_cook_assist.RAuxiliaryTemp, INPUT_RIGHT);
                        is_asssist_change = true;
                        LOGI("mars", "辅助控火: 右灶关闭 ==> 辅助控火开关=0");
                    }

                    if (g_user_cook_assist.ROneKeyRecipeSwitch == 1)
                    {
                        g_user_cook_assist.ROneKeyRecipeSwitch   = 0;
                        g_user_cook_assist.ROneKeyRecipeType     = 0;
                        g_user_cook_assist.ROneKeyRecipeTimeSet  = 0;
                        g_user_cook_assist.ROneKeyRecipeTimeLeft = 0;
                        quick_cooking_switch(g_user_cook_assist.ROneKeyRecipeSwitch, g_user_cook_assist.ROneKeyRecipeType, g_user_cook_assist.ROneKeyRecipeTimeSet*60, 15*60); //g_user_cook_assist.ROneKeyRecipeTimeSet*60
                        is_asssist_change = true;
                        LOGI("mars", "辅助控火: 右灶关闭 ==> 一键烹饪开关=0");
                    }
                }
            }

            set_ignition_switch(mars_template_ctx->status.RStoveStatus, 1);
            set_aux_ignition_switch(mars_template_ctx->status.RStoveStatus, 1);
            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_HoodStoveLink:
        {
            if (mars_template_ctx->status.HoodStoveLink != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 烟灶联动发生变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.HoodStoveLink, msg->msg_buf[(*index)+1]);
                *report_en = true;
                mars_template_ctx->status.HoodStoveLink = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_HoodLightLink:
        {
            if (mars_template_ctx->status.HoodLightLink != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 烟灯联动发生变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.HoodLightLink, msg->msg_buf[(*index)+1]);
                *report_en = true;
                mars_template_ctx->status.HoodLightLink = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_LightStoveLink:
        {
            LOGI("mars", "灯灶联动(%d - %d)", mars_template_ctx->status.LightStoveLink, msg->msg_buf[(*index)+1]);
            if (mars_template_ctx->status.LightStoveLink != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 灯灶联动发生变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.LightStoveLink, msg->msg_buf[(*index)+1]);
                *report_en = true;
                mars_template_ctx->status.LightStoveLink = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_RStoveTimingState:
        {
            if (mars_template_ctx->status.RStoveTimingState != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 右灶定时状态变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.RStoveTimingState, msg->msg_buf[(*index)+1]);
                *report_en = true;
                mars_template_ctx->status.RStoveTimingState = msg->msg_buf[(*index)+1];

                if (msg->msg_buf[(*index)+1] == 0x03)
                {
                    user_post_event_json(EVENT_RIGHT_STOVE_TIMER_FINISH);
                    LOGI("mars", "推送事件: 右灶定时完成推送 (%d -> %d) (0-停止 1-运行 2-暂停 3-完成)", mars_template_ctx->status.RStoveTimingState, msg->msg_buf[(*index)+1]);
                }
            }

            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_LStoveTimingState:
        {
            if (mars_template_ctx->status.LStoveTimingState != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 左灶定时状态变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.LStoveTimingState, msg->msg_buf[(*index)+1]);
                *report_en = true;
                mars_template_ctx->status.LStoveTimingState = msg->msg_buf[(*index)+1];

                if (msg->msg_buf[(*index)+1] == 0x03)
                {
                    user_post_event_json(EVENT_LEFT_STOVE_TIMER_FINISH);
                    LOGI("mars", "推送事件: 左灶定时完成推送 (%d -> %d) (0-停止 1-运行 2-暂停 3-完成)", mars_template_ctx->status.LStoveTimingState, msg->msg_buf[(*index)+1]);
                }
            }

            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_RStoveTimingSet:
        {
            if (mars_template_ctx->status.RStoveTimingSet != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 右炤定时时间变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.RStoveTimingSet, msg->msg_buf[(*index)+1]);
                *report_en = true;
                mars_template_ctx->status.RStoveTimingSet = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_LStoveTimingSet:
        {
            if (mars_template_ctx->status.LStoveTimingSet != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 左炤定时时间变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.LStoveTimingSet, msg->msg_buf[(*index)+1]);
                *report_en = true;
                mars_template_ctx->status.LStoveTimingSet = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_RStoveTimingLeft:
        {
            if (mars_template_ctx->status.RStoveTimingLeft != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 右灶定时剩余时间变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.RStoveTimingLeft, msg->msg_buf[(*index)+1]);
                *report_en = true;
                mars_template_ctx->status.RStoveTimingLeft = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_LStoveTimingLeft:
        {
            if (mars_template_ctx->status.LStoveTimingLeft != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 左灶定时剩余时间变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.LStoveTimingLeft, msg->msg_buf[(*index)+1]);
                *report_en = true;
                mars_template_ctx->status.LStoveTimingLeft = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_HoodFireTurnOff:
        {
            LOGI("mars","%d->%d",mars_template_ctx->status.StoveClose_fire_state, msg->msg_buf[(*index)+1]);
            if(mars_template_ctx->status.StoveClose_fire_state != msg->msg_buf[(*index)+1])
            {

                if ((msg->msg_buf[(*index)+1] >= 0x00) && (msg->msg_buf[(*index)+1] <= 0x03))
                {
                    mars_template_ctx->status.StoveClose_fire_state = msg->msg_buf[(*index)+1];
                    extern uint8_t dry_fire_state;
                    if (dry_fire_state == 0x01)
                    {
                        mars_beer_control(0x00);
                        exit_dryburn_status(1, 1);
                    }

                    //灶具关火动作也进行上报，注意平台物模型配置只接受1-3，传0的时候会默认丢失，我们只需要关注是否关火，本地正常传0，否则没有清零的地方，云平台可以继续保持配置（不接受0）
                    mars_template_ctx->status.HoodFireTurnOff = msg->msg_buf[(*index)+1];
                    LOGI("mars","[%d]属性解析到的结果是:%d*(原始值),%d(结构体值)",msg->msg_buf[(*index)],msg->msg_buf[(*index)+1],mars_template_ctx->status.HoodFireTurnOff);
                    *report_en = true;
                }
                else
                {
                    LOGI("mars","解析属性0x%02X：上报内容越界",msg->msg_buf[(*index)]);
                }
            }

            if(msg->msg_buf[(*index)+1] == 0x01 || msg->msg_buf[(*index)+1] == 0x03)
            {
                //电控主动关火，通知烹饪助算法
                set_fire_status(INPUT_RIGHT, 0);
                LOGI("mars","灶具主动关火，防干烧不再进行时间兜底计算，stove close stop dryburn time");
            }

            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_RStoveSwitch:
        {
            if (mars_template_ctx->status.RStoveSwitch != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 右灶通断阀变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.RStoveSwitch, msg->msg_buf[(*index)+1]);
                *report_en = true;
                mars_template_ctx->status.RStoveSwitch = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_HoodSpeedKey:
        {
            if (msg->msg_buf[(*index)+1] == 0x01)
            {
                if (g_user_cook_assist.ZnpySwitch == 1)
                {
                    g_user_cook_assist.ZnpyOneExit = 1;  //处于单次退出
                    set_smart_smoke_switch(0);
                    is_asssist_change = true;
                    LOGI("mars", "智能排烟: 由于用户操作烟机,处于单次退出状态");
                }
            }
            (*index)+=1;
            break;
        }

        case prop_HoodSpeed:
        {
            recv_ecb_gear(msg->msg_buf[(*index)+1]);
            if (mars_template_ctx->status.HoodSpeed != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 烟机档位变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.HoodSpeed, msg->msg_buf[(*index)+1]);
                *report_en = true;
                mars_template_ctx->status.HoodSpeed = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_HoodLight:
        {
            if (mars_template_ctx->status.HoodLight != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 烟机照明变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.HoodLight, msg->msg_buf[(*index)+1]);
                *report_en = true;
                mars_template_ctx->status.HoodLight = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_HoodOffLeftTime:
        {
            if (mars_template_ctx->status.HoodOffLeftTime != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 烟机延时关闭剩余时间变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.HoodOffLeftTime, msg->msg_buf[(*index)+1]);
                *report_en = true;
                mars_template_ctx->status.HoodOffLeftTime = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_HoodOffTimer:
        {
            if (mars_template_ctx->status.HoodOffTimer != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 烟机延时关闭设定时间(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.HoodOffTimer, msg->msg_buf[(*index)+1]);
                *report_en = true;
                mars_template_ctx->status.HoodOffTimer = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_HoodTurnOffRemind:
        {
            if (mars_template_ctx->status.HoodTurnOffRemind != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 烟灶延时关闭提醒变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.HoodTurnOffRemind, msg->msg_buf[(*index)+1]);
                *report_en = true;
                mars_template_ctx->status.HoodTurnOffRemind = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_OilBoxState:
        {
            if (mars_template_ctx->status.OilBoxState != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 集油盒状态变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.OilBoxState, msg->msg_buf[(*index)+1]);
                *report_en = true;
                mars_template_ctx->status.OilBoxState = msg->msg_buf[(*index)+1];

                if (msg->msg_buf[(*index)+1] == 0x01)
                {
                    LOGI("mars", "推送事件: 集油盒满提醒推送OilBoxPush!");
                    user_post_event_json(EVENT_LEFT_STOVE_OILBOX_REMIND);
                }
            }

            mars_template_ctx->stove_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_FsydSwitch:
        {
            // if (g_user_cook_assist.ZnpySwitch != msg->msg_buf[(*index)+1])
            // {
            //     LOGI("mars", "解析属性0x%02X: 智能排烟开关变化(%d - %d)", msg->msg_buf[(*index)], g_user_cook_assist.ZnpySwitch, msg->msg_buf[(*index)+1]);
            //     *report_en = true;
            //     is_asssist_change = true;
            //     g_user_cook_assist.ZnpySwitch = msg->msg_buf[(*index)+1];
            //     set_smart_smoke_switch(g_user_cook_assist.ZnpySwitch);
            // }

            if (g_user_cook_assist.ZnpySwitch != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 智能排烟开关变化(%d - %d)", msg->msg_buf[(*index)], g_user_cook_assist.ZnpySwitch, msg->msg_buf[(*index)+1]);
                if (g_user_cook_assist.ZnpySwitch == 1)
                {
                    if (g_user_cook_assist.ZnpyOneExit == 1)
                    {
                        LOGI("mars", "智能排烟: 当前处于单次退出状态,再次进入运行状态");
                        g_user_cook_assist.ZnpySwitch  = 1;
                        g_user_cook_assist.ZnpyOneExit = 0;
                        set_smart_smoke_switch(g_user_cook_assist.ZnpySwitch);
                    }
                    else
                    {
                        LOGI("mars", "智能排烟: 当前处于运行状态,即将关闭");
                        g_user_cook_assist.ZnpySwitch  = 0;
                        g_user_cook_assist.ZnpyOneExit = 0;
                        set_smart_smoke_switch(g_user_cook_assist.ZnpySwitch);
                    }
                }
                else
                {
                    LOGI("mars", "智能排烟: 即将开启");
                    g_user_cook_assist.ZnpySwitch  = 1;
                    g_user_cook_assist.ZnpyOneExit = 0;
                    set_smart_smoke_switch(g_user_cook_assist.ZnpySwitch);
                }

                *report_en = true;
                is_asssist_change = true;
                //g_user_cook_assist.ZnpySwitch = msg->msg_buf[(*index)+1];
                //set_smart_smoke_switch(g_user_cook_assist.ZnpySwitch);
            }

            (*index)+=1;
            break;
        }

        case prop_RAuxiliarySwitch:
        {
            if (g_user_cook_assist.RAuxiliarySwitch != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 精准控温开关变化(%d - %d)", msg->msg_buf[(*index)], g_user_cook_assist.RAuxiliarySwitch, msg->msg_buf[(*index)+1]);
                *report_en = true;
                is_asssist_change = true;
                g_user_cook_assist.RAuxiliarySwitch = msg->msg_buf[(*index)+1];
                set_temp_control_switch(g_user_cook_assist.RAuxiliarySwitch, INPUT_RIGHT);
            }

            (*index)+=1;
            break;
        }

        case prop_RAuxiliaryTemp:
        {
            int16_t tempvalue = (int16_t)(msg->msg_buf[(*index)+1] << 8) | (int16_t)msg->msg_buf[(*index)+2];
            if (g_user_cook_assist.RAuxiliaryTemp != tempvalue)
            {
                LOGI("mars", "解析属性0x%02X: 精准控温温度变化(%d - %d)", msg->msg_buf[(*index)], g_user_cook_assist.RAuxiliaryTemp, tempvalue);
                *report_en = true;
                is_asssist_change = true;
                g_user_cook_assist.RAuxiliaryTemp = tempvalue;
                g_user_cook_assist.ROilTempSwitch = 1;
                set_temp_control_target_temp(g_user_cook_assist.RAuxiliaryTemp, INPUT_RIGHT);
            }

            (*index)+=2;
            break;
        }

        case prop_RMovePotLowHeatSwitch:
        {
            if (g_user_cook_assist.RMovePotLowHeatSwitch != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 右灶移锅小火开关变化(%d - %d)", msg->msg_buf[(*index)], g_user_cook_assist.RMovePotLowHeatSwitch, msg->msg_buf[(*index)+1]);
                *report_en = true;
                is_asssist_change = true;
                g_user_cook_assist.RMovePotLowHeatSwitch = msg->msg_buf[(*index)+1];
                g_user_cook_assist.RDryFireSwitch = msg->msg_buf[(*index)+1];
                set_pan_fire_switch(g_user_cook_assist.RMovePotLowHeatSwitch, INPUT_RIGHT);
                set_dry_fire_switch(g_user_cook_assist.RDryFireSwitch, 1, g_user_cook_assist.RDryFireUserType);
            }

            (*index)+=1;
            break;
        }

        case prop_RONEKEYRECIPE:
        {
            //第一字节表示开关，
            uint8_t  recipe_act  = msg->msg_buf[(*index)+1];

            //第二字节，0未操作，1煮鸡蛋，2煮饺子，3焯肉
            uint8_t  recipe_typ  = msg->msg_buf[(*index)+2];

            //一键烹饪设置的时间
            uint16_t recipe_time = msg->msg_buf[(*index)+3];

            //if ((g_user_cook_assist.ROneKeyRecipeSwitch != recipe_act) || (g_user_cook_assist.ROneKeyRecipeType != recipe_typ) || (g_user_cook_assist.ROneKeyRecipeTimeSet != recipe_time))
            {
                LOGI("mars", "解析属性0x%02X: 右灶一键菜谱变化(%d %d %d - %d %d %d)", msg->msg_buf[(*index)], g_user_cook_assist.RMovePotLowHeatSwitch, g_user_cook_assist.ROneKeyRecipeType, g_user_cook_assist.ROneKeyRecipeTimeSet, recipe_act, recipe_typ, recipe_time);
                *report_en = true;
                is_asssist_change = true;
                g_user_cook_assist.ROneKeyRecipeSwitch   = recipe_act;
                g_user_cook_assist.ROneKeyRecipeType     = recipe_typ;
                g_user_cook_assist.ROneKeyRecipeTimeSet  = recipe_time;
                g_user_cook_assist.ROneKeyRecipeTimeLeft = recipe_time;
                g_user_cook_assist.ROilTempSwitch        = 1;
                int result = quick_cooking_switch(g_user_cook_assist.ROneKeyRecipeSwitch, g_user_cook_assist.ROneKeyRecipeType, g_user_cook_assist.ROneKeyRecipeTimeSet*60, 15*60); //g_user_cook_assist.ROneKeyRecipeTimeSet*60
                if (result == 0)
                {
                    if (g_user_cook_assist.ROneKeyRecipeSwitch == 0)
                    {
                        aos_kv_del(ONE_KEY_RECIPE_KV);
                        LOGI("mars", "清除一键菜谱标志(本地取消)");
                    }
                    else
                    {
                        aos_kv_del(ONE_KEY_RECIPE_KV);
                        aos_msleep(100);
                        unsigned char flag = 1;
                        int ret = aos_kv_set(ONE_KEY_RECIPE_KV, &flag, 1, 1);
                        LOGI("mars", "置位一键菜谱标志(本地启动)");
                    }
                }
                //sync_wifi_property();
            }

            (*index)+=3;
            break;
        }

        case prop_AuxCook:
        {
            uint8_t AuxCookSwitch = msg->msg_buf[(*index)+1];
            uint8_t AuxCookMode = msg->msg_buf[(*index)+2];
            uint8_t AuxTime = msg->msg_buf[(*index)+3];
            uint8_t AuxTemp = msg->msg_buf[(*index)+4];

            if (mars_template_ctx->status.AuxCookSwitch != AuxCookSwitch || mars_template_ctx->status.AuxCookMode != AuxCookMode || \
            mars_template_ctx->status.AuxCookSetTime != AuxTime || mars_template_ctx->status.AuxCookSetTemp != AuxTemp)
            {
                LOGI("mars", "解析属性0x%02X: 辅助烹饪发生变化(%d %d %d %d -> %d %d %d %d)", msg->msg_buf[(*index)], mars_template_ctx->status.AuxCookSwitch, mars_template_ctx->status.AuxCookMode, \
                mars_template_ctx->status.AuxCookSetTime, mars_template_ctx->status.AuxCookSetTemp, AuxCookSwitch, AuxCookMode, AuxTime, AuxTemp);

                if(mars_template_ctx->status.AuxCookSwitch == 1 && AuxCookSwitch == 0)
                {
                    LOGI("mars","电控端主动退出一键烹饪模式");
                    char buf_setmsg[8] = {0};
                    int buf_len = 0;
                    buf_setmsg[buf_len++] = prop_AuxCookStatus;
                    buf_setmsg[buf_len++] = 0x02;           //设备端主动退出的目前属于异常退出
                    LOGI("mars","一键烹饪模式异常退出");
                    Mars_uartmsg_send(cmd_store,uart_get_seq_mid(),buf_setmsg,buf_len,3);

                }

                //设置为炖模式，但是没有设置炖的时间，错误设定
                if(AuxCookMode == 0x02 && AuxTime == 0x00)
                {
                    LOGI("mars","Cook Assistant Error!设置为炖模式，但是没有设置炖的时间");
                }
                //设置为炸模式，但是没有设置炸的温度，错误设定
                else if(AuxCookMode == 0x04 && AuxTemp == 0x00)
                {
                    LOGI("mars","Cook Assistant Error!设置为炸模式，但是没有设置炸的温度");
                }
                //正常设定
                else
                {
                    mars_template_ctx->status.AuxCookSwitch = AuxCookSwitch;
                    mars_template_ctx->status.AuxCookMode =AuxCookMode;
                    mars_template_ctx->status.AuxCookSetTime = AuxTime;
                    mars_template_ctx->status.AuxCookSetTemp = AuxTemp;
                    if (AuxCookSwitch > 0)
                    {
                        char voice_buff[64] = {0x00};
                        snprintf(voice_buff, sizeof(voice_buff), "%s启动", AuxMode[mars_template_ctx->status.AuxCookMode]);
                        udp_voice_write_sync(voice_buff, strlen(voice_buff), 50);
                        LOGI("mars","%s", voice_buff);
                    }
                    else
                    {
                        udp_voice_write_sync("退出辅助烹饪", strlen("退出辅助烹饪"), 50);
                        LOG_BAI("退出辅助烹饪");
                    }

                    mars_template_ctx->status.AuxCookLeftTime = mars_template_ctx->status.AuxCookSetTime;
                    mars_template_ctx->status.AuxCookLeftTime_s = mars_template_ctx->status.AuxCookSetTime * 60;

                    if(mars_template_ctx->status.AuxCookSetTime != 0)
                    {
                        char buf_setmsg[8] = {0};
                        int buf_len = 0;
                        buf_setmsg[buf_len++] = prop_AuxCookLeftTime;
                        buf_setmsg[buf_len++] = mars_template_ctx->status.AuxCookSetTime;
                        LOGI("mars","一键烹饪模式时间设置为：%d",mars_template_ctx->status.AuxCookSetTime);

                        Mars_uartmsg_send(cmd_store,uart_get_seq_mid(),buf_setmsg,buf_len,3);
                    }
                    if(mars_template_ctx->status.AuxCookSetTemp != 0)
                    {
                        LOGI("mars","一键烹饪模式温度设置为：%d",mars_template_ctx->status.AuxCookSetTemp);
                    }

                    auxiliary_cooking_switch(mars_template_ctx->status.AuxCookSwitch,mars_template_ctx->status.AuxCookMode,mars_template_ctx->status.AuxCookSetTime,mars_template_ctx->status.AuxCookSetTemp);
                }

                //一键烹饪相关属性暂时不上报
                //*report_en = true;
                // mars_template_ctx->status.RMultiMode = msg->msg_buf[(*index)+1];
            }

            //mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=4;
            break;
        }

        //无需解析
        // case prop_AuxCookStatus:
        // {

        // }

        // case prop_AuxCookLeftTime
        // {

        // }

        case 0x2D:
        {
            (*index)+=1;
            break;
        }

        case 0x2E:
        {
            (*index)+=1;
            break;
        }

        default:
            *nak = NAK_ERROR_UNKOWN_PROCODE;
            break;
    }
}

void mars_stove_setToSlave(cJSON *root, cJSON *item, mars_template_ctx_t *mars_template_ctx, uint8_t *buf_setmsg, uint16_t *buf_len)
{
    extern bool is_ca_para;
    if (NULL == root || NULL == mars_template_ctx || NULL == buf_setmsg || NULL == buf_len)
    {
        return false;
    }

    if ((item = cJSON_GetObjectItem(root, "HoodStoveLink")) != NULL && cJSON_IsNumber(item)) {
        buf_setmsg[(*buf_len)++] = prop_HoodStoveLink;//烟灶联动
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "HoodLightLink")) != NULL && cJSON_IsNumber(item)) {
        buf_setmsg[(*buf_len)++] = prop_HoodLightLink;//烟机照明联动
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "LightStoveLink")) != NULL && cJSON_IsNumber(item)) {
        buf_setmsg[(*buf_len)++] = prop_LightStoveLink;//灯灶联动
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "RStoveTimingOpera")) != NULL && cJSON_IsNumber(item)) {
        buf_setmsg[(*buf_len)++] = prop_RStoveTimingOpera;//右灶定时动作
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "RStoveTimingSet")) != NULL && cJSON_IsNumber(item)) {
        buf_setmsg[(*buf_len)++] = prop_RStoveTimingSet;//右灶定时时间
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "LStoveTimingOpera")) != NULL && cJSON_IsNumber(item)) {
        buf_setmsg[(*buf_len)++] = prop_LStoveTimingOpera;
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "LStoveTimingSet")) != NULL && cJSON_IsNumber(item)) {
        buf_setmsg[(*buf_len)++] = prop_LStoveTimingSet;
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "HoodFireTurnOff")) != NULL && cJSON_IsNumber(item)) {
        buf_setmsg[(*buf_len)++] = prop_HoodFireTurnOff;  //灶具关火
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "RStoveSwitch")) != NULL && cJSON_IsNumber(item)) {
        buf_setmsg[(*buf_len)++] = prop_RStoveSwitch;   //右灶通断阀
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "HoodSpeed")) != NULL && cJSON_IsNumber(item)) {
        buf_setmsg[(*buf_len)++] = prop_HoodSpeed;//烟机风速
        buf_setmsg[(*buf_len)++] = item->valueint;

        if (g_user_cook_assist.ZnpySwitch == 1)
        {
            g_user_cook_assist.ZnpyOneExit = 1;  //处于单次退出
            set_smart_smoke_switch(0);
            is_ca_para = true;
            LOGI("mars", "智能排烟: 由于用户操作烟机,处于单次退出状态");
        }
    }

    if ((item = cJSON_GetObjectItem(root, "HoodLight")) != NULL && cJSON_IsNumber(item)) {
        buf_setmsg[(*buf_len)++] = prop_HoodLight;//烟机照明
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "HoodOffTimer")) != NULL && cJSON_IsNumber(item)) {
        buf_setmsg[(*buf_len)++] = prop_HoodOffTimer;
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "OilBoxState")) != NULL && cJSON_IsNumber(item)) {
        buf_setmsg[(*buf_len)++] = prop_OilBoxState;
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "SmartSmokeSwitch")) != NULL && cJSON_IsNumber(item))
    {
        LOGI("mars", "平台下发: 智能排烟开关 = %d", item->valueint);

        if (item->valueint == 0)
        {
            g_user_cook_assist.ZnpySwitch  = item->valueint;
            g_user_cook_assist.ZnpyOneExit = 0;
            set_smart_smoke_switch(g_user_cook_assist.ZnpySwitch);
        }
        else
        {
            g_user_cook_assist.ZnpySwitch  = item->valueint;
            g_user_cook_assist.ZnpyOneExit = 0;
            set_smart_smoke_switch(g_user_cook_assist.ZnpySwitch);
        }

        is_ca_para = true;
    }

    if ((item = cJSON_GetObjectItem(root, "RAuxiliarySwitch")) != NULL && cJSON_IsNumber(item))
    {
        LOGI("mars", "平台下发: 右灶控温开关 = %d", item->valueint);
        g_user_cook_assist.RAuxiliarySwitch = item->valueint;
        g_user_cook_assist.ROilTempSwitch   = 1;
        set_temp_control_switch(g_user_cook_assist.RAuxiliarySwitch, INPUT_RIGHT);
        is_ca_para = true;
    }

    if ((item = cJSON_GetObjectItem(root, "RAuxiliaryTemp")) != NULL && cJSON_IsNumber(item))
    {
        LOGI("mars", "平台下发: 右灶控温温度 = %d", item->valueint);
        g_user_cook_assist.RAuxiliaryTemp = item->valueint;
        set_temp_control_target_temp(g_user_cook_assist.RAuxiliaryTemp, INPUT_RIGHT);
        is_ca_para = true;
    }

    if ((item = cJSON_GetObjectItem(root, "RMovePotLowHeatSwitch")) != NULL && cJSON_IsNumber(item))
    {
        LOGI("mars", "平台下发: 右灶移锅小火开关 = %d", item->valueint);
        g_user_cook_assist.RMovePotLowHeatSwitch = item->valueint;
        set_pan_fire_switch(g_user_cook_assist.RMovePotLowHeatSwitch, INPUT_RIGHT);
        is_ca_para = true;
    }

    if ((item = cJSON_GetObjectItem(root, "RMovePotLowHeatOffTime")) != NULL && cJSON_IsNumber(item))
    {
        LOGI("mars", "平台下发: 右灶移锅关火时间 = %d", item->valueint);
        g_user_cook_assist.RMovePotLowHeatOffTime = item->valueint;
        set_pan_fire_delayofftime(g_user_cook_assist.RMovePotLowHeatOffTime);
        is_ca_para = true;
    }

    if ((item = cJSON_GetObjectItem(root, "OilTempSwitch")) != NULL && cJSON_IsNumber(item))
    {
        LOGI("mars", "平台下发: 油温显示开关 = %d", item->valueint);
        g_user_cook_assist.ROilTempSwitch = item->valueint;
        is_ca_para = true;
    }

    if ((item = cJSON_GetObjectItem(root, "CookingCurveSwitch")) != NULL && cJSON_IsNumber(item))
    {
        LOGI("mars", "平台下发: 温度曲线开关 = %d", item->valueint);
        g_user_cook_assist.CookingCurveSwitch = item->valueint;
        is_ca_para = true;
    }

    if ((item = cJSON_GetObjectItem(root, "ROneKeyRecipe")) != NULL && cJSON_IsObject(item))
    {
        cJSON *sub_item = NULL;
        if ( (sub_item = cJSON_GetObjectItem(item, "RecipeAction"))!= NULL && cJSON_IsNumber(sub_item))
        g_user_cook_assist.ROneKeyRecipeSwitch = sub_item->valueint;

        if ( (sub_item = cJSON_GetObjectItem(item, "RecipeType"))!= NULL && cJSON_IsNumber(sub_item))
        g_user_cook_assist.ROneKeyRecipeType = sub_item->valueint;

        if ( (sub_item = cJSON_GetObjectItem(item, "RecipeTime"))!= NULL && cJSON_IsNumber(sub_item))
        g_user_cook_assist.ROneKeyRecipeTimeSet  = sub_item->valueint;
        g_user_cook_assist.ROneKeyRecipeTimeLeft = sub_item->valueint;

        g_user_cook_assist.ROilTempSwitch = 1;

        int result = quick_cooking_switch(g_user_cook_assist.ROneKeyRecipeSwitch, g_user_cook_assist.ROneKeyRecipeType, g_user_cook_assist.ROneKeyRecipeTimeSet*60, 15*60);//g_user_cook_assist.ROneKeyRecipeTimeSet*60
        if (result == 0)
        {
            if (g_user_cook_assist.ROneKeyRecipeSwitch == 0)
            {
                aos_kv_del(ONE_KEY_RECIPE_KV);
                LOGI("mars", "清除一键菜谱标志(远程取消)");
            }
            else
            {
                aos_kv_del(ONE_KEY_RECIPE_KV);
                aos_msleep(100);
                unsigned char flag = 1;
                int ret = aos_kv_set(ONE_KEY_RECIPE_KV, &flag, 1, 1);
                LOGI("mars", "置位一键菜谱标志(远程启动)");
            }
        }
        is_ca_para = true;
    }

    if ((item = cJSON_GetObjectItem(root, "RDryFireSwitch")) != NULL && cJSON_IsNumber(item))
    {
        LOGI("mars", "平台下发: 防干烧开关 = %d", item->valueint);
        g_user_cook_assist.RDryFireSwitch = item->valueint;
        is_ca_para = true;
        set_dry_fire_switch(g_user_cook_assist.RDryFireSwitch, 1, g_user_cook_assist.RDryFireUserType);
    }

    if ((item = cJSON_GetObjectItem(root, "RDryFireTempCurveSwitch")) != NULL && cJSON_IsNumber(item))
    {
        LOGI("mars", "平台下发: 右灶防干烧温度曲线开关 = %d", item->valueint);
        g_user_cook_assist.RDryFireTempCurveSwitch = item->valueint;
        is_ca_para = true;
    }

    if ((item = cJSON_GetObjectItem(root, "DryFireUserSwitch")) != NULL && cJSON_IsNumber(item))
    {
        LOGI("mars", "平台下发: 防干烧用户切换开关 = %d", item->valueint);
        g_user_cook_assist.RDryFireUserType = item->valueint;
        is_ca_para = true;
        set_dry_fire_switch(g_user_cook_assist.RDryFireSwitch, 1, g_user_cook_assist.RDryFireUserType);
    }

    if ((item = cJSON_GetObjectItem(root, "MutivalveGear")) != NULL && cJSON_IsNumber(item)) {
        LOGI("mars", "平台下发: 八段阀火力切换 = %d", item->valueint);
        buf_setmsg[(*buf_len)++] = prop_MultiValveGear;                  //八段阀
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

}

void mars_ca_handle()
{
    mars_ca_para_save();
    report_wifi_property(NULL, NULL);
    sync_wifi_property();
}

void mars_stove_changeReport(cJSON *proot, mars_template_ctx_t *mars_template_ctx)
{
    for (uint8_t index=prop_LStoveStatus; index<=prop_OilBoxState; ++index)
    {
        if (mars_template_ctx->stove_reportflg & VALID_BIT(index))
        {
            switch (index)
            {
                case prop_LStoveStatus:
                {
                    //LOGI("mars", "mars_stove_changeReport: LStoveStatus");
                    cJSON_AddNumberToObject(proot, "LStoveStatus", mars_template_ctx->status.LStoveStatus);
                    break;
                }
                case prop_RStoveStatus:
                {
                    //LOGI("mars", "mars_stove_changeReport: RStoveStatus");
                    cJSON_AddNumberToObject(proot, "RStoveStatus", mars_template_ctx->status.RStoveStatus);
                    break;
                }
                case prop_HoodStoveLink:
                {
                    //LOGI("mars", "mars_stove_changeReport: HoodStoveLink");
                    cJSON_AddNumberToObject(proot, "HoodStoveLink", mars_template_ctx->status.HoodStoveLink);
                    break;
                }
                case prop_HoodLightLink:
                {
                    //LOGI("mars", "mars_stove_changeReport: HoodLightLink");
                    cJSON_AddNumberToObject(proot, "HoodLightLink", mars_template_ctx->status.HoodLightLink);
                    break;
                }
                case prop_LightStoveLink:
                {
                    cJSON_AddNumberToObject(proot, "LightStoveLink", mars_template_ctx->status.LightStoveLink);
                    break;
                }
                case prop_RStoveTimingState:
                {
                    //LOGI("mars", "mars_stove_changeReport: RStoveTimingState");
                    cJSON_AddNumberToObject(proot, "RStoveTimingState", mars_template_ctx->status.RStoveTimingState);
                    break;
                }
                case prop_LStoveTimingState:
                {
                    //LOGI("mars", "mars_stove_changeReport: RStoveTimingState");
                    cJSON_AddNumberToObject(proot, "LStoveTimingState", mars_template_ctx->status.LStoveTimingState);
                    break;
                }
                // case prop_RStoveTimingOpera:
                // {
                //     cJSON_AddNumberToObject(proot, "RStoveTimingOpera", mars_template_ctx->status.RStoveTimingOpera);
                //     break;
                // }
                case prop_RStoveTimingSet:
                {
                    //LOGI("mars", "mars_stove_changeReport: RStoveTimingSet");
                    cJSON_AddNumberToObject(proot, "RStoveTimingSet", mars_template_ctx->status.RStoveTimingSet);
                    break;
                }
                case prop_LStoveTimingSet:
                {
                    //LOGI("mars", "mars_stove_changeReport: RStoveTimingSet");
                    cJSON_AddNumberToObject(proot, "LStoveTimingSet", mars_template_ctx->status.LStoveTimingSet);
                    break;
                }
                case prop_RStoveTimingLeft:
                {
                    //LOGI("mars", "mars_stove_changeReport: RStoveTimingLeft");
                    cJSON_AddNumberToObject(proot, "RStoveTimingLeft", mars_template_ctx->status.RStoveTimingLeft);
                    break;
                }
                case prop_LStoveTimingLeft:
                {
                    //LOGI("mars", "mars_stove_changeReport: RStoveTimingLeft");
                    cJSON_AddNumberToObject(proot, "LStoveTimingLeft", mars_template_ctx->status.LStoveTimingLeft);
                    break;
                }
                case prop_HoodFireTurnOff:
                {
                    //LOGI("mars", "mars_stove_changeReport: HoodFireTurnOff");
                    cJSON_AddNumberToObject(proot, "HoodFireTurnOff", mars_template_ctx->status.HoodFireTurnOff);
                    break;
                }
                case prop_LThermocoupleState:
                {
                    //LOGI("mars", "mars_stove_changeReport: LThermocoupleState");
                    cJSON_AddNumberToObject(proot, "LThermocoupleState", mars_template_ctx->status.LThermocoupleState);
                    break;
                }
                case prop_RThermocoupleState:
                {
                    //LOGI("mars", "mars_stove_changeReport: RThermocoupleState");
                    cJSON_AddNumberToObject(proot, "RThermocoupleState", mars_template_ctx->status.RThermocoupleState);
                    break;
                }
                case prop_RStoveSwitch:
                {
                    //LOGI("mars", "mars_stove_changeReport: RStoveSwitch");
                    cJSON_AddNumberToObject(proot, "RStoveSwitch", mars_template_ctx->status.RStoveSwitch);
                    break;
                }
                // case prop_RAuxiliarySwitch:
                // {
                //     cJSON_AddNumberToObject(proot, "RAuxiliarySwitch", mars_template_ctx->status.RAuxiliarySwitch);
                //     break;
                // }
                // case prop_RAuxiliaryTemp:
                // {
                //     cJSON_AddNumberToObject(proot, "RAuxiliaryTemp", mars_template_ctx->status.RAuxiliaryTemp);
                //     break;
                // }
                // case prop_ROilTemp:                  //使用http上报
                // {
                //     cJSON_AddNumberToObject(proot, "ROilTemp", mars_template_ctx->status.ROilTemp);
                //     break;
                // }
                case prop_HoodSpeed:
                {
                    //LOGI("mars", "mars_stove_changeReport: HoodSpeed");
                    cJSON_AddNumberToObject(proot, "HoodSpeed", mars_template_ctx->status.HoodSpeed);
                    break;
                }
                case prop_HoodLight:
                {
                    //LOGI("mars", "mars_stove_changeReport: HoodLight");
                    cJSON_AddNumberToObject(proot, "HoodLight", mars_template_ctx->status.HoodLight);
                    break;
                }
                case prop_HoodOffLeftTime:
                {
                    //LOGI("mars", "mars_stove_changeReport: HoodOffLeftTime");
                    cJSON_AddNumberToObject(proot, "HoodOffLeftTime", mars_template_ctx->status.HoodOffLeftTime);
                    break;
                }
                // case prop_FsydSwitch:
                // {
                //     cJSON_AddNumberToObject(proot, "SmartSmokeSwitch", mars_template_ctx->status.FsydSwitch);
                //     break;
                // }
                // case prop_HoodSteplessSpeed:
                // {
                //     cJSON_AddNumberToObject(proot, "HoodSteplessSpeed", mars_template_ctx->status.HoodSteplessSpeed);
                //     break;
                // }
                case prop_HoodOffTimer:
                {
                    //LOGI("mars", "mars_stove_changeReport: HoodOffTimer");
                    cJSON_AddNumberToObject(proot, "HoodOffTimer", mars_template_ctx->status.HoodOffTimer);
                    break;
                }
                case prop_HoodTurnOffRemind:
                {
                    //LOGI("mars", "mars_stove_changeReport: HoodTurnOffRemind");
                    cJSON_AddNumberToObject(proot, "HoodTurnOffRemind", mars_template_ctx->status.HoodTurnOffRemind);
                    break;
                }
                case prop_OilBoxState:
                {
                    //LOGI("mars", "mars_stove_changeReport: OilBoxState");
                    cJSON_AddNumberToObject(proot, "OilBoxState", mars_template_ctx->status.OilBoxState);
                    break;
                }
                // case prop_RMovePotLowHeatSwitch:
                // {
                //     cJSON_AddNumberToObject(proot, "RMovePotLowHeatSwitch", mars_template_ctx->status.RMovePotLowHeatSwitch);
                //     break;
                // }
                // case prop_OilTempSwitch:
                // {
                //     cJSON_AddNumberToObject(proot, "OilTempSwitch", mars_template_ctx->status.OilTempSwitch);
                // }
                default:
                {
                    //LOGI("mars", "mars_stove_changeReport: default");
                }
            }
        }
    }

    mars_template_ctx->stove_reportflg = 0;
}


void cook_temp_send_display(int16_t temp)
{
    uint8_t  send_buf[3] = {0};
    uint16_t buf_len = 0;

    send_buf[buf_len++] = prop_ROilTemp;
    send_buf[buf_len++] = temp/0x100;
    send_buf[buf_len++] = temp%0x100;
    Mars_uartmsg_send(cmd_store, 0, send_buf, buf_len, 1);  //uart_get_seq_head()
}

int16_t IRT102mTempRange(int16_t t)
{
    if (t < 0)
        return 0;
    else if (t > 400)
        return 400;
    else
        return t;
}
// ring_buffer_t ring_buffer_irt_data;
// typedef struct {
//     int16_t ROilTemp;                   //右灶油温
//     int16_t REnvTemp;                   //右灶环境温度(NTC)
// }irt_data_t;
void IRT102mCallBack(SENSOR_STATUS_ENUM status, float TargetTemper1, float EnvirTemper1, float TargetTemper2, float EnvirTemper2)
{
    static uint64_t log_time = 0;
    bool log_flag = false;

    // static uint64_t time = 0;
    // if (time == 0 || (aos_now_ms() - time) >= 5000)
    // {
    //     ktask_t *task = krhino_cur_task_get();
    //     uint64_t min_free = 0;
    //     krhino_task_stack_min_free(task, &min_free);
    //     uint64_t cur_free = 0;
    //     //krhino_task_stack_cur_free(task, &cur_free);
    //     printf("IRT102mCallBack: name=%s prio=%d task_stack=%ld\r\n", task->task_name, task->prio, task->stack_size);
    //     printf("IRT102mCallBack: min_free=%lu cur_free=%lu \r\n", min_free, cur_free);
    //     krhino_backtrace_now();
    //     //krhino_backtrace_task(task->task_name);
    //     time = aos_now_ms();

    // }


    if (aos_now_ms() - log_time >= 30*1000)
    {
        log_flag = true;
        log_time = aos_now_ms();
    }
    if ((TargetTemper2 >= 0 && TargetTemper2 < 0.001) || (EnvirTemper2 >= 0 && EnvirTemper2 < 0.001))
    {
        return 0;
    }

    if (log_flag) LOGI("mars", "0 IRT102mCallBack: status(%d), t_temper1(%f), e_temper1(%f), t_temper2(%f), e_temper2(%f)",  status, TargetTemper1, EnvirTemper1, TargetTemper2, EnvirTemper2);

	float TargetTemperFit[2];   //拟合温度
	float EnvirTemperFit[2];	//拟合温度
	float TargetTemperComp[2];  //补偿温度
	float EnvirTemperComp[2];   //补偿温度


    g_sGas_Sensor.siNTC_Temp1 = EnvirTemper1*10;
	g_sGas_Sensor.siNTC_Temp2 = EnvirTemper2*10;
    TargetTemperComp[0] = Sensor_AmbTemp_Adj_Proc(1, TargetTemper1 * 10.0);  // 麦乐克库1.4.6版本：先执行环温补偿函数，再执行拟合函数
	TargetTemperComp[1] = Sensor_AmbTemp_Adj_Proc(2, TargetTemper2 * 10.0);
    if (log_flag) LOGI("mars", "1 Sensor_AmbTemp_Adj_Proc处理: %f, %f", TargetTemperComp[0], TargetTemperComp[1]);

    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
	TargetTemperFit[0] = Sensor_Get_Obj_Temp_Adj_Proc(1 , TargetTemperComp[0]) / 10.0;
	TargetTemperFit[1] = Sensor_Get_Obj_Temp_Adj_Proc(2 , TargetTemperComp[1]) / 10.0;
    if (log_flag) LOGI("mars", "2 Sensor_Get_Obj_Temp_Adj_Proc处理: %f, %f", TargetTemperFit[0], TargetTemperFit[1]);

    mars_template_ctx->status.LOilTemp = IRT102mTempRange((int16_t)TargetTemperFit[0]);
    mars_template_ctx->status.LEnvTemp = IRT102mTempRange((int16_t)EnvirTemper1);
    mars_template_ctx->status.ROilTemp = IRT102mTempRange((int16_t)TargetTemperFit[1]);
    mars_template_ctx->status.REnvTemp = IRT102mTempRange((int16_t)EnvirTemper2);

	// LOGI("mars", "int16_t转换赋值后: L_Temp(%d), L_env(%d), R_Temp(%d), R_env(%d)",
    // mars_template_ctx->status.LOilTemp,
    // mars_template_ctx->status.LEnvTemp,
    // mars_template_ctx->status.ROilTemp,
    // mars_template_ctx->status.REnvTemp);
    // irt_data_t data = {
    //     .ROilTemp = mars_template_ctx->status.ROilTemp,
    //     .REnvTemp = mars_template_ctx->status.REnvTemp
    //     };
    // if (ring_buffer_is_full(&ring_buffer_irt_data))
    // {
    //     LOGE("mars", "温度数据队列已满,不再添加新数据!!!!!! %d", ring_buffer_valid_size(&ring_buffer_irt_data));
    //     return;
    // }

    // ring_buffer_push(&ring_buffer_irt_data, &data);
    // LOGI("mars", "ring_buffer_irt_data: 添加数据(%d)完成,当前数据量=%d", mars_template_ctx->status.ROilTemp, ring_buffer_valid_size(&ring_buffer_irt_data));

    cook_assistant_input(INPUT_RIGHT, (unsigned short)mars_template_ctx->status.ROilTemp * 10,(unsigned short)mars_template_ctx->status.REnvTemp * 10);
    aux_assistant_input(INPUT_RIGHT,(unsigned short)mars_template_ctx->status.ROilTemp * 10,(unsigned short)mars_template_ctx->status.REnvTemp * 10);
    // cook_assistant_input(INPUT_RIGHT, (unsigned short)TargetTemper1 * 10, (unsigned short)EnvirTemper1 * 10);

    prepare_gear_change_task();

    m_tempreport_pre(mars_template_ctx->status.ROilTemp, mars_template_ctx);
    m_tempreport_fgs(mars_template_ctx->status.ROilTemp, mars_template_ctx);
}
#if 0
static void cook_assistant_thread(void *argv)
{
    ring_buffer_init(&ring_buffer_irt_data, 16, sizeof(irt_data_t));

    while(true)
    {
        if (ring_buffer_valid_size(&ring_buffer_irt_data) > 0)
        {
            irt_data_t data = {0x00};
            if (ring_buffer_read(&ring_buffer_irt_data, &data, 1) == 1)
            {
                cook_assistant_input(INPUT_RIGHT, (unsigned short)(data.ROilTemp) * 10, (unsigned short)(data.REnvTemp) * 10);
                prepare_gear_change_task();
                m_tempreport_pre(data.ROilTemp, mars_dm_get_ctx());
                m_tempreport_fgs(data.ROilTemp, mars_dm_get_ctx());
            }
        }
        aos_msleep(100);
    }
}

static void init_cook_assistant_thread()
{
    static aos_task_t task_cook_assistant;
    int ret = aos_task_new_ext(&task_cook_assistant, "task_cook_assistant", cook_assistant_thread, NULL, 8192, AOS_DEFAULT_APP_PRI + 3);
    if (ret != 0)
        LOGE("mars", "aos_task_new_ext: failed!!! (烹饪助手线程)");
}
#endif
int mars_irtInit(void)
{
    //init_cook_assistant_thread();
    LOGI("mars", "I²C红外测温模组固件版本: ");
    Lib_VersionInfoPrintf();
    int info_len = sizeof(SENSOR_PARA_S);
    int ret = aos_kv_get("irt_info", (void *)&g_sGas_Sensor_ParaS, &info_len);
    if (ret == 0)
    {
        LOGI("mars", "拟合曲线参数:通道1: fCh1ObjTemp100=%f, Ch1_A1=%f Ch1_B1=%f Ch1_A2=%f Ch1_B2=%f",
		g_sGas_Sensor_ParaS.fCh1ObjTemp100, g_sGas_Sensor_ParaS.fAdjCh1_Line_A1, g_sGas_Sensor_ParaS.fAdjCh1_Line_B1, g_sGas_Sensor_ParaS.fAdjCh1_Line_A2, g_sGas_Sensor_ParaS.fAdjCh1_Line_B2);
        LOGI("mars", "拟合曲线参数:通道2: fCh2ObjTemp100=%f, Ch2_A1=%f Ch2_B1=%f Ch2_A2=%f Ch2_B2=%f",
		g_sGas_Sensor_ParaS.fCh2ObjTemp100, g_sGas_Sensor_ParaS.fAdjCh2_Line_A1, g_sGas_Sensor_ParaS.fAdjCh2_Line_B1, g_sGas_Sensor_ParaS.fAdjCh2_Line_A2, g_sGas_Sensor_ParaS.fAdjCh2_Line_B2);
        LOGI("mars", "校准标志位: Ch1Flag=%d Ch2Flag=%d",
        g_sGas_Sensor_ParaS.AdjCh1ParaFlag, g_sGas_Sensor_ParaS.AdjCh2ParaFlag);
    }
    else
    {
        LOGE("mars", "error: I²C红外测温模组未标定!!!!!!");
    }

    Driver_IRT102mInit(250, IRT102mCallBack);
}

void mars_sensor_uartMsgFromSlave(uartmsg_que_t *msg,
                                mars_template_ctx_t *mars_template_ctx,
                                uint16_t *index, bool *report_en, uint8_t *nak)
{
    switch (msg->msg_buf[(*index)])
    {
        case prop_RadarGear:
        {
            if (mars_template_ctx->status.RadarGear != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: Radar_changed雷达档位变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.RadarGear, msg->msg_buf[(*index)+1]);
                //*report_en = true;
                mars_template_ctx->status.RadarGear = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->sensor_reportflg |= SVALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }
        case prop_RadarSign:
        {
            //没有定时器任务存在，且雷达信号为1，则打开照明，创建一个2s的定时器
            // if(light_close_timer.hdl == NULL && msg->msg_buf[(*index)+1] == 1)
            // {
            //     char buf_setmsg[8] = {0};
            //     uint8_t len = 0;
            //     buf_setmsg[len++] = prop_HoodLight;                    //临时根据雷达信号设置烟机照明方便测试
            //     buf_setmsg[len++] = 1;
            //     Mars_uartmsg_send(cmd_set, uart_get_seq_mid(), &buf_setmsg, sizeof(buf_setmsg), 3);

            //     aos_timer_new_ext(&light_close_timer, timer_func_close_light, NULL, 2 * 1000, 0, 1);
            // }

            if (mars_template_ctx->status.RadarSign != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: Radar_changed雷达信号变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.RadarSign, msg->msg_buf[(*index)+1]);
                //*report_en = true;
                mars_template_ctx->status.RadarSign = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->sensor_reportflg |= SVALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }
        case prop_MultiValveGear:
        {
            if (mars_template_ctx->status.MultiValveGear != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 八段阀火力发生变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.MultiValveGear, msg->msg_buf[(*index)+1]);
                //*report_en = true;
                mars_template_ctx->status.MultiValveGear = msg->msg_buf[(*index)+1];

                //通知辅助烹饪当前八段阀档位
                set_multivalve_gear(mars_template_ctx->status.MultiValveGear, 1);
            }

            LOGW("mars", "**************** 当前火力档位: %d ************************************************", msg->msg_buf[(*index)+1]);
            LOGW("mars", "**************** 当前火力档位: %d ************************************************", msg->msg_buf[(*index)+1]);
            LOGW("mars", "**************** 当前火力档位: %d ************************************************", msg->msg_buf[(*index)+1]);
            (*index)+=1;
            break;
        }
        case prop_MultiVaveStatus:
        {
            if (mars_template_ctx->status.MultiVaveStatus != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 八段阀处于最大挡位置变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.MultiVaveStatus, msg->msg_buf[(*index)+1]);
                //*report_en = true;
                mars_template_ctx->status.MultiVaveStatus = msg->msg_buf[(*index)+1];

                if (mars_template_ctx->status.MultiVaveStatus == 0)
                {
                    exit_aux_func(1);
                }
            }

            mars_template_ctx->sensor_reportflg |= SVALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        default:
            *nak = NAK_ERROR_UNKOWN_PROCODE;
            break;
    }
    return;
}

void mars_sensor_changeReport(cJSON *proot, mars_template_ctx_t *mars_template_ctx)
{
    for (uint8_t index=prop_RadarGear; index<=prop_MultiVaveStatus; ++index)
    {
        if (mars_template_ctx->sensor_reportflg & SVALID_BIT(index))
        {
            switch(index)
            {
                case prop_MultiValveGear:
                {
                    cJSON_AddNumberToObject(proot, "MutivalveGear", mars_template_ctx->status.MultiValveGear);
                    break;
                }

                default:
                {
                    LOGI("mars","now not support this prop report[0x%02X]",index);
                }

            }
        }
    }

    mars_template_ctx->sensor_reportflg = 0;
}

#endif
