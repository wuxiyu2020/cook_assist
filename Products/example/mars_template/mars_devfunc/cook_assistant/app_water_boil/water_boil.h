#ifndef _WATER_BOIL_H_
#define _WATER_BOIL_H_

#ifdef __cplusplus
extern "C"
{
#endif

extern bool g_water_boil_app_is_open;
typedef struct sig_water_boil sig_water_boil_t;
extern sig_water_boil_t *g_sig_water_boil;
extern bool g_water_is_boil;

// 烧水场景
enum WATER_BOIL_SCENE
{
    SCENE_START = 0,
    /* 场景1：
    *   1.没盖盖子
    *   2.85－90度是在上升状态   ET70 80－85改85－90
    */ 
    WATER_BOIL_SCENE_1 = 1,


    /* 场景1.1：
    *   1.场景1已经出现过了
    *   2.在场景1.1在那之后1分钟内，92度以上出现了平稳超过15秒　　15秒有调优空间    
    */ 
    WATER_BOIL_SCENE_1_1,  // 命名规则，如果依赖其他场景，就在其他场景后面加_N


    /* 场景1.2：  目的：优化场景1.1，防止水开了，但硬要等到平稳才触发。反过来，场景1.1也可以作为1.2的兜底
    *   1.温度大于90度时，上升速率比场景1中记录的water_boil_scenes1_temp_range_min_slope速率大
    */ 
    WATER_BOIL_SCENE_1_2,


    /* 场景2：
    *   1.盖了玻璃盖
    *   2.５０－８５度范围内出现上升越来越快
    *   3.10个平均温度大于７0度后存在3个速率大于等于1500
    */
    WATER_BOIL_SCENE_2,


    /* 场景3：  用户眼看着看了，但是程序还没判开，用户打开盖子满足这个规则后就判开
    *   １.35－85度范围出现跳升　　　这个温度范围可能需要调节
    * 如果跳升温度大于９３小于１００度，就在这个时间点上延迟５秒水开，
    *   否则记录跳升后的jump_rise node，记录的前提是跳升>=6度
    */ 
    WATER_BOIL_SCENE_3,


    /* 场景3.1：
    *   1.在场景3记住了jump_rise node基础上
    *   2.出现跳降,跳降后温度与场景3跳升前的温度不相差3度
    */ 
    WATER_BOIL_SCENE_3_1,


    /* 场景3.1.1：
    *   1.在场景3.1基础上
    *   2.温度上升越来越快发生在85度以上就认为水开了（突然跳升，又跳降，可能打开盖子，又关闭盖子，等到越来越快时，就认为水开了） 
    */ 
    WATER_BOIL_SCENE_3_1_1,


    /* 场景3.1.2：
    *   1.在场景3.1基础上
    *   2.10个温度计算的平均值大于90度  （作为WATER_BOIL_SCENE_3_1_1的保底）
    */ 
    WATER_BOIL_SCENE_3_1_2,


    /* 场景3.2：
    *   １.在场景３基础上
    *   ２．出现平缓
    */ 
    WATER_BOIL_SCENE_3_2,


    /* 场景4：　
    *   1.温度大于92度平稳 15秒
    */ 
    WATER_BOIL_SCENE_4,


    /* 场景5：　
    *   １.温度大于90度，触发了上升越来越快
    */ 
    WATER_BOIL_SCENE_5,


    /* 场景6：　煮少量水时，前面一些场景的路径很难触发
    *   １.10个温度计算的平均值大于9５度
    */ 
    WATER_BOIL_SCENE_6,


    /* 场景7：　存在盖着玻璃盖，不会有越来越快的现象发生
    *   １.盖着玻璃盖
    *　　2.最后一个跳升node开始温度>=70，利用越来越快链表，计算最后一个node上升速率，要求速率大于等于　125/tick（斜率扩大1000倍），即0.5度/s
    */
    WATER_BOIL_SCENE_7,


    /* 场景7.1：　
    *   １.当前是玻璃盖或者金属盖（盖子判断出现错误时）
    *　　2.保底，８５度以上出现平缓１5秒
    */ 
    WATER_BOIL_SCENE_7_1,


    /* 场景8：
    *   1.盖了金属盖
    *   2.在38到57度内
    *   2.１０个温度计算的平均温度大于等于n(38<=n<=57)度后，单个温度在n度上下两上下２度内持续了30-60秒，就水开
    */ 
    // WATER_BOIL_SCENE_8,  // 暂不支持金属盖烧水


    /* 场景8.1： 
    *   1.盖了金属盖
    *   2.平稳链表里面存在至少５个持续时间大于２５秒的节点
    *   3.之后出现两个小于２５秒的节点，并且后一个比前一个时间短
    */ 
    // WATER_BOIL_SCENE_8_1,  // 与盖子场景POT_COVER_SCENE_1_2类似


    /* 场景９：　　// 需要去除跳升情况
    *   1.７０度以上瞬时上升速率大于３000　　玻璃盖
    *   2.或者80度以上瞬时上升速率大于1500　　玻璃盖
    *   2.或者90度以上瞬时上升速率大于1250　　无盖
    */ 
    WATER_BOIL_SCENE_9,


    /* 场景10:    焯水时,有浮沫,温度稳步了90以上   环境温度低/有风时也可套用?
    *   1.平稳链表里存在大于85度以上的节点,并且节点总时间加起来超过30秒
    *   2.之后累计单个温度大于等于900度出现过12次
    *   3.如果10秒后10个平均温度大于等于88度,就认为开了
    */ 
    WATER_BOIL_SCENE_10,


    /* 场景11:    热水盖凉盖/盖热盖煮开(热水不盖盖煮开,场景4已经兼容了)
    *   1.10个平均温度大于50度后
    *   2.平稳链表中,存在持续时间少于32ticks的node
    *   3.存在上升速率大于3000  // 凉盖
    *   4.温度大于70度后,存在上升速率大于等于2000   // 热盖       ET70修改：70改80  改回去了
    *   5.温度大于80度后,存在上升速率大于等于1500 // 保底   最后还有场景4保底   ET70修改：80改90  改回去了
    */ 
    WATER_BOIL_SCENE_11,


    /* 场景12:    全透明的玻璃锅
    *   1. 10个温度的平均温度大于85度后，出现过2次跳升
    *   2. 等10个温度的平均温度大于第二次跳升后的温度时，就认为水开了
    */ 
    // WATER_BOIL_SCENE_12,
    /* 场景12:    全透明的玻璃锅
    *   1. 当前是玻璃锅
    *   2. 每10个温度计算的平均数中，最近4个前后挨着的两个比，都小于1.5度    （玻璃锅特征是明显，但是水开不明显，这里使用温度平稳来判断）
    */ 
    WATER_BOIL_SCENE_12,



    // 冷水热盖子 实验发现冷水热盖子与冷水冷盖子可以统一起来 (只是通过把盖子判断的场景POT_COVER_SCENE_1温度限制给放开了,具体看该场景处的注释)  
    // 冷水热盖子:60度盖子,开始烧后,温度先慢慢降到53度,然后开始上升 但还是使用冷水冷盖子

    SCENE_END,
};

typedef struct sig_water_boil
{
    enum CA_SIGNALS type;
} sig_water_boil_t;

void reset_water_boil_value();
void water_boil_add_signals();
int water_boil_signals_worker(ca_signal_list_t *p);
void reset_water_boil_signals();

#ifdef __cplusplus
}
#endif
#endif  // _WATER_BOIL_H_