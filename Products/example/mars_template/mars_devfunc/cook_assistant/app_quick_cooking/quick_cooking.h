#ifndef _QUICK_COOKING_H_
#define _QUICK_COOKING_H_

#ifdef __cplusplus
extern "C"
{
#endif

// 全局变量
extern unsigned short g_quick_cooking_app_is_open;
extern bool have_water_boil;
extern bool count_down_lock;
extern bool temp_have_first_down;
extern unsigned int temp_have_first_down_tick;
extern unsigned int last_turn_big_fire_tick;
extern unsigned int last_turn_small_fire_tick;

typedef struct cook_model cook_model_t;
extern cook_model_t *g_cook_model;

// 烹饪类型
enum COOK_TYPE
{
    COOK_TYPE_START= 0,
    SOFT_BOILED_EGG,                // 溏心蛋
    BOIL_DUMPLINGS,                 // 煮饺子
    BLANCHING,                      // 焯水（肉）
    COOK_TYPE_END
};

// 烹饪过程中事件
enum COOK_EVENT
{
    COOK_EVENT_NONE = 0,  // 无事件
    WATER_FIRST_BOIL,  // 水第一次开
    TEMP_NOT_DOWN_AFTER_30_SECONDS_LOOP,  // 水开后每30秒没有检测到温度下降
    TEMP_HAVE_DOWN_THEN_WATER_BOIL,  // 温度下降后，又检测到水开了
    COOK_END,  // 结束烹饪
};

// 烹饪模型
typedef struct cook_model
{
    unsigned short seted_time;         // 设置关火倒计时时间  单位秒
    unsigned short remaining_time;     // 倒计时剩余时间  单位秒
    enum COOK_TYPE cook_type;          // 烹饪类型
    unsigned short total_tick;         // 烹饪模式经历了多少个温度  1秒4个温度
    unsigned short safety_time;        // 保底时间，如果始终没判断出来，在这个时间结束后，就关火
    enum COOK_EVENT cook_event;        // 烹饪过程中的事件
} cook_model_t;

typedef void (*cook_funcs)();
void reset_quick_cooking_value();
void count_down();
void count_down_reset();
int quick_cooking_switch_for_test(unsigned short switch_flag);

// 溏心蛋
void func_soft_boil_egg();
void soft_boil_egg_connect();
void reset_soft_boil_egg();
extern bool soft_boil_egg_have_connect;

// 焯水
void func_blanching();
void blanching_connect();
void reset_blanching();
extern bool blanching_have_connect;

// 煮饺子
void func_boil_dumplings();
void boil_dumplings_connect();
void reset_boil_dumplings();
extern unsigned short small_fire_time;
extern unsigned short big_fire_time; 
extern bool boil_dumplings_have_connect;

#ifdef __cplusplus
}
#endif
#endif  // _QUICK_COOKING_H_