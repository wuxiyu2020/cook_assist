二次标定库V1.3版本相比V1.2版本有以下区别:

1.新增结构体变量ChannelCollectStatus,如下:
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

通过该结构体变量可以获取当前标定温度点是否完成或者标定失败以及当前标定温度值


说明:当温度37摄氏度和100摄氏度都标定完成(读取CollectStatus[0]和CollectStatus[1]状态都是COLLECT_OK),调用Lib_Irt102mAlgoProc(uint8_t* FlashSaveFlag)后，传参FlashSaveFlag会变成1.说明2个通道的曲线1标定完成
当温度100摄氏度和250摄氏度都标定完成(读取CollectStatus[0]和CollectStatus[1]状态都是COLLECT_OK),调用Lib_Irt102mAlgoProc(uint8_t* FlashSaveFlag)后，传参FlashSaveFlag会变成1.说明2个通道的曲线2标定完成a


模块错误返回
1、成功							0
2、校准曲线错误					-1
3、IIC初始化失败				-2
4、传感器1IIC无应答				-3
5、传感器2IIC无应答				-4
6、传感器1初始化失败（IIC有应答，模组返回错误信息）			-5
7、传感器2初始化失败（IIC有应答，模组返回错误信息）			-6
8、传感器1温度读取失败（IIC有应答，模组返回错误信息）			-7
9、传感器2温度读取失败（IIC有应答，模组返回错误信息）			-8


## 探头数量及地址配置

I2C_BASE_CONFIG I2cBaseConfig = 
{
    .gpio_sda = 1,
    .gpio_scl = 2,
    .DevNum   = 2,						//探头数量
    .DevAddr  = {0x01, 0x02},			//探头地址
    // .DevNum   = 1,
    // .DevAddr  = {0x7F},
    .InitFlag = 0,
};

## 库文件参与编译方式
库文件名称为 Lib_Irt102mDataCalibra.a

在文件 tools/tg7100cevb.sh 中，切换 COMMON_LIBS 即可决定是否将库文件加入编译