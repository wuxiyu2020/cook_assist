#ifndef _POT_COVER_H_
#define _POT_COVER_H_

#ifdef __cplusplus
extern "C"
{
#endif

extern bool g_pot_cover_app_is_open;
extern long pot_cover_scenes;

enum POT_COVER_TYPE
{
    COVER_UNKONW = 0,       // 初始状态，不知道有没有盖，什么盖
    COVER_NONE,             // 无盖
    COVER_GLASS,            // 玻璃盖
    COVER_METAL,            // 金属盖 （铝盖）  玻璃和金属混合   目前把这两种合并　因为特性差不多
    COVER_GLASS_POT,        // 透明玻璃锅
    // COVER_GLASS_AND_METAL,  // 
};
extern enum POT_COVER_TYPE g_pot_cover_type;
extern enum POT_COVER_TYPE g_last_judge_cover_type;

enum POT_COVER_SCENE  // 场景
{
    /* 场景1：玻璃盖
    // *   1.灶具开启动时温度小于５０度；
    // *   2.0－50度内出现了平稳，并且平稳链表里存在node持续时间至少３０秒
    * 改成:0－灶具开启动时温度 内出现了平稳，并且平稳链表里存在node持续时间至少３０秒
    */ 
    POT_COVER_SCENE_1 = 1,  // 因为要移位，至少从１开始

    /* 场景1.1：玻璃盖
    *   1.灶具开启动时温度小于５０度；
    *   2.0－50度内出现了平稳，并且平稳链表里存在至少2个node持续时间至少１５秒,并且存在
    *   3.当前温度小于８０度
    */ 
    POT_COVER_SCENE_1_1,

    /* 场景1.2：金属盖
    *   1.灶具开启动时温度小于50度；　　// 这个场景的判断需要在　1.1后面执行
    *   2.0－50度内出现了平稳，并且平稳链表里存在至少3个node持续时间至少25秒
    *   3.当前温度小于57度
    */ 
    // POT_COVER_SCENE_1_2,

    /* 场景1.2.1：对1.2做补充
    *   1.灶具开启动时温度小于50度；　　// 这个场景的判断需要在　1.2后面执行
    *   2.0－50度内出现了平稳，40度之前出现过至少25秒的平缓，大于等于40度后又出现过至少25秒的平缓
    *   3.当前温度小于57度
    */ 
    // POT_COVER_SCENE_1_2_1,

    /* 场景2：无盖
    *   1.灶具开启动时温度小于５０度；
    *   2.没出现过SCENE_1
    *   2.在初始温度－５０度内出现了上升，并且上升链表里存在node持续时间至少３０秒
    */ 
    POT_COVER_SCENE_2,

    POT_COVER_SCENE_3,  // 开盖

    POT_COVER_SCENE_4,  // 盖盖

    /* 场景5：  
    *   1.如果当前不是无盖，但是90度还没水开，就更新为无盖
    */ 
    POT_COVER_SCENE_5,

    /* 场景6：  
    *   1.如果当前是金属盖，但是５７度还没水开，就更新为玻璃盖
    */ 
    POT_COVER_SCENE_6,

    /* 场景7：  无盖判断
    *   1.80度前
    *   2.平稳链表中不存在持续时间大于200ticks的node,大于120的最多一个;链表中存在3个持续ticks大于40,小于48
    */ 
    POT_COVER_SCENE_7,

    /* 场景8:
    *   1.累计<=40度的平稳链表中的总时间，如果时间超过>=2分钟，就认为是玻璃盖
    */ 
    POT_COVER_SCENE_8,

    /* 场景9:  玻璃锅
    *   1. 开始烹饪20秒内出现了跳升
    *   2. 存在3次以上跳升和跳降，并且温差大于等于6度
    */ 
    POT_COVER_SCENE_9,
};

typedef struct sig_open_cover  // 打开盖子
{
    enum CA_SIGNALS type;
} sig_open_cover_t;

typedef struct sig_close_cover  // 关闭盖子
{
    enum CA_SIGNALS type;
} sig_close_cover_t;

void reset_pot_cover_value();
void pot_cover_add_signals();
int pot_cover_signals_worker(ca_signal_list_t *p);
void reset_pot_cover_signals();

#ifdef __cplusplus
}
#endif
#endif  // _POT_COVER_H_