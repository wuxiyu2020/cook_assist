#include "drv_sensor_irt102m.h"
//#include <tg7100c_glb.h>
#include <aos/kernel.h>
#include <math.h>
#include "../../mars_driver/mars_i2c.h"
//#include <../../../../Living_SDK/platform/mcu/tg7100c/hal_drv/tg7100c_hal/hal_software_i2c.h>

#define IIC_MARS (1)

Irt102m_SysParaConfig Irt102mParaConfig;
aos_timer_t Timer_AutoReport;
uint32_t Error_I2cNum = 0;
SENSOR_STATUS_ENUM SensorStatus[2] = {SENSOR_STATUS_NORMAL};//传感器状�?

I2C_BASE_CONFIG I2cBaseConfig = 
{
    .gpio_sda = 1,
    .gpio_scl = 2,
    .DevNum   = 2,
    .DevAddr  = {0x01, 0x02},
    // .DevNum   = 1,
    // .DevAddr  = {0x7F},
    .InitFlag = 0,
};


#define IIC_TIMEOUT     200
#define WAIT_DATA_COUNT 50

uint8_t Reg[1] = {0x00};
//IRT102m�?件驱�?
static IRT102m_Driver_Typedef IRT102mDriver;
//IRT102m 温度主动上报回调函数
static module_autoreport_cb pIRT102mAutoReportCallBack = NULL;

//I2C 配置成功变量
I2C_CONFIG_FLAG i2c_config_flg;

//函数功能:检查目标温度是否�?�算完成
int g_i_max = 0;
static FUNC_RET_TYPE Driver_WaitDataReady(uint8_t DevAddr,uint8_t TempReadyMask)
{
    uint16_t i = 0;
    uint8_t drdy[2] = {0};
    uint8_t dataReady = 0;

    for (i = 0; i < WAIT_DATA_COUNT; i++)
    {
        #if IIC_MARS
        if(mars_i2c_reg_read(DevAddr, DATA_READY, &drdy[0], 2, IIC_TIMEOUT) != 0)
		{
			break;
		}
        #else
        Driver_SoftWareI2cReadBytes(DevAddr, DATA_READY,&drdy[0], 2);
        #endif
        
        dataReady = drdy[1] | drdy[0];
        if (TempReadyMask == (dataReady & 0x0F))
        {
            break;
        }else
        {
            printf("@Driver_WaitDataReady:dataReady = 0x%x,i = %d, DevAddr = %d\r\n",dataReady,i,DevAddr);
        }
        aos_msleep(10);
    }
    if (i > g_i_max)
    {
        g_i_max = i;
    }
   
    if (i >= WAIT_DATA_COUNT)
    {
        printf(" Tobj data after DSP not ready\r\n");
        return FUNC_RET_TIMEOUT;
    }
    else
    {
        return FUNC_RET_OK;
    }
}
/*函数功能:读取IRT102m传感器数�?
  传参:
  返回:
  FUNC_RET_TYPE: 函数执�?�状�?
*/
static FUNC_RET_TYPE Driver_ReadData(uint8_t DevAddr,uint8_t RegAddr, int32_t *RawData)
{
    uint8_t to1RawData[3] = {0};
    uint32_t unsignedRawData = 0;

    #if IIC_MARS
    if(mars_i2c_reg_read(DevAddr, RegAddr, &to1RawData[0], 3, IIC_TIMEOUT) != 0)
    {
        return FUNC_RET_TIMEOUT;
    }
    #else
    Driver_SoftWareI2cReadBytes(DevAddr, RegAddr,&to1RawData[0], 3);
    #endif
   
    unsignedRawData = to1RawData[0] << 16 | to1RawData[1] << 8 | to1RawData[2];
    if (to1RawData[0] & 0x80)
    {
        *RawData = unsignedRawData - 16777216;
    }
    else {
    	*RawData = unsignedRawData;
    }

    return FUNC_RET_OK;
}
/*函数功能:读取IRT102m传感器温度数�?
  传参:
  返回:
  FUNC_RET_TYPE: 函数执�?�状�?
*/
FUNC_RET_TYPE Driver_IRT102mGetSensorData(float *vt1,float *Tobj1,float *Tamb1,float *vt2,float *Tobj2,float *Tamb2)
{
	int32_t TacalData = 0;
	int32_t TorawData = 0;
	int32_t TocalData = 0;
	int32_t toData = 0;
	uint8_t drdyClear[2] = {0xFF, 0xFF};
    uint8_t Timeout[2] = {0};
    uint8_t tdata[2] = {0};
	
	float torawvtp[2]={0};//*0.0020489;
	float tocalvtp[2]={0};//*0.0029073;		
	float tadata_t[2]={0};///(double)16384;
	float todata_t[2]={0};///(double)16384;

	int32_t l_ucLen = 0;
	
    for(uint8_t i =0;i < I2cBaseConfig.DevNum;i++)
    {
        if(SensorStatus[i] == SENSOR_STATUS_NORMAL)
        {
            if (Driver_WaitDataReady(I2cBaseConfig.DevAddr[i],TEMP_DRDY | TA_DRDY | TO1_DRDY) == 0)
            {
                if(Driver_ReadData(I2cBaseConfig.DevAddr[i],TEMP_MSB, 		&TacalData) != FUNC_RET_OK)
                {
                    Timeout[i] ++;
                    continue;
                }
                if(Driver_ReadData(I2cBaseConfig.DevAddr[i],DATA1_RAW_MSB, 	&TorawData) != FUNC_RET_OK)
                {
                    Timeout[i] ++;
                    continue;
                }
                if(Driver_ReadData(I2cBaseConfig.DevAddr[i],DATA1_CAL_MSB, 	&TocalData) != FUNC_RET_OK)
                {
                    Timeout[i] ++;
                    continue;
                }
                if(Driver_ReadData(I2cBaseConfig.DevAddr[i],DATA1_MSB, 		&toData) != FUNC_RET_OK)
                {
                    Timeout[i] ++;
                    continue;
                }
                torawvtp[i]=(float)TorawData * 0.0020489;
                tocalvtp[i]=(float)TocalData * 1000 /524288 ;
                tadata_t[i]=(float)TacalData * 1 / 16384  ;
                todata_t[i]=(float)toData * 1 / 16384;

                // printf("DevAddr:0x%x,todata_t:%d.%.03d,tadata_t:%d.%.03d,tocalvtp:%d.%.03d,torawvtp:%d.%.03d\r\n", I2cBaseConfig.DevAddr[i],(int)(todata_t[i]), (int)(fabs(todata_t[i])*1000)%1000,\
                // (int)(tadata_t[i]), (int)(fabs(tadata_t[i])*1000)%1000,(int)(tocalvtp[i]), (int)(fabs(tocalvtp[i])*1000)%1000,(int)(torawvtp[i]), (int)(fabs(torawvtp[i])*1000)%1000);
                
                //to1raw to1cal TA TO1
                #if IIC_MARS
                mars_i2c_reg_write(I2cBaseConfig.DevAddr[i], DATA_READY, &drdyClear[0], 2, IIC_TIMEOUT);
                #else
                Driver_SoftWareI2cWriteBytes(I2cBaseConfig.DevAddr[i], DATA_READY,&drdyClear[0], 2);
                #endif
                
                aos_msleep(1);//延时<1ms
            }else{
                Error_I2cNum ++;
                printf("no data!\r\n");
                IRT102mDriver.Init(&I2cBaseConfig);
            }
        }else
        {
            #if IIC_MARS
            if(mars_i2c_reg_read(I2cBaseConfig.DevAddr[i], DATA_READY, &tdata[0], 2, 5) != 0)
            {
                Timeout[i] = 1;//Sensor Fault
            }else
            {
                IRT102mDriver.Init(&I2cBaseConfig);//Sensor fault recover
                Timeout[i] = 0;//Sensor Normal
            }
            #else
            Driver_SoftWareI2cReadBytes(DevAddr, DATA_READY,&drdy[0], 2);
            #endif
        }
    }
    for(uint8_t i =0;i < I2cBaseConfig.DevNum;i++)
    {
        if(Timeout[i])
        {
            SensorStatus[i] = SENSOR_STATUS_FAULT;
        }else
        {
            SensorStatus[i] = SENSOR_STATUS_NORMAL;
        }
    }
    *vt1 	= tocalvtp[0];
    *Tobj1	= todata_t[0];
    *Tamb1	= tadata_t[0];
    *vt2    = tocalvtp[1];
    *Tobj2	= todata_t[1];
    *Tamb2	= tadata_t[1];
	return FUNC_RET_OK;
}

/*函数功能:配置IRT102m传感�?
  传参:
  返回:
  FUNC_RET_TYPE: 函数执�?�状�?
*/
static FUNC_RET_TYPE Driver_SensorConfig(uint8_t DevAddr,uint8_t RegAddr, uint8_t sysChopEn, uint8_t gain, uint8_t osr)
{
    uint8_t sensorCfgReadBack = 0;

	Reg[0] = ((sysChopEn << 7) & 0x80) | ((gain << 3) & 0x38) | (osr & 0x07);

    #if IIC_MARS
    // uint8_t test_buf[4] = {0x01, 0x02, 0x03, 0x04};
    // mars_i2c_reg_write(DevAddr, RegAddr, test_buf, 4, 1000);
    mars_i2c_reg_write(DevAddr, RegAddr, Reg, 1, IIC_TIMEOUT);
    mars_i2c_reg_read(DevAddr, RegAddr, &sensorCfgReadBack, 1, IIC_TIMEOUT);
    aos_msleep(500);
    #else
    Driver_SoftWareI2cWriteBytes(DevAddr, RegAddr,&Reg[0], 1);
    Driver_SoftWareI2cReadBytes(DevAddr, RegAddr,&sensorCfgReadBack, 1);
    #endif
    
    if (Reg[0] != sensorCfgReadBack) {
        printf("DevAddr=0x%x SensorConfig wrong!\r\n",DevAddr);
		i2c_config_flg.i2c_SensorConfig_flag[DevAddr-1] = 0;
        return FUNC_RET_FAIL;
    }
    else {
		
		i2c_config_flg.i2c_SensorConfig_flag[DevAddr-1] = 1;
        printf("DevAddr=0x%x SensorConfig Success!\r\n",DevAddr);
        return FUNC_RET_OK;
    }
}
/*函数功能:设置IRT102m工作模式
  传参:
  返回:
  FUNC_RET_TYPE: 函数执�?�状�?
*/
static FUNC_RET_TYPE Driver_ModeConfig(uint8_t DevAddr,uint8_t sleepEn, uint8_t clkMode, uint8_t modeEn, uint8_t modeSel)
{
    uint8_t cmd = 0;
    uint8_t cmdReadBack = 0;
    cmd = ((sleepEn << 5) & 0x20) | ((clkMode << 4) & 0x10) | ((modeEn << 3) & 0x08) | (modeSel & 0x07);

#if IIC_MARS
    mars_i2c_reg_write(DevAddr, CMD, &cmd, 1, IIC_TIMEOUT);
    mars_i2c_reg_read(DevAddr, CMD, &cmdReadBack, 1, IIC_TIMEOUT);
#else
    Driver_SoftWareI2cWriteBytes(DevAddr, CMD,&cmd, 1);
    Driver_SoftWareI2cReadBytes(DevAddr, CMD,&cmdReadBack, 1);
#endif

    if (cmd != cmdReadBack) {
		
		i2c_config_flg.i2c_ModeConfig_flag[DevAddr-1] = 0;
		printf("DevAddr=0x%x ModeConfig wrong!\r\n",DevAddr);
		return 1;
    }
    else {
		i2c_config_flg.i2c_ModeConfig_flag[DevAddr-1] = 1;
        printf("DevAddr=0x%x ModeConfig Success!\r\n",DevAddr);
        return 0;
    }
}
/*函数功能:IRT102m驱动初�?�化
  传参:
  SensorConfig:  I2C引脚配置
  返回:
  FUNC_RET_TYPE: 函数执�?�状�?
*/
static FUNC_RET_TYPE IRT102mInit(I2C_BASE_CONFIG* SensorConfig)
{
    uint8_t osrT16384 = 0x07;
	uint8_t osrT16384Readback = 0;
	uint8_t adcDitherEn = 0x80;
	uint8_t dac = 0x0D;
	uint8_t adcDitherReadBack = 0;

    /*uint8_t Tdata = 0x23;
    uint8_t Rdata = 0x01;*/
    uint8_t AddrData = 0;
    /*uint8_t FlashStart = 0x68;
    uint8_t FlashStatus = 0;
    uint16_t TimeoutCnt = 0;*/

    #if IIC_MARS
    mars_i2c_init();
    #else
    Driver_SoftWareI2cInit(SensorConfig);
    #endif

	memset(&i2c_config_flg,0,sizeof(i2c_config_flg));
	
    for(uint8_t i = 0;i < SensorConfig->DevNum;i++)
    {
        Driver_SensorConfig(SensorConfig->DevAddr[i],SENSOR_CHAN1_CONFIG, SYS_CHOP_DIS, GAIN_64, OSR_16384X);
    }
    for(uint8_t i = 0;i < SensorConfig->DevNum;i++)
    {
        #if IIC_MARS
        mars_i2c_reg_write(SensorConfig->DevAddr[i], SYS_CONFIG_2, &adcDitherEn, 1, IIC_TIMEOUT);
        mars_i2c_reg_write(SensorConfig->DevAddr[i], 0x97, &dac, 1, IIC_TIMEOUT);
        #else
        Driver_SoftWareI2cWriteBytes(SensorConfig->DevAddr[i], SYS_CONFIG_2,&adcDitherEn, 1);
	    Driver_SoftWareI2cWriteBytes(SensorConfig->DevAddr[i], 0x97,&dac, 1);
        #endif
    }
	for(uint8_t i = 0;i < SensorConfig->DevNum;i++)
    {
        Driver_ModeConfig(SensorConfig->DevAddr[i],SLEEP_DIS,CLK_600K,FSM_DIS,TATO1_MODE);
        Driver_ModeConfig(SensorConfig->DevAddr[i],SLEEP_DIS,CLK_600K,FSM_EN,TATO1_MODE);
    }
    // printf("@@@@@@@@@@@@@@IRT102mInit:0x%x,0x%x\r\n",SensorConfig->DevAddr[0],SensorConfig->DevAddr[1]);
}

/*函数功能:�?件定时器周期到达执�?�的回调函数
  返回:
  FUNC_RET_TYPE: 函数执�?�状�?
*/
static FUNC_RET_TYPE IRT102mEventHanlde(void *arg1, void *arg2)
{
    SENSOR_STATUS_ENUM status = SENSOR_STATUS_NORMAL;
    float TargetTemper[2] = {0};
    float EnvirTemper[2] = {0};

    if(Irt102mParaConfig.AutoReportFlag)
    {
        IRT102mQueryData(&TargetTemper[0],&EnvirTemper[0],&TargetTemper[1],&EnvirTemper[1]);
        if(pIRT102mAutoReportCallBack)
        {
            pIRT102mAutoReportCallBack(status,TargetTemper[0],EnvirTemper[0],TargetTemper[1],EnvirTemper[1]);
        }
    }
    return FUNC_RET_OK;
}

/*函数功能:设置数据�?动上报周�?
  传参:
  Period:            数据�?动上报周�?,单位(ms);0:关闭�?动上报数�?功能
  CallBack:          需要执行的回调函数当上报周期到�?
  返回:
  FUNC_RET_TYPE: 函数执�?�状�?
*/
static FUNC_RET_TYPE IRT102mSetReportPeriod(uint32_t Period,module_autoreport_cb CallBack)
{
    if(Period)
    {
        Irt102mParaConfig.AutoReportFlag = 1;
        Irt102mParaConfig.AutoReportPeriod = Period;
        pIRT102mAutoReportCallBack = CallBack;
        aos_timer_new(&Timer_AutoReport, IRT102mEventHanlde, NULL, Period, 1);
        aos_timer_start(&Timer_AutoReport);
    }else
    {
        Irt102mParaConfig.AutoReportFlag = 0;
    }
    return FUNC_RET_OK;
}
/*函数功能:关闭数据�?动上报功�?
  返回:
  FUNC_RET_TYPE: 函数执�?�状�?
*/
FUNC_RET_TYPE IRT102mStopReport(void)
{
    Irt102mParaConfig.AutoReportFlag = 0;
    aos_timer_stop(&Timer_AutoReport);
    return FUNC_RET_OK;
}
/*函数功能:IRT102m温度数据查�??
  传参:
  TargetTemper1:      指向存储�?�?1温度地址
  EnvirTemper1:       指向存储�?�?1温度地址
  TargetTemper2:      指向存储�?�?2温度地址
  EnvirTemper2:       指向存储�?�?2温度地址
  返回:
  FUNC_RET_TYPE: 函数执�?�状�?
*/
FUNC_RET_TYPE IRT102mQueryData(float* TargetTemper1,float* EnvirTemper1,float* TargetTemper2,float* EnvirTemper2)
{
    float Tdata[2] = {0};
    if(Driver_IRT102mGetSensorData(&Tdata[0],TargetTemper1,EnvirTemper1,&Tdata[1],TargetTemper2,EnvirTemper2) == FUNC_RET_OK)
    {
    }else
    {
        *TargetTemper1 = 0;
        *EnvirTemper1 = 0;
        *TargetTemper2 = 0;
        *EnvirTemper2 = 0;
        return FUNC_RET_TIMEOUT;
    }
    return FUNC_RET_OK;
}

//IRT102m驱动函数注册
static void IRT102mDriverRegister(pIRT102m_Driver_Typedef pDriver)
{
    IRT102mDriver.Init = pDriver->Init;
    IRT102mDriver.SetReportPeriod = pDriver->SetReportPeriod;
    IRT102mDriver.StopReport = pDriver->StopReport;
    IRT102mDriver.QueryData = pDriver->QueryData;
    IRT102mDriver.SysTick_ms = pDriver->SysTick_ms;
    IRT102mDriver.EventHandle = pDriver->EventHandle;
}
/*函数功能:IRT102m�?件驱动初始化
  传参:
  ReportPeriod:      数据�?动上报周�?,单位(ms);0:关闭�?动上报数�?功能
  CallBack:          需要执行的回调函数当上报周期到�?
  返回:
  FUNC_RET_TYPE: 函数执�?�状�?
*/
FUNC_RET_TYPE Driver_IRT102mInit(uint32_t ReportPeriod,module_autoreport_cb CallBack)
{
    IRT102m_Driver_Typedef Driver;
    Driver.Init = IRT102mInit;
    Driver.SetReportPeriod = IRT102mSetReportPeriod;
    Driver.StopReport = IRT102mStopReport;
    Driver.QueryData = IRT102mQueryData;
    // Driver.SysTick_ms = aos_now_ms;
    Driver.EventHandle = IRT102mEventHanlde;

    //IRT102m驱动函数注册,完成注册即可正常使用IRT102m相关函数调用
    IRT102mDriverRegister(&Driver);

    IRT102mDriver.Init(&I2cBaseConfig);
    IRT102mDriver.SetReportPeriod(ReportPeriod,CallBack);
    return FUNC_RET_OK;
}
