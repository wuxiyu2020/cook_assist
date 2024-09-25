#ifndef __LIB_IRT102M_DATA_CALIBRA_H
#define __LIB_IRT102M_DATA_CALIBRA_H

#include <stdint.h>
#include "drv_sensor_irt102m.h"

#define HAL_PARTITION HAL_PARTITION_PARAMETER_3

#define  SENSOR_MAX_TEMP 400
#define  SENSOR_MIN_TEMP 0

#define  SENSOR_TEMPOINT_ADJUST_VALUE0	370
#define  SENSOR_TEMPOINT_ADJUST_VALUE1	1000
#define  SENSOR_TEMPOINT_ADJUST_VALUE2	2500
#define  SENSOR_TEMPOINT1_OFFSET		200  
#define  SENSOR_TEMPOINT2_OFFSET		500
#define  SENSOR_TEMPOINT3_OFFSET		1000

#define  SENSOR_DATA_NUM 5
//#define  NULL 0

#define s32 signed int
#define u32 unsigned int
#define s16 signed short
#define u16 unsigned short
#define s8  signed char
#define u8  unsigned char


#define SYS_MODE_WORK 0
#define SYS_MODE_TEST 1
#define SYS_MODE_SELFTEST 2

//宏定义
//#define SOFT_NONE_STRUCTRUE	//无结构件
//#define SOFT_SLOPE_COATEDGLASS//斜面镀膜基片
//#define SOFT_SQUARE_COATEDGLASS//方形镀膜基片
#define SOFT_SQUARE_SILICONGLASS//方形硅基片

typedef enum
{
	MODE_WORK = 0,  //正常工作模式
	MODE_TEST,      //标定模式
	MODE_SELFTEST,  //查看数据模式
}SysModeEnum;//系统工作模式设置

typedef enum
{
	TARGET_TEMP_37 = 0x01,//目标温度37摄氏度
	TARGET_TEMP_100,//目标温度100摄氏度
	TARGET_TEMP_250,//目标温度250摄氏度
}TargetTemperEnum;//标定时目标温度设置

typedef enum
{
	COLLECT_OK = 1,//数据采集完成
	COLLECTING,//数据采集中
	COLLECT_FAIL,//数据采集失败
}DATA_COLLECT_STATUS;//温度标定时数据获取状态

typedef struct
{
	TargetTemperEnum TargetTemper;//当前标定的温度、
	DATA_COLLECT_STATUS CollectStatus[2];//0:表示通道0,1:表示通道1
}ChannelCollectStatus;

#pragma pack(1)
typedef struct
{
	s16		siObject_Temp1;		//计算后的最终目标温度
	s16		siNTC_Temp1;	//NTC温度
	u16		uiDist1;		//距离
	
	s16		siObject_Temp2;		//计算后的最终目标温度
	s16		siNTC_Temp2;	//NTC温度
	u16		uiDist2;		//距离	
	
	
    s16		uiReserved; 			//保留, 设为0		
	u16 	ucDev_Mode;
} SENSOR;

typedef struct
{
	float fCh1TempBuf[SENSOR_DATA_NUM];	
	float fCh1TempBuf_ave;		
	float fCh2TempBuf[SENSOR_DATA_NUM];	
	float fCh2TempBuf_ave;
	
	float fCh1ObjTemp37;
	float fCh1ObjOffset37;
	float fCh1ObjTemp100;
	float fCh1ObjOffset100;
	float fCh1ObjTemp250;
	float fCh1ObjOffset250;
	
	float fCh2ObjTemp37;
	float fCh2ObjOffset37;
	float fCh2ObjTemp100;
	float fCh2ObjOffset100;
	float fCh2ObjTemp250;
	float fCh2ObjOffset250;
	
	u8     ucAdj1Line1PointFlag;//完成校准点标志 
	u8     ucAdj1Line2PointFlag;//完成校准点标志
	u8     ucAdj2Line1PointFlag;//完成校准点标志 
	u8     ucAdj2Line2PointFlag;//完成校准点标志 
	u16     ucCur_ChTemp;	//当前测试黑体温度及通道

}OBJDATA;		

typedef struct
{
	float   fCh1ObjTemp100;             //通道1标定值
	float   fCh2ObjTemp100;             //通道2标定值
	float   fAdjCh1_Line_A1;			//曲线1斜率A 通道1
	float   fAdjCh1_Line_B1;			//曲线1截距B
	float   fAdjCh1_Line_A2;			//曲线2斜率A
	float   fAdjCh1_Line_B2;			//曲线2截距B
	
	float   fAdjCh2_Line_A1;			//曲线1斜率A 通道2
	float   fAdjCh2_Line_B1;			//曲线1截距B
	float   fAdjCh2_Line_A2;			//曲线2斜率A
	float   fAdjCh2_Line_B2;			//曲线2截距B
	
	unsigned char AdjCh1ParaFlag;      //曲线参数标志位 0：未校准 0x01:曲线1校准 0x02:曲线2校准 
	unsigned char AdjCh2ParaFlag;      //曲线参数标志位 0：未校准 0x01:曲线1校准 0x02:曲线2校准 
    u8		ulReserved; 			//保留, 设为0	
} SENSOR_PARA_S; 					// total  bytes


#pragma pack()
enum ENUM_OBJTEMP
{
	ENUM_CURCHTEMP_NULL = 0x00,
	ENUM_CURRENTTEMP37 = 0x01,
	ENUM_CURRENTTEMP100 = 0x02,
	ENUM_CURRENTTEMP250 = 0x03,
	
	ENUM_CURCH1TEMP37 = 0x11,
	ENUM_CURCH1TEMP100 = 0x12,
	ENUM_CURCH1TEMP250 = 0x14,
	
	ENUM_CURCH1L1SAMPLEEND =0x13,
	ENUM_CURCH1L2SAMPLEEND =0x16,
	
	ENUM_CURCH2TEMP37 = 0x21,
	ENUM_CURCH2TEMP100 = 0x22,
	ENUM_CURCH2TEMP250 = 0x24,

	ENUM_CURCH2L1SAMPLEEND =0x23,
	ENUM_CURCH2L2SAMPLEEND =0x26,
};
extern OBJDATA  g_fOBJDATA;						   //二次标定结构体
extern SENSOR_PARA_S g_sGas_Sensor_ParaS;
extern ChannelCollectStatus ChanCollectStatus;
extern SENSOR g_sGas_Sensor;


FUNC_RET_TYPE Lib_SetSysWorkMode(SysModeEnum SysMode,TargetTemperEnum TargetTemper,uint8_t* FlashSaveFlag);//设置系统工作模式
void Lib_Irt102mAlgoProc(uint8_t* FlashSaveFlag);//二次标定函数,调用该函数前先从FLASH读取变量g_sGas_Sensor_ParaS,比如:hal_flash_read(HAL_PARTITION,&off,(unsigned char*)&g_sGas_Sensor_ParaS,sizeof(SENSOR_PARA_S));
float Sensor_AmbTemp_Adj_Proc(u8 ch ,float l_siObj_T);//环境温度补偿函数
float Sensor_Get_Obj_Temp_Adj_Proc(u8 ch ,float l_siObj_T);//温度曲线拟合函数
void Lib_VersionInfoPrintf(void);



#endif
