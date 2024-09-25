/*
 * @filePath: Do not edit
 * @LastEditTime: 2024-06-17 16:28:08
 * @LastEditors: Zhouxc
 */
#ifndef _COOK_WRAPPER_H_
#define _COOK_WRAPPER_H_
#ifdef __cplusplus
extern "C"
{
#endif

    #include "ca_apps.h"
    #include "fsyd.h"

// #define SIMULATION
#ifdef SIMULATION
    int get_cur_state(char *msg, enum INPUT_DIR input_dir);
    int get_hood_gear(char *msg);
    int get_fire_gear(char *msg, enum INPUT_DIR input_dir);
#endif
    // 本地对外函数
    void prepare_gear_change_task();
    void cook_assistant_init(enum INPUT_DIR input_dir);                                                        // 烹饪助手初始化
    void cook_assistant_input(enum INPUT_DIR input_dir, unsigned short temp, unsigned short environment_temp); // 温度输入

    void set_ignition_switch(unsigned char ignition_switch, enum INPUT_DIR input_dir); // 点火开关控制
    void set_hood_min_gear(unsigned char gear);
    void recv_ecb_gear(unsigned char gear);                           // 风机状态
    void recv_ecb_fire(unsigned char fire, enum INPUT_DIR input_dir); // 大小火状态

    void register_close_fire_cb(int (*cb)(enum INPUT_DIR input_dir));                // 关火回调
    void register_hood_gear_cb(int (*cb)(const int gear));                           // 风机回调
    void register_fire_gear_cb(int (*cb)(const int gear, enum INPUT_DIR input_dir)); // 大小火回调

    void register_thread_lock_cb(int (*cb)());
    void register_thread_unlock_cb(int (*cb)());
    void register_cook_assist_remind_cb(int (*cb)(int));
    //云端对外函数
    void set_work_mode(unsigned char mode);
    void set_smart_smoke_switch(unsigned char smart_smoke_switch);
    void set_pan_fire_delayofftime(unsigned int delayoff_time);     //add by zhoubw
    void set_pan_fire_switch(unsigned char pan_fire_switch, enum INPUT_DIR input_dir);
    void set_pan_fire_close_delay_tick(unsigned short tick, enum INPUT_DIR input_dir);

    void set_temp_control_switch(unsigned char temp_control_switch, enum INPUT_DIR input_dir);
    void set_temp_control_target_temp(unsigned short temp, enum INPUT_DIR input_dir);

    void register_work_mode_cb(int (*cb)(const unsigned char));
    void register_smart_smoke_switch_cb(int (*cb)(const unsigned char));
    void register_pan_fire_switch_cb(int (*cb)(const unsigned char, enum INPUT_DIR));
    void register_temp_control_switch_cb(int (*cb)(const unsigned char, enum INPUT_DIR));
    void register_temp_control_target_temp_cb(int (*cb)(const unsigned short, enum INPUT_DIR));
    void register_oil_temp_cb(void (*cb)(const unsigned short, enum INPUT_DIR));

    //void set_dry_fire_switch(unsigned char fire_dry_switch,enum INPUT_DIR input_dir,enum DRYBURN_USER dryburn_user_category);
    void set_dry_fire_switch(unsigned char fire_dry_switch, enum INPUT_DIR input_dir, uint8_t dryburn_user_category);
    void exit_dryburn_status(enum INPUT_DIR input_dir, uint8_t reason);
    void set_fire_status(enum INPUT_DIR input_dir,uint8_t status);
#ifdef __cplusplus
}
#endif
#endif
