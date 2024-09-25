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

//�궨��
//#define SOFT_NONE_STRUCTRUE	//�޽ṹ��
//#define SOFT_SLOPE_COATEDGLASS//б���Ĥ��Ƭ
//#define SOFT_SQUARE_COATEDGLASS//���ζ�Ĥ��Ƭ
#define SOFT_SQUARE_SILICONGLASS//���ι��Ƭ

typedef enum
{
	MODE_WORK = 0,  //��������ģʽ
	MODE_TEST,      //�궨ģʽ
	MODE_SELFTEST,  //�鿴����ģʽ
}SysModeEnum;//ϵͳ����ģʽ����

typedef enum
{
	TARGET_TEMP_37 = 0x01,//Ŀ���¶�37���϶�
	TARGET_TEMP_100,//Ŀ���¶�100���϶�
	TARGET_TEMP_250,//Ŀ���¶�250���϶�
}TargetTemperEnum;//�궨ʱĿ���¶�����

typedef enum
{
	COLLECT_OK = 1,//���ݲɼ����
	COLLECTING,//���ݲɼ���
	COLLECT_FAIL,//���ݲɼ�ʧ��
}DATA_COLLECT_STATUS;//�¶ȱ궨ʱ���ݻ�ȡ״̬

typedef struct
{
	TargetTemperEnum TargetTemper;//��ǰ�궨���¶ȡ�
	DATA_COLLECT_STATUS CollectStatus[2];//0:��ʾͨ��0,1:��ʾͨ��1
}ChannelCollectStatus;

#pragma pack(1)
typedef struct
{
	s16		siObject_Temp1;		//����������Ŀ���¶�
	s16		siNTC_Temp1;	//NTC�¶�
	u16		uiDist1;		//����
	
	s16		siObject_Temp2;		//����������Ŀ���¶�
	s16		siNTC_Temp2;	//NTC�¶�
	u16		uiDist2;		//����	
	
	
    s16		uiReserved; 			//����, ��Ϊ0		
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
	
	u8     ucAdj1Line1PointFlag;//���У׼���־ 
	u8     ucAdj1Line2PointFlag;//���У׼���־
	u8     ucAdj2Line1PointFlag;//���У׼���־ 
	u8     ucAdj2Line2PointFlag;//���У׼���־ 
	u16     ucCur_ChTemp;	//��ǰ���Ժ����¶ȼ�ͨ��

}OBJDATA;		

typedef struct
{
	float   fCh1ObjTemp100;             //ͨ��1�궨ֵ
	float   fCh2ObjTemp100;             //ͨ��2�궨ֵ
	float   fAdjCh1_Line_A1;			//����1б��A ͨ��1
	float   fAdjCh1_Line_B1;			//����1�ؾ�B
	float   fAdjCh1_Line_A2;			//����2б��A
	float   fAdjCh1_Line_B2;			//����2�ؾ�B
	
	float   fAdjCh2_Line_A1;			//����1б��A ͨ��2
	float   fAdjCh2_Line_B1;			//����1�ؾ�B
	float   fAdjCh2_Line_A2;			//����2б��A
	float   fAdjCh2_Line_B2;			//����2�ؾ�B
	
	unsigned char AdjCh1ParaFlag;      //���߲�����־λ 0��δУ׼ 0x01:����1У׼ 0x02:����2У׼ 
	unsigned char AdjCh2ParaFlag;      //���߲�����־λ 0��δУ׼ 0x01:����1У׼ 0x02:����2У׼ 
    u8		ulReserved; 			//����, ��Ϊ0	
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
extern OBJDATA  g_fOBJDATA;						   //���α궨�ṹ��
extern SENSOR_PARA_S g_sGas_Sensor_ParaS;
extern ChannelCollectStatus ChanCollectStatus;
extern SENSOR g_sGas_Sensor;


FUNC_RET_TYPE Lib_SetSysWorkMode(SysModeEnum SysMode,TargetTemperEnum TargetTemper,uint8_t* FlashSaveFlag);//����ϵͳ����ģʽ
void Lib_Irt102mAlgoProc(uint8_t* FlashSaveFlag);//���α궨����,���øú���ǰ�ȴ�FLASH��ȡ����g_sGas_Sensor_ParaS,����:hal_flash_read(HAL_PARTITION,&off,(unsigned char*)&g_sGas_Sensor_ParaS,sizeof(SENSOR_PARA_S));
float Sensor_AmbTemp_Adj_Proc(u8 ch ,float l_siObj_T);//�����¶Ȳ�������
float Sensor_Get_Obj_Temp_Adj_Proc(u8 ch ,float l_siObj_T);//�¶�������Ϻ���
void Lib_VersionInfoPrintf(void);



#endif
