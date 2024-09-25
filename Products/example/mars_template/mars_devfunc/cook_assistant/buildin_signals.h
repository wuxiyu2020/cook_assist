#ifndef _BUILDIN_SIGNALS_H_
#define _BUILDIN_SIGNALS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "ca_signals_lib.h"

/* *************************buildin 信号配置************************* 
 * 不允许改这里面的值，有其他需求，需要在对应业务模块新增相关信号        
 * 命名规则： 每N个温度，后面就下划线N                            */
// 每1个温度算一次平均值，存10个  即保存连续的20个（最近5秒）温度数据
#define BUILDIN_AVG_INTERVAL_1    1
#define BUILDIN_AVG_ARR_SEIZ_1    20

// 每10个温度算一次平均值，存10个  即每隔2.5秒计算一次平值，计算10个，总共25秒的数据参与其中
#define BUILDIN_AVG_INTERVAL_10    10
#define BUILDIN_AVG_ARR_SEIZ_10    10

// 每4个温度算一次平均值，存10个
#define BUILDIN_AVG_INTERVAL_4    4
#define BUILDIN_AVG_ARR_SEIZ_4    10

// // 用每4个温度计算的平均值数组，判断温度在上升，规则个数
// #define BUILDIN_AVG_ARR4_COMPARE_TEMP_RISE_RULE_SIZE    11
// 用每10个温度计算的平均值数组，判断温度在上升，规则个数
#define BUILDIN_AVG_ARR10_COMPARE_TEMP_RISE_RULE_SIZE    11

// 用每4个温度计算的平均值数组，判断温度在下降，规则个数
#define BUILDIN_AVG_ARR4_COMPARE_TEMP_DOWN_RULE_SIZE    11

// 用每1个温度计算的平均值数组，判断温度在平稳，规则个数
#define BUILDIN_AVG_ARR1_COMPARE_TEMP_GENTAL_RULE_SIZE  6

// 用每1个温度计算的平均值数组，判断温度跳升，规则个数
#define BUILDIN_AVG_ARR1_COMPARE_TEMP_JUMP_RISE_RULE_SIZE  5

// 用每1个温度计算的平均值数组，判断温度跳降，规则个数
#define BUILDIN_AVG_ARR1_COMPARE_TEMP_JUMP_DOWN_RULE_SIZE  5

// 用每1个温度计算的平均值数组，判断温度大幅跳动，规则个数
#define BUILDIN_AVG_ARR1_COMPARE_TEMP_SHAKE_RULE_SIZE  10
/* *************************buildin 信号配置************************* */

void reset_buildin_signals();
void buildin_app();

#ifdef __cplusplus
}
#endif
#endif  // _BUILDIN_SIGNALS_H_