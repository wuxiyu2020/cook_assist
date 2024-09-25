#ifndef DRV_SENSOR_IRT102M_H
#define DRV_SENSOR_IRT102M_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
// #include "hal_software_i2c.h"

#define SOFT_RESET	        0x00
#define DATA_READY	        0x02
#define RAW_DATA_READY	    0x03
#define ERR_CODE	        0x04
#define DATA1_MSB	        0x10   //Object temp out Tobj after DSP and IIR filter
#define DATA1_CSB	        0x11
#define DATA1_LSB	        0x12
#define TEMP_MSB        	0x16   //Ambient temp out
#define TEMP_CSB	        0x17
#define TEMP_LSB	        0x18
#define DATA1_CAL_MSB	    0x19   //Channel 1 To1 data after calibration
#define DATA1_CAL_CSB	    0x1A
#define DATA1_CAL_LSB	    0x1B
#define DATA2_CAL_MSB	    0x1C
#define DATA2_CAL_CSB	    0x1D
#define DATA2_CAL_LSB	    0x1E
#define DATA1_RAW_MSB	    0x22   //Channel 1 To1 raw data before calibration
#define DATA1_RAW_CSB	    0x23
#define DATA1_RAW_LSB	    0x24
#define DATA2_RAW_MSB	    0x25
#define DATA2_RAW_CSB	    0x26
#define DATA2_RAW_LSB	    0x27
#define TEMP_RAW_MSB	    0x28
#define TEMP_RAW_CSB	    0x29
#define TEMP_RAW_LSB	    0x2A

#define CMD                 0x30
#define SLP_TIME            0x31
#define BLOW_START          0x40
#define BLOW_CTRL           0x41
#define	PDIN_FROM_REG       0x42

#define	ENG_MODE_ACCESS     0x78
#define	TEST_MODE_1         0x79
#define	TEST_MODE_2         0x7A
#define	TEST_MODE_3         0x7B
#define	TEST_MODE_4         0x7C
#define	ENG_OTP_1           0x80
#define	ENG_OTP_2           0x81
#define	ENG_OTP_3           0x82
#define	ENG_OTP_4           0x83
#define	CAL_T_1             0x84
#define	CAL_T_2             0x85
#define	CAL_T_3             0x86
#define	ENG_OTP_8           0x87
#define	ENG_OTP_9           0x88
#define	ENG_OTP_10          0x89
#define	ENG_OTP_11          0x8A
#define	ENG_OTP_12          0x8B
#define	ENG_OTP_13          0x8C
#define	ENG_OTP_14          0x8D
#define	ENG_OTP_15          0x8E
#define	ENG_OTP_16          0x8F
#define	ID0	                0x90
#define	ID1	                0x91
#define	Chip_Address	    0x92
#define	SYS_CONFIG_1	    0x93
#define	SYS_CONFIG_2	    0x94
#define	SENSOR_CHAN1_CONFIG	0x95
#define	SENSOR_CHAN2_CONFIG	0x96
#define	BPS_CONFIG	        0x97
#define	CAL_DATA1_2	        0x98
#define	CAL_DATA1_3 	    0x99
#define	CAL_DATA1_4 	    0x9A
#define	CAL_DATA2_1	        0x9B
#define	CAL_DATA2_2	        0x9C
#define	CAL_DATA2_3 	    0x9D
#define	CAL_DATA2_4	        0x9E
#define	TOMAX_1     	    0x9F
#define	TOMAX_2     	    0xA0
#define	TOMIN_1	            0xA1
#define	TOMIN_2	            0xA2
#define	PWMCTRL     	    0xA3
#define	EMISSIVITY_1	    0xA4
#define	EMISSIVITY_2	    0xA5
#define	TCSENS_1	        0xA6
#define	TCSENS_2    	    0xA7
#define	TEMP_TH_1	        0xA8
#define	TEMP_TH_2	        0xA9
#define	TEMP_HYST_1	        0xAA
#define	TEMP_HYST_2    	    0xAB
#define	TEMP_TH_SLP_MSB	    0xAC
#define	TEMP_TH_SLP_LSB	    0xAD
#define	CH1_OFF_MSB	        0xAE
#define	CH1_OFF_LSB  	    0xAF
#define	CH1_GAIN_MSB	    0xB0
#define	CH1_GAIN_LSB	    0xB1
#define	CH1_KS_MSB  	    0xB2
#define	CH1_KSS_MSB   	    0xB3
#define	CH1_KS_LSB   	    0xB4
#define	CH2_OFF_MSB	        0xB5
#define	CH2_OFF_LSB	        0xB6
#define	CH2_GAIN_MSB	    0xB7
#define	CH2_GAIN_LSB	    0xB8
#define	CH2_KS_MSB	        0xB9
#define	CH2_KSS_MSB	        0xBA
#define	CH2_KS_LSB	        0xBB

#define	VT_DATA1_MSB	    0xC0
#define	VT_DATA1_LSB	    0xC1
#define	VT_DATA2_MSB	    0xC2
#define	VT_DATA2_LSB	    0xC3
#define	VT_DATA3_MSB	    0xC4
#define	VT_DATA3_LSB	    0xC5
#define	VT_DATA4_MSB	    0xC6
#define	VT_DATA4_LSB	    0xC7
#define	VT_DATA5_MSB	    0xC8
#define	VT_DATA5_LSB	    0xC9
#define	VT_DATA6_MSB	    0xCA
#define	VT_DATA6_LSB	    0xCB
#define	VT_DATA7_MSB	    0xCC
#define	VT_DATA7_LSB	    0xCD
#define	VT_DATA8_MSB	    0xCE
#define	VT_DATA8_LSB	    0xCF
#define	VT_DATA9_MSB	    0xD0
#define	VT_DATA9_LSB	    0xD1
#define	VT_DATA10_MSB	    0xD2
#define	VT_DATA10_LSB	    0xD3
#define	VT_DATA11_MSB	    0xD4
#define	VT_DATA11_LSB	    0xD5
#define	VT_DATA12_MSB	    0xD6
#define	VT_DATA12_LSB	    0xD7
#define	VT_DATA13_MSB	    0xD8
#define	VT_DATA13_LSB	    0xD9
#define	VT_DATA14_MSB	    0xDA
#define	VT_DATA14_LSB	    0xDB
#define	VT_DATA15_MSB	    0xDC
#define	VT_DATA15_LSB	    0xDD
#define	VT_DATA16_MSB	    0xDE
#define	VT_DATA16_LSB	    0xDF
#define	VT_DATA17_MSB	    0xE0
#define	VT_DATA17_LSB	    0xE1
#define	VT_DATA18_MSB	    0xE2
#define	VT_DATA18_LSB	    0xE3
#define	VT_DATA19_MSB	    0xE4
#define	VT_DATA19_LSB	    0xE5
#define	VT_DATA20_MSB	    0xE6
#define	VT_DATA20_LSB	    0xE7
#define	VT_DATA21_MSB	    0xE8
#define	VT_DATA21_LSB	    0xE9
#define	VT_DATA22_MSB	    0xEA
#define	VT_DATA22_LSB	    0xEB
#define	VT_DATA23_MSB	    0xEC
#define	VT_DATA23_LSB	    0xED
#define	VT_DATA24_MSB	    0xEE
#define	VT_DATA24_LSB	    0xEF
#define	VT_DATA25_MSB	    0xF0
#define	VT_DATA25_LSB	    0xF1
#define	VT_DATA26_MSB	    0xF2
#define	VT_DATA26_LSB	    0xF3
#define	VT_DATA27_MSB	    0xF4
#define	VT_DATA27_LSB	    0xF5
#define	VT_DATA28_MSB	    0xF6
#define	VT_DATA28_LSB	    0xF7
#define	VT_DATA29_MSB	    0xF8
#define	VT_DATA29_LSB	    0xF9
#define	VT_DATA30_MSB	    0xFA
#define	VT_DATA30_LSB	    0xFB
#define	VT_DATA31_MSB	    0xFC
#define	VT_DATA31_LSB	    0xFD
#define	VT_DATA32_MSB	    0xFE
#define	VT_DATA32_LSB	    0xFF

#define SLEEP_EN      0x01 << 5
#define SLEEP_DIS     0x00 << 5
#define CLK_1_2M      0x01
#define CLK_600K      0x00
#define FSM_EN        0x01
#define FSM_DIS       0x00
#define IDLE_MODE     0x00
#define TATO1_MODE    0x01
#define TATO1TO2_MODE 0x02
#define TO1TO2_MODE   0x03

#define TA_DRDY           0x01
#define TO1_DRDY          0x02
#define TO2_DRDY          0x03
#define TEMP_DRDY         0x08

#define TA_RAW_DRDY       0x10
#define TO1_RAW_DRDY      0x20
#define TO2_RAW_DRDY      0x40

#define GAIN_8        0x00
#define GAIN_12       0x01
#define GAIN_16       0x02
#define GAIN_32       0x03
#define GAIN_48       0x04
#define GAIN_64       0x05
#define GAIN_96       0x06
#define GAIN_128      0x07
#define OSR_128X      0x04
#define OSR_256X      0x05
#define OSR_512X      0x00
#define OSR_1024X     0x01
#define OSR_2048X     0x02
#define OSR_4096X     0x03
#define OSR_8192X     0x06
#define OSR_16384X    0x07
#define SYS_CHOP_EN   0x01
#define SYS_CHOP_DIS  0x00

typedef struct
{
    uint8_t     AutoReportFlag;  //开启自动上报:1,关闭自动上报:0
    uint32_t    AutoReportPeriod;//数据自动上报周期:单位(ms)
}Irt102m_SysParaConfig;

typedef enum
{
    SENSOR_STATUS_NORMAL = 0,
    SENSOR_STATUS_FAULT,
    SENSOR_STATUS_ERROR,
}SENSOR_STATUS_ENUM;

//函数执行状态
typedef enum
{
    FUNC_RET_OK = 0,   //函数执行成功
    FUNC_RET_FAIL,     //函数执行失败
    FUNC_RET_TIMEOUT,  //函数执行超时
}FUNC_RET_TYPE;

typedef void (* module_autoreport_cb)(SENSOR_STATUS_ENUM status, float TargetTemper1, float EnvirTemper1, float TargetTemper2, float EnvirTemper2);

//I2C初始化配置
typedef struct
{
    uint8_t     gpio_sda;/**< i2c pin sda */
    uint8_t     gpio_scl;/**< i2c pin scl */
    uint8_t     DevNum;//I2C总线挂载设备数目
    uint8_t     DevAddr[2];//I2C总线挂载设备地址
    uint8_t     InitFlag;//初始化完成:1,未初始化:0
}I2C_BASE_CONFIG;

//IRT102m驱动函数结构体
typedef struct
{
    FUNC_RET_TYPE ( *Init )( I2C_BASE_CONFIG* );
    FUNC_RET_TYPE ( *SetReportPeriod )( uint32_t , module_autoreport_cb );
    FUNC_RET_TYPE ( *StopReport )( void );
    FUNC_RET_TYPE ( *QueryData )( float* ,float* ,float* ,float* );
    long long ( *SysTick_ms )(void);
    FUNC_RET_TYPE ( *EventHandle)( void*, void*);
}IRT102m_Driver_Typedef,*pIRT102m_Driver_Typedef;


//I2C配置成功标志
typedef struct
{
    uint8_t     i2c_SensorConfig_flag[2]; /**探头配置标志 */
    uint8_t     i2c_ModeConfig_flag[2];   /**模式配置标志 */
}I2C_CONFIG_FLAG,*pI2C_CONFIG_FLAG;

extern SENSOR_STATUS_ENUM SensorStatus[2];

FUNC_RET_TYPE Driver_IRT102mInit(uint32_t ReportPeriod,module_autoreport_cb CallBack);
FUNC_RET_TYPE IRT102mQueryData(float* TargetTemper1,float* EnvirTemper1,float* TargetTemper2,float* EnvirTemper2);
FUNC_RET_TYPE IRT102mStopReport(void);
FUNC_RET_TYPE Driver_IRT102mGetSensorData(float *vt1,float *Tobj1,float *Tamb1,float *vt2,float *Tobj2,float *Tamb2);
/**********************
* ch表示通道号,传入1、2
* l_siObj_T原始温度，传入温度值*10
* l_uiDist默认0            在麦乐克的1.4.6版本删除了次参数！！！！
* 返回温度是0.1摄氏度
**********************/
float Sensor_Get_Obj_Temp_Adj_Proc(uint8_t ch ,float l_siObj_T);

#endif /* DRV_SENSOR_IRT102M */


