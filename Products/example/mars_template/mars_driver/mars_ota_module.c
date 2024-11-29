#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>

#include "aos/kernel.h"
#include "aos/kv.h"
#include "hal/soc/flash.h"

#include "ota_hal_plat.h"
#include "ota_hal_os.h"

#include "iot_import.h"

#include "mars_uartmsg.h"

#include "../mars_devfunc/mars_devmgr.h"

#define OTA_CRC16 "ota_file_crc16"

static unsigned int _offset = 0;

#define OTA_MODULE_TIMEOUT  (30 * 60 * 1000)

#define TMP_BUF_LEN 1024
#define OTA_RESEND  5

static aos_task_t task_reboot_otamodule;

static uint16_t ota_seq = 0;

void mars_otamodule_timeoutSet(void);

#if 0
static aos_task_t task_moduleota_process;
#endif
static bool g_moduleota_taskstatus = false;

static int ota_image_check(uint32_t image_size)
{
    uint32_t filelen, flashaddr, len = 0, left;
    uint8_t md5_recv[16];
    uint8_t md5_calc[16];
    ota_md5_context ctx;
    uint8_t *tmpbuf;

    tmpbuf = (uint8_t *)aos_malloc(TMP_BUF_LEN);

    filelen = image_size - 16;
    flashaddr = filelen;
    hal_flash_read(HAL_PARTITION_OTA_TEMP, &flashaddr, (uint8_t *)md5_recv, 16);

    ota_md5_init(&ctx);
    ota_md5_starts(&ctx);
    flashaddr = 0;
    left = filelen;

    while (left > 0)
    {
        if (left > TMP_BUF_LEN)
        {
            len = TMP_BUF_LEN;
        }
        else
        {
            len = left;
        }
        left -= len;
        hal_flash_read(HAL_PARTITION_OTA_TEMP, &flashaddr, (uint8_t *)tmpbuf, len);
        ota_md5_update(&ctx, (uint8_t *)tmpbuf, len);
    }

    ota_md5_finish(&ctx, md5_calc);
    ota_md5_free(&ctx);

    aos_free(tmpbuf);
    if (memcmp(md5_calc, md5_recv, 16) != 0)
    {
        LOGI("mars", "RX:   %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x ",
                  md5_recv[0], md5_recv[1], md5_recv[2], md5_recv[3],
                  md5_recv[4], md5_recv[5], md5_recv[6], md5_recv[7],
                  md5_recv[8], md5_recv[9], md5_recv[10], md5_recv[11],
                  md5_recv[12], md5_recv[13], md5_recv[14], md5_recv[15]);
        LOGI("mars", "Need: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x ",
                  md5_calc[0], md5_calc[1], md5_calc[2], md5_calc[3],
                  md5_calc[4], md5_calc[5], md5_calc[6], md5_calc[7],
                  md5_calc[8], md5_calc[9], md5_calc[10], md5_calc[11],
                  md5_calc[12], md5_calc[13], md5_calc[14], md5_calc[15]);
        LOGI("mars", "user crc check fail");
        // goto exit;
    }

    LOGI("mars", "OTA image md5 check success");

    return 0;
exit:
    return -1;
}

/*
static int ota_image_crc(uint32_t image_size)
{
    uint16_t crcout;
    uint32_t filelen, flashaddr, len = 0, left;
    ota_crc16_ctx ctx = {0};
    uint8_t *tmpbuf;

    tmpbuf = (uint8_t *)aos_malloc(TMP_BUF_LEN);

    filelen = image_size - 16;

    ota_crc16_init(&ctx);

    flashaddr = 0;
    left = filelen;

    while (left > 0)
    {
        if (left > TMP_BUF_LEN)
        {
            len = TMP_BUF_LEN;
        }
        else
        {
            len = left;
        }
        left -= len;
        hal_flash_read(HAL_PARTITION_OTA_TEMP, &flashaddr, (uint8_t *)tmpbuf, len);
        ota_crc16_update(&ctx, tmpbuf, len);
    }

    ota_crc16_final(&ctx, &crcout);
    aos_free(tmpbuf);

    return crcout;
}*/


uint16_t crc16_maxim_multi(unsigned char *ptr, int len, uint16_t before_crc)
{
    unsigned int i;
    unsigned short crc = before_crc;
    while(len--)
    {
        crc ^= *ptr++;
        for (i = 0; i < 8; ++i)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc = (crc >> 1);
        }
    }
    return crc;         //这里不取反
}

static int ota_image_crc(uint32_t image_size)
{
    uint16_t crcout = 0x0000;
    uint32_t filelen, flashaddr, len = 0, left;
    uint8_t *tmpbuf;
    tmpbuf = (uint8_t *)aos_malloc(TMP_BUF_LEN);

    //filelen = image_size - 16;
    filelen = image_size;
    flashaddr = 0;
    left = filelen;

    while (left > 0)
    {
        if (left > TMP_BUF_LEN)         //1024为单位校验
        {
            len = TMP_BUF_LEN;
        }
        else
        {
            len = left;
        }
        
        hal_flash_read(HAL_PARTITION_OTA_TEMP, &flashaddr, (uint8_t *)tmpbuf, len);
        crcout = crc16_maxim_multi((char *)tmpbuf, len, crcout);
        left -= len;
    }
    printf("finish crc,flash addr is:%d\r\n",flashaddr);
    aos_free(tmpbuf);
    mprintf_d("in function crc out is:%02x\r\n",crcout);
    return ~crcout;
}

static bool ota_new_start = false;
static int ota_init(void *something)
{
    int ret = 0;
    ota_boot_param_t *param = (ota_boot_param_t *)something;
    _offset = param->off_bp;

    hal_logic_partition_t *part_info = hal_flash_get_info(HAL_PARTITION_OTA_TEMP);

    LOGI("mars", "ota init: off_bp:%d part:%d len:%d", param->off_bp, HAL_PARTITION_OTA_TEMP, param->len);
    if (part_info->partition_length < param->len || param->len == 0)
    {
        LOGI("mars", "ota init: length failed!!!");
        ret = OTA_PARAM_FAIL;
        return ret;
    }

    LOGI("mars", "ota init: 开始擦除OTA分区......");
    ota_new_start = true;
    if (param->off_bp == 0)
    {
        int ret = 0;
        int len = part_info->partition_length;
        int offset = _offset;
        while (len > 0)
        {
            ret = hal_flash_erase(HAL_PARTITION_OTA_TEMP, offset, 4096);
            if (ret != 0)
            {
                LOGI("mars", "ota init: erase failed!!!");
                ret = OTA_INIT_FAIL;
                return ret;
            }
            offset += 4096;
            len -= 4096;
            aos_msleep(1);
        }
    }
    LOGI("mars", "ota init: 擦除完成");
    return ret;
}

static int ota_write(int *off, char *in_buf, int in_buf_len)
{
    int32_t ret = hal_flash_write(HAL_PARTITION_OTA_TEMP, (uint32_t *)&_offset, (uint8_t *)in_buf, in_buf_len);
    if (ret == 0)
        LOGI("mars", "ota write: write success (本次写入=%d 累计写入=%d)", in_buf_len, _offset);
    else
        LOGI("mars", "ota write: write failed!!!");
    return ret;
}

static int ota_read(int *off, char *out_buf, int out_buf_len)
{
    return hal_flash_read(HAL_PARTITION_OTA_TEMP, (uint32_t *)off, (uint8_t *)out_buf, out_buf_len);
}

typedef struct
{
    uint32_t dst_adr;
    uint32_t src_adr;
    uint32_t siz;
    uint16_t crc;
} ota_hdr_t;

static int hal_ota_switch(uint32_t ota_len, uint16_t ota_crc)
{
    uint32_t addr = 0;
    ota_hdr_t ota_hdr = {
        .dst_adr = 0xA000,
        .src_adr = 0x100000,
        .siz = ota_len,
        .crc = ota_crc,
    };

    hal_flash_write(HAL_PARTITION_PARAMETER_1, &addr, (uint8_t *)&ota_hdr, sizeof(ota_hdr));

    return 0;
}

typedef enum{
    M_MODULEOTA_STATRT = 0,
    M_MODULEOTA_ING,
    M_MODULEOTA_SUCCESS,
    M_MODULEOTA_FAILD,
    M_MODULEOTA_GETVERSION,
    M_MODULEOTA_NULL,
}m_moduleota_step_en;

typedef struct{
    uint8_t loop_time;
    m_moduleota_step_en ota_step;
    uint8_t ota_module;
    uint16_t pack_size;
    uint16_t pack_sum;
    uint16_t pack_cnt;
    uint16_t img_crc;
    uint32_t img_size;
}m_moduleota_st;

static m_moduleota_st g_ota_status = {.loop_time = 200, 
                                    .ota_step = M_MODULEOTA_STATRT,
                                    .pack_size = 256,
                                    .pack_sum = 0,
                                    .pack_cnt = 0};

void reboot_otamodule()
{
    aos_msleep(10 * 1000);
    HAL_Reboot();
    aos_task_exit(0);
}

void do_otamodule_reboot()
{
    aos_task_new_ext(&task_reboot_otamodule, "reboot otamodule", reboot_otamodule, NULL, 512, 0);
}

#if 1
uint32_t g_flashaddr = 0, g_len = 0, g_left = 0;
void moduleota_step(void)
{
    uint8_t ota_buf[512] = {0};
    uint16_t buf_len = 0;

    if (g_ota_status.ota_module == 0)    
    {
        ota_seq = uart_get_seq_mid();
        LOGI("mars", "腰部ota: 获取seq=%d", ota_seq);
    }
    else
    {
//        ota_seq = uart_get_seq_head();
        ota_seq = uart_get_seq_mid();
        LOGI("mars", "头部ota: 获取seq=%d", ota_seq);
    }
    //LOGI("mars", "moduleota_step, step(%d), packsum(%d), packcnt(%d)!!!!!!!!!!!!!!!!!", g_ota_status.ota_step, g_ota_status.pack_sum, g_ota_status.pack_cnt);
    if (g_moduleota_taskstatus){
        switch (g_ota_status.ota_step){
            case (M_MODULEOTA_STATRT):
            {
                if (g_ota_status.img_size % g_ota_status.pack_size){
                    g_ota_status.pack_sum = (g_ota_status.img_size / g_ota_status.pack_size) + 1;
                }else{
                    g_ota_status.pack_sum = (g_ota_status.img_size / g_ota_status.pack_size);
                }
                //msg send
                ota_buf[buf_len++] = 0xF9;
                ota_buf[buf_len++] = g_ota_status.ota_module;          //ota target
                ota_buf[buf_len++] = 0x00;          //ota req
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.img_size >> 24);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.img_size >> 16);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.img_size >> 8);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.img_size );
                //version
                char version_str[10] = {0};
                int len = 10;
                HAL_Kv_Get("ota_m_version_cloud", version_str, &len);
                if (len){   //"0.1"
                    version_str[1] = 0;
                    uint8_t ver_h = (uint8_t)atoi(version_str);
                    uint8_t ver_l = (uint8_t)atoi(version_str+2);
                    ota_buf[buf_len++] = (uint8_t)(ver_h << 4) | (ver_l & 0x0F); 
                }else{
                    ota_buf[buf_len++] = 0x00;   
                }
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.img_crc >> 8);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.img_crc);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_size >> 8);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_size);
                Mars_uartmsg_send(0x0A, ota_seq, ota_buf, buf_len, OTA_RESEND);
                LOGI("mars", "模块ota: 发送请求帧 (总大小=%d 总包数=%d 单次发送长度=%d)", g_ota_status.img_size, g_ota_status.pack_sum, g_ota_status.pack_size);
                g_ota_status.pack_cnt = 0;
                g_left = g_ota_status.img_size;
                // g_ota_status.ota_step = M_MODULEOTA_ING;
                g_flashaddr = 0;
                break;
            }
            case (M_MODULEOTA_ING):
            {
                ota_buf[buf_len++] = 0xF9;
                ota_buf[buf_len++] = g_ota_status.ota_module;          //ota target
                ota_buf[buf_len++] = 0x01;          //ota data
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_sum >> 8);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_sum );
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_cnt >> 8);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_cnt);
                if (g_left > 0)
                {
                    LOGI("mars", "OTA file left size (%d)", g_left);
                    g_len = (g_left > g_ota_status.pack_size)? g_ota_status.pack_size : g_left;                    
                    ota_buf[buf_len++] = (uint8_t)(g_len >> 8);  //(uint8_t)(g_ota_status.pack_size >> 8);
                    ota_buf[buf_len++] = (uint8_t)(g_len & 0xFF); //(uint8_t)(g_ota_status.pack_size);
                    int32_t readRes = hal_flash_read(HAL_PARTITION_OTA_TEMP, &g_flashaddr, (uint8_t *)ota_buf+buf_len, g_len);
                    if (readRes != 0)
                    {
                        LOGI("mars", "模块ota: 读取flash failed!!! (ret=%d len=%d)", readRes, g_len);
                    }
                    buf_len += g_len;
                    aos_msleep(250);
                    Mars_uartmsg_send(0x0A, ota_seq, ota_buf, buf_len, OTA_RESEND);                    
                    g_left  -= g_len;
                    g_ota_status.pack_cnt++;
                    LOGI("mars", "模块ota: 发送数据帧,本次发送长度=%d (当前包/总包数: %d/%d)", g_len, g_ota_status.pack_cnt, g_ota_status.pack_sum);
                }
                else
                {
                    g_ota_status.ota_step = M_MODULEOTA_SUCCESS;
                    //结束升级
                    buf_len = 0;
                    ota_buf[buf_len++] = 0xF9;
                    ota_buf[buf_len++] = g_ota_status.ota_module;                      //ota target
                    ota_buf[buf_len++] = 0x04;                      //ota data
                    Mars_uartmsg_send(0x0A, ota_seq, ota_buf, buf_len, OTA_RESEND);
                    LOGI("mars", "模块ota: 发送结束帧 (M_MODULEOTA_SUCCESS 1)");
                    g_moduleota_taskstatus = false;
                    // do_otamodule_reboot();
                }

                break;
            }
            case (M_MODULEOTA_SUCCESS):
            {
                //结束升级
                ota_buf[buf_len++] = 0xF9;
                ota_buf[buf_len++] = g_ota_status.ota_module;                      //ota target
                ota_buf[buf_len++] = 0x04;                      //ota data
                Mars_uartmsg_send(0x0A, ota_seq, ota_buf, buf_len, OTA_RESEND);
                LOGI("mars", "模块ota: 发送结束帧 (M_MODULEOTA_SUCCESS 2)");
                g_moduleota_taskstatus = false;
                // HAL_Reboot();
                // do_otamodule_reboot();
                break;
            }
            case (M_MODULEOTA_FAILD):
            {
                //结束升级
                ota_buf[buf_len++] = 0xF9;
                ota_buf[buf_len++] = g_ota_status.ota_module;          //ota target
                ota_buf[buf_len++] = 0x04;          //ota data
                Mars_uartmsg_send(0x0A, ota_seq, ota_buf, buf_len, OTA_RESEND);
                LOGI("mars", "模块ota: 发送结束帧 (M_MODULEOTA_FAILD)");
                g_moduleota_taskstatus = false;
                extern void mars_ota_status(int status);
                int err = OTA_REBOOT_FAIL;
                mars_ota_status(err);
                // HAL_Reboot();
                // do_otamodule_reboot();
                break;
            }
            default:
                break;
        }
    }
}
#else
uint32_t g_flashaddr = 0, g_len = 0, g_left = 0;
void moduleota_step(void)
{
    int ret = 0;
    // int buffer_len = 0;
    // char module_name_value[MODULE_NAME_LEN + 1] = {0};
    // char module_version_value[MODULE_VERSION_LEN + 1] = {0};

    // buffer_len = MODULE_NAME_LEN;
    // ret = HAL_Kv_Get("ota_m_name_cloud",module_name_value, &buffer_len);
    // buffer_len = MODULE_VERSION_LEN;
    // ret |= HAL_Kv_Get("ota_m_version_cloud",module_version_value, &buffer_len);

    g_moduleota_taskstatus = true;

    uint8_t ota_buf[1024] = {0};
    uint16_t buf_len = 0;

    g_ota_status.pack_size = 256;
    while (g_moduleota_taskstatus)
    {
        buf_len = 0;
        LOGI("mars", "moduleota_step, step(%d), packsum(%d), packcnt(%d)!!!!!!!!!!!!!!!!!", 
            g_ota_status.ota_step, g_ota_status.pack_sum, g_ota_status.pack_cnt);
        //串口阻塞发送，等待应答
        switch (g_ota_status.ota_step){
            case (M_MODULEOTA_STATRT):
            {
                if (g_ota_status.img_size % g_ota_status.pack_size){
                    g_ota_status.pack_sum = (g_ota_status.img_size / g_ota_status.pack_size) + 1;
                }else{
                    g_ota_status.pack_sum = (g_ota_status.img_size / g_ota_status.pack_size);
                }
                //msg send
                ota_buf[buf_len++] = 0xF9;
                ota_buf[buf_len++] = g_ota_status.ota_module;          //ota target
                ota_buf[buf_len++] = 0x00;          //ota req
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.img_size >> 24);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.img_size >> 16);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.img_size >> 8);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.img_size );
                //version
#if 1
                char version_str[10] = {0};
                int len = 10;
                HAL_Kv_Get("ota_m_version_cloud", version_str, &len);
                if (len){   //"0.1"
                    version_str[1] = 0;
                    uint8_t ver_h = (uint8_t)atoi(version_str);
                    uint8_t ver_l = (uint8_t)atoi(version_str+2);
                    ota_buf[buf_len++] = (uint8_t)(ver_h << 4) | (ver_l & 0x0F); 
                }else{
                    ota_buf[buf_len++] = 0x00;   
                }
#else
                ota_buf[buf_len++] = 0x00; 
#endif
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.img_crc >> 8);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.img_crc);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_size >> 8);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_size);
                Mars_uartmsg_send(0x0A, uart_get_seq(), ota_buf, buf_len, 1);
                g_ota_status.pack_cnt = 0;
                g_left = g_ota_status.img_size;
                g_ota_status.ota_step = M_MODULEOTA_ING;
                g_flashaddr = 0;
                break;
            }
            case (M_MODULEOTA_ING):
            {
                ota_buf[buf_len++] = 0xF9;
                ota_buf[buf_len++] = g_ota_status.ota_module;          //ota target
                ota_buf[buf_len++] = 0x01;          //ota data
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_sum >> 8);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_sum );
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_cnt >> 8);
                ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_cnt);
                if (g_left > 0){
                    LOG("OTA file left size (%d)\r\n", g_left);
                    g_len = g_left>g_ota_status.pack_size?g_ota_status.pack_size:g_left;
                    g_left -= g_len;
                    ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_size >> 8);
                    ota_buf[buf_len++] = (uint8_t)(g_ota_status.pack_size);
                    hal_flash_read(HAL_PARTITION_OTA_TEMP, &g_flashaddr, (uint8_t *)ota_buf+buf_len, g_len);
                    buf_len += g_len;
                    Mars_uartmsg_send(0x0A, uart_get_seq(), ota_buf, buf_len, 1);
                    g_ota_status.pack_cnt++;
                }else{
                    g_ota_status.ota_step = M_MODULEOTA_SUCCESS;
                    g_moduleota_taskstatus = false;
                    //结束升级
                    buf_len = 0;
                    ota_buf[buf_len++] = 0xF9;
                    ota_buf[buf_len++] = 0x01;                      //ota target
                    ota_buf[buf_len++] = 0x04;                      //ota data
                    Mars_uartmsg_send(0x0A, uart_get_seq(), ota_buf, buf_len, 1);
                    do_otamodule_reboot();
                }
                break;
            }
            case (M_MODULEOTA_SUCCESS):
            {
                g_moduleota_taskstatus = false;
                //结束升级
                ota_buf[buf_len++] = 0xF9;
                ota_buf[buf_len++] = g_ota_status.ota_module;                      //ota target
                ota_buf[buf_len++] = 0x04;                      //ota data
                Mars_uartmsg_send(0x0A, uart_get_seq(), ota_buf, buf_len, 1);
                // HAL_Reboot();
                do_otamodule_reboot();
                break;
            }
            case (M_MODULEOTA_FAILD):
            {
                g_moduleota_taskstatus = true;
                //结束升级
                ota_buf[buf_len++] = 0xF9;
                ota_buf[buf_len++] = g_ota_status.ota_module;          //ota target
                ota_buf[buf_len++] = 0x04;          //ota data
                Mars_uartmsg_send(0x0A, uart_get_seq(), ota_buf, buf_len, 1);
                // HAL_Reboot();
                do_otamodule_reboot();
                break;
            }
            default:
                break;
        }

        aos_msleep(g_ota_status.loop_time);
    }    

    aos_task_exit(0);
}
#endif

void mars_otamodule_rsp(uint16_t seq, uint8_t *buf, uint16_t len)
{
    if (ota_new_start)
    {
        LOGW("mars", "新的OTA包正在下载中,忽略当前应答");
        return;
    }

    char hex_buf[128] = {0};
    for (int a=0;a<len;++a){
        char tmp[4] = {0};
        sprintf(tmp, "%02x ", buf[a]);
        strcat(hex_buf, tmp);
    }
    LOGI("mars", "ota ack[%d][%d]: %s", seq, len, hex_buf);
    
    if (ota_seq != seq){
        return;
    }

    uint8_t ack = buf[1];
    if (ack == 0){  //成功
        if(M_MODULEOTA_ING == g_ota_status.ota_step){
            moduleota_step();
        }else if(M_MODULEOTA_STATRT == g_ota_status.ota_step){
            g_ota_status.ota_step = M_MODULEOTA_ING;
            moduleota_step();
        }
    }else{
        if(M_MODULEOTA_STATRT == g_ota_status.ota_step && 0x04 == buf[2]){
            uint16_t pack_size = ((uint16_t)buf[3] << 8) | (uint16_t)buf[4];
            LOGI("mars", "mars_otamodule_rsp, reset size(%d)", pack_size);
            g_ota_status.pack_size = pack_size;
            g_ota_status.ota_step = M_MODULEOTA_STATRT;
            moduleota_step();
        }else{
            //ota失败
            g_ota_status.ota_step = M_MODULEOTA_FAILD;
            moduleota_step();
        }
    }
    return;
}

static int ota_boot(void *something)
{
    int ret = 0;
    ota_boot_param_t *param = (ota_boot_param_t *)something;
    if (param == NULL)
    {
        LOGI("mars", "ota_boot: failed!!!");
        ret = OTA_REBOOT_FAIL;
        return ret;
    }
    if (param->res_type == OTA_FINISH)
    {
        if (param->upg_flag == OTA_DIFF)
        {

        }
        else
        {
            LOGI("mars", "ota_image_check 1111111111");
            if (ota_image_check(param->len) != 0)
            {
                LOGI("mars", "ota_boot: failed!!! (ota_image_check failed!!!)");
                return -1;
            }
            param->crc = ota_image_crc(param->len);
            LOGI("mars", "模块ota: 固件下载完成, 准备开始传输数据到模块......");
            LOGI("mars","crc16 out is:%02X",param->crc);
            ota_new_start = false;

            // 开始串口传输OTA数据
            g_ota_status.img_crc = param->crc;
            // g_ota_status.img_size = param->len - 16;
            g_ota_status.img_size = param->len;         //校验位也进行传输
            g_ota_status.pack_size = 128;
            //msg send
            g_ota_status.ota_step = M_MODULEOTA_STATRT;
            g_moduleota_taskstatus = true;
            moduleota_step();
            //mars_otamodule_timeoutSet();
        }

    }
    return ret;
}

static int ota_rollback(void *something)
{
    return 0;
}

const char *aos_get_app_version(void);
static const char *ota_get_version(unsigned char dev_type)
{
    return "0.0.1";
}

static int ota_init_1(void *something)
{
    ota_init(something);
}

static int ota_write_1(int *off, char *in_buf, int in_buf_len)
{
    ota_write(off, in_buf, in_buf_len);
}

static int ota_read_1(int *off, char *out_buf, int out_buf_len)
{
    ota_read(off, out_buf, out_buf_len);
}

static int ota_boot_1(void *something)
{
    LOGI("mars", "ota_boot_1 is run");
    g_ota_status.ota_module = 0x00; //控制板
    ota_boot(something);
}

static int ota_rollback_1(void *something)
{
    ota_rollback(something);
}

static const char *ota_get_version_1(unsigned char dev_type)
{
    static char version_str[10] = {0};
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    
    memset(version_str, 0x00, sizeof(version_str));
    sprintf(version_str, "%d.%d", \
            mars_template_ctx->status.ElcSWVersion>>4, \
            mars_template_ctx->status.ElcSWVersion & 0x0F);
    return version_str;
}

ota_hal_module_t mars_ota_module_1 = {          //电源板
    .init = ota_init_1,
    .write = ota_write_1,
    .read = ota_read_1,
    .boot = ota_boot_1,
    .rollback = ota_rollback_1,
    .version = ota_get_version_1,
};

static int ota_init_2(void *something)
{
    ota_init(something);
}
static int ota_write_2(int *off, char *in_buf, int in_buf_len)
{
    ota_write(off, in_buf, in_buf_len);
}

static int ota_read_2(int *off, char *out_buf, int out_buf_len)
{
    ota_read(off, out_buf, out_buf_len);
}

static int ota_boot_2(void *something)
{
    LOGI("mars", "ota_boot_2 is run");
    g_ota_status.ota_module = 0x02; //显示板升级
    ota_boot(something);
}

static int ota_rollback_2(void *something)
{
    ota_rollback(something);
}

static const char *ota_get_version_2(unsigned char dev_type)
{
    static char version_str[10] = {0};
    mars_template_ctx_t *mars_template_ctx = mars_dm_get_ctx();
    
    memset(version_str, 0x00, sizeof(version_str));
    sprintf(version_str, "%d.%d", \
            mars_template_ctx->status.PwrSWVersion>>4, \
            mars_template_ctx->status.PwrSWVersion & 0x0F);
    return version_str;
}

ota_hal_module_t mars_ota_module_2 = {          //头部显示板
    .init = ota_init_2,
    .write = ota_write_2,
    .read = ota_read_2,
    .boot = ota_boot_2,
    .rollback = ota_rollback_2,
    .version = ota_get_version_2,
};

static aos_timer_t mars_otamodule_timeout;
static void mars_otamodule_callback(void *arg1, void *arg2)
{
    g_ota_status.ota_step = M_MODULEOTA_FAILD;
    moduleota_step();
}

void mars_otamodule_timeoutSet(void)
{
    aos_timer_new_ext(&mars_otamodule_timeout, mars_otamodule_callback, NULL, OTA_MODULE_TIMEOUT, 0, 1);
}
