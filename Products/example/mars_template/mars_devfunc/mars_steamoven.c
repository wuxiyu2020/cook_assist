/*
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-08-17 16:58:28
 * @LastEditors: Zhouxc
 * @LastEditTime: 2024-05-20 14:53:43
 * @FilePath     : /tg7100c/Products/example/mars_template/mars_devfunc/mars_steamoven.c
 */

#include <string.h>
#include "mars_steamoven.h"
#include "mars_httpc.h"
#include "../mars_driver/mars_cloud.h"

#define         VALID_BIT(n)            ((uint64_t)1 << (n - prop_LStOvOperation))

#if MARS_STEAMOVEN


static uint8_t report_cookhistory = 0;

#if 1
//cook history post
#define STR_COOKHISTORYPOST  "{\"deviceMac\":\"%s\",\"iotId\":\"%s\",\"menuId\":%ld,\"cavity\":%d,\"recipeType\":%d,\"number\":%d,\"modelType\":%d,\"token\":\"%s\",\"cookModelDTOList\":[%s]}"
#define STR_COOKHISTORYWITHOUTMENUPOST  "{\"deviceMac\":\"%s\",\"iotId\":\"%s\",\"cavity\":%d,\"number\":%d,\"modelType\":%d,\"token\":\"%s\",\"cookModelDTOList\":[%s]}"
#define STR_NORMAL     "{\"model\":%d,\"temp\":%d,\"time\":%d,\"Index\":%d},"
#define STR_NENGKAO    "{\"model\":%d,\"temp\":%d,\"time\":%d,\"LSteamGear\":%d,\"Index\":%d},"
#define STR_MICROWAVE  "{\"model\":%d,\"time\":%d,\"microwaveGear\":%d,\"Index\":%d},"

void mars_cookhistory_logpost(int cavity)
{
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    int is_multi = 0;   //非多段
    int modelType = 3;
    char modelList[256] = {0};
    uint32_t LCookbookID = mars_template_ctx->status.LCookbookID;
    uint8_t recipeType = mars_template_ctx->status.LMultiMode==1?6:0;

    if (0 == cavity) //左腔
    {   
        if(0x01 == mars_template_ctx->status.LMultiMode || 0x02 == mars_template_ctx->status.LMultiMode) //多段
        {            
            is_multi  = 1;
            modelType = 2;      //多段默认为蒸烤
            for (int index=0; index<mars_template_ctx->status.LMultiStageState.stage_sum; ++index)
            {
                char tmp_str[128] = {0};
                if (mars_template_ctx->status.LMultiStageContent.stage[index].workmode == 40)
                {
                    if (memcmp(get_product_name(), AD_NAME_1, sizeof(AD_NAME_1)) == 0)
                    {
                        sprintf(tmp_str, STR_NENGKAO, \
                            mars_template_ctx->status.LMultiStageContent.stage[index].workmode, \
                            mars_template_ctx->status.LMultiStageContent.stage[index].set_temper, \
                            mars_template_ctx->status.LMultiStageContent.stage[index].set_time, \
                            mars_template_ctx->status.LMultiStageContent.stage[index].SteamGear, \
                            index+1);
                    }
                    else
                    {
                        sprintf(tmp_str, STR_NORMAL, \
                            mars_template_ctx->status.LMultiStageContent.stage[index].workmode, \
                            mars_template_ctx->status.LMultiStageContent.stage[index].set_temper, \
                            mars_template_ctx->status.LMultiStageContent.stage[index].set_time, \
                            index+1);
                    }
                }
                else
                {
                    sprintf(tmp_str, STR_NORMAL, \
                        mars_template_ctx->status.LMultiStageContent.stage[index].workmode, \
                        mars_template_ctx->status.LMultiStageContent.stage[index].set_temper, \
                        mars_template_ctx->status.LMultiStageContent.stage[index].set_time, \
                        index+1);
                }

                strcat(modelList, tmp_str);
            }
        }
        else
        {
            if (mars_template_ctx->status.LStOvMode == 48)
            {
                sprintf(modelList, STR_MICROWAVE, mars_template_ctx->status.LStOvMode, 
                                                  mars_template_ctx->status.LStOvSetTimer, 
                                                  mars_template_ctx->status.LMicroGear, 1);
            }
            else if (mars_template_ctx->status.LStOvMode == 40)
            {
                if (memcmp(get_product_name(), AD_NAME_1, sizeof(AD_NAME_1)) == 0)
                {
                    sprintf(modelList, STR_NENGKAO,   mars_template_ctx->status.LStOvMode, 
                                                      mars_template_ctx->status.LStOvSetTemp, 
                                                      mars_template_ctx->status.LStOvSetTimer, 
                                                      mars_template_ctx->status.LSteamGear, 1);
                }
                else
                {
                    sprintf(modelList, STR_NORMAL, \
                        mars_template_ctx->status.LStOvMode,
                        mars_template_ctx->status.LStOvSetTemp,
                        mars_template_ctx->status.LStOvSetTimer, 1);
                }
            }
            else 
            {            
                sprintf(modelList, STR_NORMAL, mars_template_ctx->status.LStOvMode, 
                                               mars_template_ctx->status.LStOvSetTemp, 
                                               mars_template_ctx->status.LStOvSetTimer, 1);
            }
            switch (mars_template_ctx->status.LStOvMode)
            {
                case 0x01:
                case 0x04:
                case 0x05:
                case 100:
                    modelType = 0;  //蒸模式
                    break;
                case 0x23:
                case 0x24:
                case 0x26:
                case 0x28:
                case 0x2A:
                    modelType = 1;  //烤模式
                    break;
                case 0x30:
                    modelType = 3;  //烤模式
                    break;
                default:
                    break;
            }
        }
    }
    else
    {
        LCookbookID = 0;
        recipeType  = 0;
        // 右腔没有多段
        sprintf(modelList, STR_NORMAL, \
            mars_template_ctx->status.RStOvMode, \
            mars_template_ctx->status.RStOvSetTemp, \
            mars_template_ctx->status.RStOvSetTimer, \
            1);
        switch (mars_template_ctx->status.RStOvMode)
        {
            case 0x01:
            case 0x04:
            case 0x05:
            case 100:
                modelType = 0;  //蒸模式
                break;
            case 0x23:
            case 0x24:
            case 0x26:
            case 0x28:
            case 0x2A:
                modelType = 1;  //烤模式
            default:
                break;
        }
    }

    modelList[strlen(modelList)-1] = 0;       //去掉逗号
    //uint8_t recipeType = mars_template_ctx->status.LMultiMode==1?6:0;
    http_msg_t cook_http = {0};
    char* cookHistory_msg = (char *)aos_malloc(512);
    //LOGI("mars", "mars_cookhistory_logpost: aos_malloc = 0x%08X", cookHistory_msg);
    if (cookHistory_msg){
        sprintf(cookHistory_msg,
            STR_COOKHISTORYPOST,
            mars_template_ctx->macStr,
            mars_template_ctx->device_name,
            LCookbookID, //mars_template_ctx->status.LCookbookID,
            cavity,
            recipeType,             //非菜谱
            is_multi,               //是否多段
            modelType,              //蒸烤类型
            mars_cloud_tokenget(),
            modelList);

        LOGI("mars", "烹饪历史http上报: %s", cookHistory_msg);
        cook_http.http_method = HTTP_COOKHISTORY;
        cook_http.msg_str = cookHistory_msg;
        cook_http.msg_len = 512;
        send_http_to_queue(&cook_http);
    }

}
#endif

void mars_steamoven_uartMsgFromSlave(uartmsg_que_t *msg, 
                                mars_template_ctx_t *mars_template_ctx, 
                                uint16_t *index, bool *report_en, uint8_t *nak)
{
    switch (msg->msg_buf[(*index)])
    {
        case prop_LStOvState:
        {
            if (mars_template_ctx->status.LStOvState != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 左腔状态变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.LStOvState, msg->msg_buf[(*index)+1]);
                *report_en = true;

                if (mars_template_ctx->status.LStOvState == 0x00 && msg->msg_buf[(*index)+1] != 0x00)
                {
                    report_cookhistory |= 0x01;
                    LOGI("mars", "左腔工作状态改变: 需要上报烹饪历史(%d -> %d) [0-停止 1-预约 2-预热 3-运行 4-完成 5-运行暂停 6-预约暂停]", mars_template_ctx->status.LStOvState, msg->msg_buf[(*index)+1]);
                }

                if (mars_template_ctx->status.LStOvState == 0x02 && msg->msg_buf[(*index)+1] == 0x03)
                {
                    user_post_event_json(EVENT_LEFT_STOV_PREHEAT_FINISH);
                    LOGI("mars", "推送事件: 左腔预热完成(%d -> %d) [0-停止 1-预约 2-预热 3-运行 4-完成 5-运行暂停 6-预约暂停]", mars_template_ctx->status.LStOvState, msg->msg_buf[(*index)+1]);
                }

                if (mars_template_ctx->status.LStOvState == 0x03 && msg->msg_buf[(*index)+1] == 0x04)
                {
                    user_post_event_json(EVENT_LEFT_STOV_COOK_FINISH);
                    LOGI("mars", "推送事件: 左腔烹饪完成(%d -> %d) [0-停止 1-预约 2-预热 3-运行 4-完成 5-运行暂停 6-预约暂停]", mars_template_ctx->status.LStOvState, msg->msg_buf[(*index)+1]);
                }

                mars_template_ctx->status.LStOvState = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);            
            (*index)+=1;
            break;
        }

        case prop_LStOvMode:
        {
            if (mars_template_ctx->status.LStOvMode != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 左腔模式变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.LStOvMode, msg->msg_buf[(*index)+1]); 
                //*report_en = true;
                mars_template_ctx->status.LStOvMode = msg->msg_buf[(*index)+1];          
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_LStOvSetTemp:
        {
            int16_t tempvalue = (int16_t)(msg->msg_buf[(*index)+1] << 8) | (int16_t)msg->msg_buf[(*index)+2];
            if (mars_template_ctx->status.LStOvSetTemp != tempvalue)
            {                
                LOGI("mars", "解析属性0x%02X: 左腔温度(设置)变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.LStOvSetTemp, tempvalue); 
                //*report_en = true;
                mars_template_ctx->status.LStOvSetTemp = tempvalue;
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=2;
            break;
        }

        case prop_LStOvRealTemp:
        {
            int16_t tempvalue = (int16_t)(msg->msg_buf[(*index)+1] << 8) | (int16_t)msg->msg_buf[(*index)+2];
            if (mars_template_ctx->status.LStOvRealTemp != tempvalue)
            {
                LOGI("mars", "解析属性0x%02X: 左腔温度(实时)变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.LStOvRealTemp, tempvalue); 
                //*report_en = true;
                mars_template_ctx->status.LStOvRealTemp = tempvalue;
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=2;
            break;
        }

        case prop_LStOvSetTimer:
        {
            uint16_t timevalue = (uint16_t)(msg->msg_buf[(*index)+1] << 8) | (uint16_t)msg->msg_buf[(*index)+2];
            if (mars_template_ctx->status.LStOvSetTimer != timevalue)
            {      
                LOGI("mars", "解析属性0x%02X: 左腔时间(设置)变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.LStOvSetTimer, timevalue); 
                //*report_en = true;
                mars_template_ctx->status.LStOvSetTimer = timevalue;                
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=2;
            break;
        }

        case prop_LStOvSetTimerLeft:
        {
            uint16_t timevalue = (uint16_t)(msg->msg_buf[(*index)+1] << 8) | (uint16_t)msg->msg_buf[(*index)+2];
            if (mars_template_ctx->status.LStOvSetTimerLeft != timevalue)
            {
                LOGI("mars", "解析属性0x%02X: 左腔时间(剩余)变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.LStOvSetTimerLeft, timevalue);
                if (mars_template_ctx->status.LStOvState == 0x03)
                    *report_en = true;
                mars_template_ctx->status.LStOvSetTimerLeft = timevalue;
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=2;
            break;
        }
        
        case prop_LStOvOrderTimer:
        {
            uint16_t timevalue = (uint16_t)(msg->msg_buf[(*index)+1] << 8) | (uint16_t)msg->msg_buf[(*index)+2];
            if (mars_template_ctx->status.LStOvOrderTimer != timevalue)
            {
                LOGI("mars", "解析属性0x%02X: 左腔预约时间(设置)变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.LStOvOrderTimer, timevalue);
                //*report_en = true;
                mars_template_ctx->status.LStOvOrderTimer = timevalue;
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=2;
            break;
        }

        case prop_LStOvOrderTimerLeft:
        {
            uint16_t timevalue = (uint16_t)(msg->msg_buf[(*index)+1] << 8) | (uint16_t)msg->msg_buf[(*index)+2];
            if (mars_template_ctx->status.LStOvOrderTimerLeft != timevalue)
            {
                LOGI("mars", "解析属性0x%02X: 左腔预约时间(剩余)变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.LStOvOrderTimerLeft, timevalue);
                if (mars_template_ctx->status.LStOvState == 0x01) //预约状态下才上报
                    *report_en = true;
                mars_template_ctx->status.LStOvOrderTimerLeft = timevalue;
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=2;
            break;
        }

        case prop_LStOvDoorState:
        {
            if (mars_template_ctx->status.LStOvDoorState != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 左腔箱门变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.LStOvDoorState, msg->msg_buf[(*index)+1]); 
                *report_en = true;
                mars_template_ctx->status.LStOvDoorState = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_LStOvLightState:
        {
            if (mars_template_ctx->status.LStOvLightState != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 左腔照明变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.LStOvLightState, msg->msg_buf[(*index)+1]); 
                *report_en = true;
                mars_template_ctx->status.LStOvLightState = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_LMultiStageState:
        {
            uint8_t stage_sum       = mars_template_ctx->status.LMultiStageState.stage_sum;
            uint8_t current_state   = mars_template_ctx->status.LMultiStageState.current_state;
            uint8_t remind          = mars_template_ctx->status.LMultiStageState.remind;
            uint8_t remind_len      = mars_template_ctx->status.LMultiStageState.remind_len;
            if ((stage_sum != msg->msg_buf[(*index)+1]) || (current_state != msg->msg_buf[(*index)+2]) || (remind != msg->msg_buf[(*index)+3])  || (remind_len != msg->msg_buf[(*index)+4]))
            {
                LOGI("mars", "解析属性0x%02X: 左腔多段状态变化(多段总数=%d 当前步骤=%d 是否提醒=%d 提醒长度=%d)", msg->msg_buf[(*index)], msg->msg_buf[(*index)+1], msg->msg_buf[(*index)+2], msg->msg_buf[(*index)+3], msg->msg_buf[(*index)+4]); 
                *report_en = true;
                mars_template_ctx->status.LMultiStageState.stage_sum     = msg->msg_buf[(*index)+1];
                mars_template_ctx->status.LMultiStageState.current_state = msg->msg_buf[(*index)+2];
                mars_template_ctx->status.LMultiStageState.remind        = msg->msg_buf[(*index)+3];
                mars_template_ctx->status.LMultiStageState.remind_len    = msg->msg_buf[(*index)+4];

                if (mars_template_ctx->status.LMultiStageState.remind_len > 0x00 && mars_template_ctx->status.LMultiStageState.remind_len <= 0x28)
                {
                    memset(mars_template_ctx->status.LMultiStageState.remind_buf, 0x00,  sizeof(mars_template_ctx->status.LMultiStageState.remind_buf));
                    memcpy(mars_template_ctx->status.LMultiStageState.remind_buf, msg->msg_buf+(*index)+5, mars_template_ctx->status.LMultiStageState.remind_len);
                }
                else
                {
                    mars_template_ctx->status.LMultiStageState.remind_len = 0;
                    memset(mars_template_ctx->status.LMultiStageState.remind_buf, 0, sizeof(mars_template_ctx->status.LMultiStageState.remind_buf));
                }
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(prop_LMultiStageState);
            (*index) += mars_template_ctx->status.LMultiStageState.remind_len+4;
            break;
        }
        
        case prop_LCookbookName:
        {
            uint8_t recipe_name_len = msg->msg_buf[(*index)+1];
            LOGI("mars", "解析属性0x%02X: 左腔菜谱名称", msg->msg_buf[(*index)]); 
            if ((recipe_name_len > 0) && (recipe_name_len <= 32))
            {
                if (memcmp(mars_template_ctx->status.LCookbookName.buf, msg->msg_buf+(*index)+2, recipe_name_len) != 0)
                {
                    LOGI("mars", "解析属性0x%02X，左腔菜谱名称需要上报");
                    *report_en = true;
                    mars_template_ctx->status.LCookbookName.len = msg->msg_buf[(*index)+1];
                    memset(mars_template_ctx->status.LCookbookName.buf, 0x00,  sizeof(mars_template_ctx->status.LCookbookName.buf));
                    memcpy(mars_template_ctx->status.LCookbookName.buf, msg->msg_buf+(*index)+2, mars_template_ctx->status.LCookbookName.len);
                }
            }
            else
            {
                mars_template_ctx->status.LCookbookName.len = 0;
                memset(mars_template_ctx->status.LCookbookName.buf, 0x00,  sizeof(mars_template_ctx->status.LCookbookName.buf));
            }

            (*index) += (mars_template_ctx->status.LCookbookName.len+1);
            mars_template_ctx->steamoven_reportflg |= VALID_BIT(prop_LCookbookName);
            break;
        }

        case prop_LMultiMode:
        {
            if (mars_template_ctx->status.LMultiMode != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 左腔烹饪模式变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.LMultiMode, msg->msg_buf[(*index)+1]); 
                *report_en = true;
                mars_template_ctx->status.LMultiMode = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_LCookbookID:
        {
            uint32_t recipe_id = ((uint32_t)msg->msg_buf[(*index)+1] << 24) | ((uint32_t)msg->msg_buf[(*index)+2] << 16) | ((uint32_t)msg->msg_buf[(*index)+3] << 8) | (uint32_t)msg->msg_buf[(*index)+4];
            if (mars_template_ctx->status.LCookbookID != recipe_id)
            {
                LOGI("mars", "解析属性0x%02X: 左腔菜谱ID变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.LCookbookID, recipe_id);
                *report_en = true;
                mars_template_ctx->status.LCookbookID = recipe_id;
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=4;
            break;
        }

        case prop_RStOvState:
        {
            if (mars_template_ctx->status.RStOvState != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 右腔状态变化(%d - %d) [0-停止 1-预约 2-预热 3-运行 4-完成 5-运行暂停 6-预约暂停]", msg->msg_buf[(*index)], mars_template_ctx->status.RStOvState, msg->msg_buf[(*index)+1]); 
                *report_en = true;

                if (mars_template_ctx->status.RStOvState == 0x00 && msg->msg_buf[(*index)+1] != 0x00)//右腔首次进入蒸烤动作，需要上报烹饪历史
                {
                    report_cookhistory |= 0x02;
                    LOGI("mars", "右腔工作状态改变: 需要上报烹饪历史(%d -> %d) [0-停止 1-预约 2-预热 3-运行 4-完成 5-运行暂停 6-预约暂停]", mars_template_ctx->status.RStOvState, msg->msg_buf[(*index)+1]);
                }

                if (mars_template_ctx->status.RStOvState == 0x02 && msg->msg_buf[(*index)+1] == 0x03)
                {
                    user_post_event_json(EVENT_RIGHT_STOV_PREHEAT_FINISH);
                    LOGI("mars", "推送事件: 右腔预热完成RHeatPush! (%d -> %d) [0-停止 1-预约 2-预热 3-运行 4-完成 5-运行暂停 6-预约暂停]", mars_template_ctx->status.RStOvState, msg->msg_buf[(*index)+1]);
                }

                if (mars_template_ctx->status.RStOvState == 0x03 && msg->msg_buf[(*index)+1] == 0x04)
                {
                    user_post_event_json(EVENT_RIGHT_STOV_COOK_FINISH);
                    LOGI("mars", "推送事件: 右腔烹饪完成RCookPush! (%d -> %d) [0-停止 1-预约 2-预热 3-运行 4-完成 5-运行暂停 6-预约暂停]", mars_template_ctx->status.RStOvState, msg->msg_buf[(*index)+1]);
                }

                mars_template_ctx->status.RStOvState = msg->msg_buf[(*index)+1];      
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_RStOvMode:
        {
            if (mars_template_ctx->status.RStOvMode != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 右腔模式变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.RStOvMode, msg->msg_buf[(*index)+1]); 
                *report_en = true;
                mars_template_ctx->status.RStOvMode = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_RStOvSetTemp:
        {
            int16_t tempvalue = (int16_t)(msg->msg_buf[(*index)+1] << 8) | (int16_t)msg->msg_buf[(*index)+2];
            if (mars_template_ctx->status.RStOvSetTemp != tempvalue)
            {                
                LOGI("mars", "解析属性0x%02X: 右腔温度(设置)变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.RStOvSetTemp, tempvalue); 
                //*report_en = true;
                mars_template_ctx->status.RStOvSetTemp = tempvalue;
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=2;
            break;
        }

        case prop_RStOvRealTemp:
        {
            int16_t tempvalue = (int16_t)(msg->msg_buf[(*index)+1] << 8) | (int16_t)msg->msg_buf[(*index)+2];
            if (mars_template_ctx->status.RStOvRealTemp != tempvalue)
            {
                LOGI("mars", "解析属性0x%02X: 右腔温度(实时)变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.RStOvRealTemp, tempvalue); 
                //*report_en = true;
                mars_template_ctx->status.RStOvRealTemp = tempvalue;
            } 

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=2;
            break;
        }

        case prop_RStOvSetTimer:
        {
            uint16_t timevalue = (uint16_t)(msg->msg_buf[(*index)+1] << 8) | (uint16_t)msg->msg_buf[(*index)+2];
            if (mars_template_ctx->status.RStOvSetTimer != timevalue)
            {      
                LOGI("mars", "解析属性0x%02X: 右腔时间(设置)变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.RStOvSetTimer, timevalue); 
                //*report_en = true;
                mars_template_ctx->status.RStOvSetTimer = timevalue;
            }  

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=2;
            break;
        }

        case prop_RStOvSetTimerLeft:
        {
            uint16_t timevalue = (uint16_t)(msg->msg_buf[(*index)+1] << 8) | (uint16_t)msg->msg_buf[(*index)+2];
            if (mars_template_ctx->status.RStOvSetTimerLeft != timevalue)
            {
                LOGI("mars", "解析属性0x%02X: 右腔时间(剩余)变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.RStOvSetTimerLeft, timevalue);
                if (mars_template_ctx->status.RStOvState == 0x03)
                    *report_en = true;
                mars_template_ctx->status.RStOvSetTimerLeft = timevalue;
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=2;
            break;
        }

        case prop_RStOvOrderTimer:
        {
            uint16_t timevalue = (uint16_t)(msg->msg_buf[(*index)+1] << 8) | (uint16_t)msg->msg_buf[(*index)+2];
            if (mars_template_ctx->status.RStOvOrderTimer != timevalue)
            {
                LOGI("mars", "解析属性0x%02X: 右腔预约时间(设置)变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.RStOvOrderTimer, timevalue);
                //*report_en = true;
                mars_template_ctx->status.RStOvOrderTimer = timevalue;
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=2;
            break;
        }

        case prop_RStOvOrderTimerLeft:
        {
            uint16_t timevalue = (uint16_t)(msg->msg_buf[(*index)+1] << 8) | (uint16_t)msg->msg_buf[(*index)+2];
            if (mars_template_ctx->status.RStOvOrderTimerLeft != timevalue)
            {
                LOGI("mars", "解析属性0x%02X: 右腔预约时间(剩余)变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.RStOvOrderTimerLeft, timevalue);
                if (mars_template_ctx->status.RStOvState == 0x01) //预约状态下才上报
                    *report_en = true;
                mars_template_ctx->status.RStOvOrderTimerLeft = timevalue;
            }            

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=2;
            break;
        }

        case prop_RStOvDoorState:
        {
            if (mars_template_ctx->status.RStOvDoorState != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 右腔箱门变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.RStOvDoorState, msg->msg_buf[(*index)+1]); 
                *report_en = true;
                mars_template_ctx->status.RStOvDoorState = msg->msg_buf[(*index)+1];
            }  

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_RStOvLightState:
        {
            if (mars_template_ctx->status.RStOvLightState != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 右腔照明变化(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.RStOvLightState, msg->msg_buf[(*index)+1]); 
                *report_en = true;
                mars_template_ctx->status.RStOvLightState = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        case prop_RMultiMode:
        {
            if (mars_template_ctx->status.RMultiMode != msg->msg_buf[(*index)+1])
            {
                LOGI("mars", "解析属性0x%02X: 右腔烹饪模式(%d - %d)", msg->msg_buf[(*index)], mars_template_ctx->status.RMultiMode, msg->msg_buf[(*index)+1]); 
                *report_en = true;
                mars_template_ctx->status.RMultiMode = msg->msg_buf[(*index)+1];
            }

            mars_template_ctx->steamoven_reportflg |= VALID_BIT(msg->msg_buf[(*index)]);
            (*index)+=1;
            break;
        }

        default:
            *nak = NAK_ERROR_UNKOWN_PROCODE;
            break;
    }
}

void mars_steamoven_setToSlave(cJSON *root, cJSON *item, mars_template_ctx_t *mars_template_ctx, uint8_t *buf_setmsg, uint16_t *buf_len)
{
    if (NULL == root || NULL == mars_template_ctx || NULL == buf_setmsg || NULL == buf_len){
        return;
    }

    if ((item = cJSON_GetObjectItem(root, "LMultiMode")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.LMultiMode = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_LMultiMode;
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "LStOvMode")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.LStOvMode = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_LStOvMode;
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "LSteamGear")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.LSteamGear = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_LSteamGear;
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "LMicroWaveGear")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.LMicroGear = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_LMicroWaveGear;
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "LStOvSetTemp")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.LStOvSetTemp = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_LStOvSetTemp;
        buf_setmsg[(*buf_len)++] = (uint8_t)(item->valueint >> 8);
        buf_setmsg[(*buf_len)++] = (uint8_t)(item->valueint);

    }

    if ((item = cJSON_GetObjectItem(root, "LStOvSetTimer")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.LStOvSetTimer = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_LStOvSetTimer;
        buf_setmsg[(*buf_len)++] = (uint8_t)(item->valueint >> 8);
        buf_setmsg[(*buf_len)++] = (uint8_t)(item->valueint);
    }

    if ((item = cJSON_GetObjectItem(root, "LStOvOrderTimer")) != NULL && cJSON_IsNumber(item)) {
        //mars_template_ctx->status.LStOvOrderTimer = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_LStOvOrderTimer;
        buf_setmsg[(*buf_len)++] = (uint8_t)(item->valueint >> 8);
        buf_setmsg[(*buf_len)++] = (uint8_t)(item->valueint);
    }
    
    if (((item = cJSON_GetObjectItem(root, "LMultiStageContent")) != NULL || (item = cJSON_GetObjectItem(root, "LCookbookParam")) != NULL) && cJSON_IsArray(item)) 
    {
        buf_setmsg[(*buf_len)++] = prop_LMultiStageContent;
        mars_template_ctx->status.LMultiStageContent.stage_sum = cJSON_GetArraySize(item);
        buf_setmsg[(*buf_len)++] = mars_template_ctx->status.LMultiStageContent.stage_sum;

        if (3 >= mars_template_ctx->status.LMultiStageContent.stage_sum){

            for (int i=0;i<mars_template_ctx->status.LMultiStageContent.stage_sum;++i){
                cJSON *obj = cJSON_GetArrayItem(item, i);
                if (obj != NULL){
                    cJSON *tmp_item = NULL;
                    //步骤编号
                    char *tp = cJSON_PrintUnformatted(obj);
                    LOGI("mars", "%s", tp);

                    if ( (tmp_item = cJSON_GetObjectItem(obj, "Valid"))!= NULL && cJSON_IsNumber(tmp_item)){
                        mars_template_ctx->status.LMultiStageContent.stage[i].stage_index = i+1;                        
                    }else{
                        mars_template_ctx->status.LMultiStageContent.stage[i].stage_index = 0;  
                    }
                    buf_setmsg[(*buf_len)++] = mars_template_ctx->status.LMultiStageContent.stage[i].stage_index;     
                    //提醒与否
                    if ((tmp_item = cJSON_GetObjectItem(obj, "Paused")) != NULL && cJSON_IsNumber(tmp_item)){
                        mars_template_ctx->status.LMultiStageContent.stage[i].remind = tmp_item->valueint;
                    }else{
                        mars_template_ctx->status.LMultiStageContent.stage[i].remind = 0;
                    }
                    buf_setmsg[(*buf_len)++] = mars_template_ctx->status.LMultiStageContent.stage[i].remind;
                    //工作模式
                    if ((tmp_item = cJSON_GetObjectItem(obj, "Mode")) != NULL && cJSON_IsNumber(tmp_item)){
                        mars_template_ctx->status.LMultiStageContent.stage[i].workmode = tmp_item->valueint;
                    }else{
                        mars_template_ctx->status.LMultiStageContent.stage[i].workmode = 0;
                    }
                    buf_setmsg[(*buf_len)++] = mars_template_ctx->status.LMultiStageContent.stage[i].workmode;
                    //设定温度
                    if ((tmp_item = cJSON_GetObjectItem(obj, "Temp")) != NULL && cJSON_IsNumber(tmp_item)){
                        mars_template_ctx->status.LMultiStageContent.stage[i].set_temper = tmp_item->valueint;
                    }else{
                        mars_template_ctx->status.LMultiStageContent.stage[i].set_temper = 0;
                    }
                    buf_setmsg[(*buf_len)++] = (uint8_t)(mars_template_ctx->status.LMultiStageContent.stage[i].set_temper >> 8);
                    buf_setmsg[(*buf_len)++] = (uint8_t)(mars_template_ctx->status.LMultiStageContent.stage[i].set_temper);
                    //设定时间
                    if ((tmp_item = cJSON_GetObjectItem(obj, "Timer")) != NULL && cJSON_IsNumber(tmp_item)){
                        mars_template_ctx->status.LMultiStageContent.stage[i].set_time = tmp_item->valueint;
                    }else{
                        mars_template_ctx->status.LMultiStageContent.stage[i].set_time = 0;
                    }
                    buf_setmsg[(*buf_len)++] = (uint8_t)(mars_template_ctx->status.LMultiStageContent.stage[i].set_time >> 8);
                    buf_setmsg[(*buf_len)++] = (uint8_t)(mars_template_ctx->status.LMultiStageContent.stage[i].set_time);

                    //蒸汽盘外圈时间
                    mars_template_ctx->status.LMultiStageContent.stage[i].steam_time = 0;
                    buf_setmsg[(*buf_len)++] = (uint8_t)(mars_template_ctx->status.LMultiStageContent.stage[i].steam_time >> 8);
                    buf_setmsg[(*buf_len)++] = (uint8_t)(mars_template_ctx->status.LMultiStageContent.stage[i].steam_time);
                    //侧面风机时间
                    mars_template_ctx->status.LMultiStageContent.stage[i].fan_time = 0;
                    buf_setmsg[(*buf_len)++] = (uint8_t)(mars_template_ctx->status.LMultiStageContent.stage[i].fan_time >> 8);
                    buf_setmsg[(*buf_len)++] = (uint8_t)(mars_template_ctx->status.LMultiStageContent.stage[i].fan_time);
                    //注水时间
                    mars_template_ctx->status.LMultiStageContent.stage[i].water_time= 0;
                    buf_setmsg[(*buf_len)++] = mars_template_ctx->status.LMultiStageContent.stage[i].water_time;

                    //蒸汽档位
                    if ((tmp_item = cJSON_GetObjectItem(obj, "SteamGear")) != NULL && cJSON_IsNumber(tmp_item)){
                        mars_template_ctx->status.LMultiStageContent.stage[i].SteamGear = tmp_item->valueint;
                    }else{
                        mars_template_ctx->status.LMultiStageContent.stage[i].SteamGear = 2;
                    }
                    buf_setmsg[(*buf_len)++] = mars_template_ctx->status.LMultiStageContent.stage[i].SteamGear;

                    //提醒内容长度
                    //mars_template_ctx->status.LMultiStageContent.stage[i].remind_len = 0;
                    //buf_setmsg[(*buf_len)++] = mars_template_ctx->status.LMultiStageContent.stage[i].remind_len;

                    if ((tmp_item = cJSON_GetObjectItem(obj, "RemindText")) != NULL && cJSON_IsString(tmp_item) && strlen(tmp_item->valuestring) > 0)
                    {
                        mars_template_ctx->status.LMultiStageContent.stage[i].remind_len = strlen(tmp_item->valuestring);
                        memcpy(mars_template_ctx->status.LMultiStageContent.stage[i].remind_buf, tmp_item->valuestring, strlen(tmp_item->valuestring));

                        buf_setmsg[(*buf_len)++] = strlen(tmp_item->valuestring);
                        memcpy(buf_setmsg + (*buf_len), tmp_item->valuestring, strlen(tmp_item->valuestring));
                        LOGI("mars", "第%d段: 提醒长度=%d, 提醒内容=%s", (i+1), strlen(tmp_item->valuestring), buf_setmsg+(*buf_len));
                        (*buf_len) += strlen(tmp_item->valuestring);
                    }
                    else
                    {
                        mars_template_ctx->status.LMultiStageContent.stage[i].remind_len = 0;
                        buf_setmsg[(*buf_len)++] = mars_template_ctx->status.LMultiStageContent.stage[i].remind_len;
                        LOGI("mars", "第%d段: 提醒长度=0", (i+1));
                    }
                }
            }
            //云端是否下发烹饪模式？
            // if (!strcmp("LMultiStageContent", item->string)){
            //     mars_template_ctx->status.LMultiMode = 0x02;
            //     buf_setmsg[(*buf_len)++] = prop_LMultiMode;
            //     buf_setmsg[(*buf_len)++] = mars_template_ctx->status.LMultiMode;                
            // }else{
            //     mars_template_ctx->status.LMultiMode = 0x01;
            //     buf_setmsg[(*buf_len)++] = prop_LMultiMode;
            //     buf_setmsg[(*buf_len)++] = mars_template_ctx->status.LMultiMode;    
            // }
        }
    }

    if ((item = cJSON_GetObjectItem(root, "LCookbookName")) != NULL && cJSON_IsString(item)) 
    {
        mars_template_ctx->status.LCookbookName.len = strlen(item->valuestring);
        memcpy(mars_template_ctx->status.LCookbookName.buf, \
                item->valuestring, \
                mars_template_ctx->status.LCookbookName.len);
        buf_setmsg[(*buf_len)++] = prop_LCookbookName;
        buf_setmsg[(*buf_len)++] = mars_template_ctx->status.LCookbookName.len;
        memcpy( buf_setmsg+(*buf_len), \
                mars_template_ctx->status.LCookbookName.buf, \
                mars_template_ctx->status.LCookbookName.len);
        (*buf_len) += mars_template_ctx->status.LCookbookName.len;
    }

    if ((item = cJSON_GetObjectItem(root, "LCookbookID")) != NULL && cJSON_IsNumber(item)) 
    {
        mars_template_ctx->status.LCookbookID = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_LCookbookID;
        buf_setmsg[(*buf_len)++] = (uint8_t)(mars_template_ctx->status.LCookbookID >> 24);
        buf_setmsg[(*buf_len)++] = (uint8_t)(mars_template_ctx->status.LCookbookID >> 16);
        buf_setmsg[(*buf_len)++] = (uint8_t)(mars_template_ctx->status.LCookbookID >> 8);
        buf_setmsg[(*buf_len)++] = (uint8_t)mars_template_ctx->status.LCookbookID;        
    }

    if ((item = cJSON_GetObjectItem(root, "LStOvOperation")) != NULL && cJSON_IsNumber(item)) 
    {
        //mars_template_ctx->status.LStOvOperation = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_LStOvOperation;
        buf_setmsg[(*buf_len)++] = item->valueint;
    }


    if ((item = cJSON_GetObjectItem(root, "RMultiMode")) != NULL && cJSON_IsNumber(item)) 
    {
        //mars_template_ctx->status.RMultiMode = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_RMultiMode;
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "RStOvMode")) != NULL && cJSON_IsNumber(item)) 
    {
        //mars_template_ctx->status.RStOvMode = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_RStOvMode;
        buf_setmsg[(*buf_len)++] = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(root, "RStOvSetTemp")) != NULL && cJSON_IsNumber(item)) 
    {
        //mars_template_ctx->status.RStOvSetTemp = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_RStOvSetTemp;
        buf_setmsg[(*buf_len)++] = (uint8_t)(item->valueint >> 8);
        buf_setmsg[(*buf_len)++] = (uint8_t)(item->valueint);
    }

    if ((item = cJSON_GetObjectItem(root, "RStOvSetTimer")) != NULL && cJSON_IsNumber(item)) 
    {
        //mars_template_ctx->status.RStOvSetTimer = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_RStOvSetTimer;
        buf_setmsg[(*buf_len)++] = (uint8_t)(item->valueint >> 8);
        buf_setmsg[(*buf_len)++] = (uint8_t)(item->valueint);
    }

    if ((item = cJSON_GetObjectItem(root, "RStOvOrderTimer")) != NULL && cJSON_IsNumber(item)) 
    {
        //mars_template_ctx->status.RStOvOrderTimer = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_RStOvOrderTimer;
        buf_setmsg[(*buf_len)++] = (uint8_t)(item->valueint >> 8);
        buf_setmsg[(*buf_len)++] = (uint8_t)(item->valueint);
    }

    if ((item = cJSON_GetObjectItem(root, "RStOvOperation")) != NULL && cJSON_IsNumber(item)) 
    {
        //mars_template_ctx->status.RStOvOperation = item->valueint;
        buf_setmsg[(*buf_len)++] = prop_RStOvOperation;
        buf_setmsg[(*buf_len)++] = item->valueint;
    }
}

void mars_steamoven_changeReport(cJSON *proot, mars_template_ctx_t *mars_template_ctx)
{
    for (uint8_t index=prop_LStOvOperation; index<=prop_RCookbookID; ++index){
        if (mars_template_ctx->steamoven_reportflg & VALID_BIT(index)){
            switch (index){
                case prop_LStOvOperation:
                {
                    cJSON_AddNumberToObject(proot, "LStOvOperation", mars_template_ctx->status.LStOvOperation);
                    break;
                }
                case prop_LStOvState:
                {
                    cJSON_AddNumberToObject(proot, "LStOvState", mars_template_ctx->status.LStOvState);
                    break;
                }
                case prop_LStOvMode:
                {
                    cJSON_AddNumberToObject(proot, "LStOvMode", mars_template_ctx->status.LStOvMode);
                    break;
                }
                case prop_LStOvSetTemp:
                {
                    cJSON_AddNumberToObject(proot, "LStOvSetTemp", mars_template_ctx->status.LStOvSetTemp);
                    break;
                }
                case prop_LStOvRealTemp:
                {
                    cJSON_AddNumberToObject(proot, "LStOvRealTemp", mars_template_ctx->status.LStOvRealTemp);
                    break;
                }
                case prop_LStOvSetTimer:
                {
                    cJSON_AddNumberToObject(proot, "LStOvSetTimer", mars_template_ctx->status.LStOvSetTimer);
                    break;
                }
                case prop_LStOvSetTimerLeft:
                {
                    cJSON_AddNumberToObject(proot, "LStOvSetTimerLeft", mars_template_ctx->status.LStOvSetTimerLeft);
                    break;
                }
                case prop_LStOvOrderTimer:
                {
                    cJSON_AddNumberToObject(proot, "LStOvOrderTimer", mars_template_ctx->status.LStOvOrderTimer);
                    break;
                }
                case prop_LStOvOrderTimerLeft:
                {
                    cJSON_AddNumberToObject(proot, "LStOvOrderTimerLeft", mars_template_ctx->status.LStOvOrderTimerLeft);
                    break;
                }
                case prop_LStOvDoorState:
                {
                    cJSON_AddNumberToObject(proot, "LStOvDoorState", mars_template_ctx->status.LStOvDoorState);
                    break;
                }
                case prop_LStOvLightState:
                {
                    cJSON_AddNumberToObject(proot, "LStOvLightState", mars_template_ctx->status.LStOvLightState);
                    break;
                }
                case prop_LMultiStageContent:
                {
                    break;
                }
                case prop_LMultiStageState:
                {
                    cJSON *item = cJSON_CreateObject();
                    if (item){
                        cJSON_AddNumberToObject(item, "cnt", mars_template_ctx->status.LMultiStageState.stage_sum);
                        cJSON_AddNumberToObject(item, "current", mars_template_ctx->status.LMultiStageState.current_state);
                        cJSON_AddNumberToObject(item, "remind",  mars_template_ctx->status.LMultiStageState.remind);
                        cJSON_AddStringToObject(item, "RemindText",  mars_template_ctx->status.LMultiStageState.remind_buf);
                        cJSON_AddItemToObject(proot, "LMultiStageState", item);
                    }
                    break;
                }
                case prop_LMultiMode:
                {
                    cJSON_AddNumberToObject(proot, "LMultiMode", mars_template_ctx->status.LMultiMode);
                    break;
                }
                case prop_LCookbookName:
                {
                    cJSON_AddStringToObject(proot, "LCookbookName", mars_template_ctx->status.LCookbookName.buf);
                    break;
                }
                case prop_LCookbookID:
                {
                    cJSON_AddNumberToObject(proot, "LCookbookID", mars_template_ctx->status.LCookbookID);
                    break; 
                }
                case prop_RStOvOperation:
                {
                    cJSON_AddNumberToObject(proot, "RStOvOperation", mars_template_ctx->status.RStOvOperation);
                    break;
                }
                case prop_RStOvState:
                {
                    cJSON_AddNumberToObject(proot, "RStOvState", mars_template_ctx->status.RStOvState);
                    break;
                }
                case prop_RStOvSetTemp:
                {
                    cJSON_AddNumberToObject(proot, "RStOvSetTemp", mars_template_ctx->status.RStOvSetTemp);
                    break;
                }
                case prop_RStOvRealTemp:
                {
                    cJSON_AddNumberToObject(proot, "RStOvRealTemp", mars_template_ctx->status.RStOvRealTemp);
                    break;
                }
                case prop_RStOvSetTimer:
                {
                    cJSON_AddNumberToObject(proot, "RStOvSetTimer", mars_template_ctx->status.RStOvSetTimer);
                    break;
                }
                case prop_RStOvSetTimerLeft:
                {
                    cJSON_AddNumberToObject(proot, "RStOvSetTimerLeft", mars_template_ctx->status.RStOvSetTimerLeft);
                    break;
                }
                case prop_RStOvOrderTimer:
                {
                    cJSON_AddNumberToObject(proot, "RStOvOrderTimer", mars_template_ctx->status.RStOvOrderTimer);
                    break;
                }
                case prop_RStOvOrderTimerLeft:
                {
                    cJSON_AddNumberToObject(proot, "RStOvOrderTimerLeft", mars_template_ctx->status.RStOvOrderTimerLeft);
                    break;
                }
                case prop_RStOvDoorState:
                {
                    cJSON_AddNumberToObject(proot, "RStOvDoorState", mars_template_ctx->status.RStOvDoorState);
                    break;
                }
                case prop_RStOvLightState:
                {
                    cJSON_AddNumberToObject(proot, "RStOvLightState", mars_template_ctx->status.RStOvLightState);
                    break;
                }
                case prop_RStOvMode:
                {
                    cJSON_AddNumberToObject(proot, "RStOvMode", mars_template_ctx->status.RStOvMode);                
                    break;
                }
                default:
                    break;
            }
        }
    }

    if (0x01 == report_cookhistory){        //左腔     
        mars_cookhistory_logpost(0);
    }else if (0x02 == report_cookhistory){  //右腔       
        mars_cookhistory_logpost(1);
    }else if(0x03 == report_cookhistory){   //两边同时        
        mars_cookhistory_logpost(0);
        mars_cookhistory_logpost(1);
    }
    report_cookhistory = 0;
    //mars_template_ctx->status.LCookbookID = 0;
    // mars_template_ctx->status.LMultiMode = 0;
    mars_template_ctx->steamoven_reportflg = 0;
}
#endif