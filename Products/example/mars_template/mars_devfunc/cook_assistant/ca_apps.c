#include "ca_apps.h"

#define log_module_name "cook_assistant"
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)
#define log_info(...)  LOGI(log_module_name, ##__VA_ARGS__)
#define log_warn(...)  LOGW(log_module_name, ##__VA_ARGS__)
#define log_error(...) LOGE(log_module_name, ##__VA_ARGS__)
#define log_fatal(...) LOGF(log_module_name, ##__VA_ARGS__)

// app注册　　每新增一个app都需要更新数组大小，并把对应的app入口函数放到apps列表里面，！有依赖关系的需要注意前后顺序！
app_enter_func enter_app_funcs[ENTER_APP_FUNC_SIZE] = {  // 这里的函数个数要与ENTER_APP_FUNC_SIZE对应起来，每次新增了记得修改ENTER_APP_FUNC_SIZE的值
    app_pot_cover, 
    app_water_boil, 
    app_quick_cooking
};

// 灶具关闭reset各个app
app_reset_func reset_app_funcs[RESET_APP_FUNC_SIZE] = {
    reset_pot_cover_value,
    reset_water_boil_value,
    reset_quick_cooking_value
};

// 外部app
int (*g_fire_gear_cb)(const int gear, enum INPUT_DIR input_dir);
int (*g_close_fire_cb)(enum INPUT_DIR input_dir);
int (*g_hood_gear_cb)(const int gear);
int (*g_hood_light_cb)(const int gear);

enum FIRE fire_state = FIRE_UNKNOW;
void ca_set_fire(enum FIRE fire, enum INPUT_DIR input_dir)
{
    log_debug("fire_state %d, fire %d", fire_state, fire);
    if (fire_state == fire)
        return;
#ifdef PRODUCTION
    g_fire_gear_cb(fire, input_dir);
#endif
    fire_state = fire;
}

void ca_close_fire(enum INPUT_DIR input_dir)
{
    log_debug("关火了%d", input_dir);
#ifdef PRODUCTION
    g_close_fire_cb(input_dir);
#endif
}

// 恢复通断阀
void reset_on_off_value()
{
    extern g_is_need_reset_on_off_value;
    // g_is_need_reset_on_off_value: 目前这个值只有在判断到移锅时，才赋值false，此时如果移锅小火功能开着，就不需要恢复通断阀，由它去控制通断阀
    if (fire_state == FIRE_SMALL && g_is_need_reset_on_off_value)
    {
        g_fire_gear_cb(FIRE_BIG, 1);
    }
}