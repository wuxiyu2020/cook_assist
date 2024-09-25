#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <regex.h>
#include <stdlib.h>

#include "../cook_wrapper.h"
#include "../cloud.h"
#include "log.h"

#define log_module_name "cook_assistant_test"
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)
#define log_info(...)  LOGI(log_module_name, ##__VA_ARGS__)
#define log_warn(...)  LOGW(log_module_name, ##__VA_ARGS__)
#define log_error(...) LOGE(log_module_name, ##__VA_ARGS__)
#define log_fatal(...) LOGF(log_module_name, ##__VA_ARGS__)

#define TEST_TYPE 2  // 1: 放干烧   2:一键烹饪

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

int newfile_flag = 0;

void set_work_mode(unsigned char mode);

// 临时设置的烟机档位回调函数
static int hood_gear_change(int gear)
{
    // uint8_t buf_setmsg[10] = {0};
    // uint16_t buf_len = 0;

    // buf_setmsg[buf_len++] = prop_HoodLight;     //烟机照明
    // buf_setmsg[buf_len++] = gear;

    // Mars_uartmsg_send(cmd_set,uart_get_seq_mid(),buf_setmsg,buf_len,3);
    log_debug("%s", "*************************************************************");
    if (gear == 0)
        log_debug("%s", "** 关闭烟机照明 **");
    if (gear == 1)
        log_debug("%s", "** 打开烟机照明 **");
    log_debug("%s", "*************************************************************");
    return 0;
}

static int close_fire()
{
    log_debug("关火了");
    return 0;
}

typedef unsigned long long uint64_t;


void mars_ca_init(void)
{
    log_debug("%s", __func__);
    set_work_mode(1);
    register_hood_light_cb(hood_gear_change);          // 临时使用烟机照明表示关火
    register_close_fire_cb(close_fire);

    cook_assistant_init(INPUT_RIGHT);
}

#include <errno.h>
// 读取温度数据文件，并把每个温度输入到cook_assistant_input
void read_temp_file(char *filename, const char *filepath)
{
    // 就奇怪没把测试程序放到项目中时,这些代码运行没问题,放进来就不能运行了,cwd是空字符串,导致log_dirpath是/log_output  现直接赋值为./log_output
    // char *cwd = malloc(100);  
    // getcwd(cwd, 100);
    // char *log_dirname = "log_output";
    // char *log_dirpath = malloc(strlen(cwd) + strlen(log_dirname) + 2);
    // sprintf(log_dirpath, "%s/%s", cwd, log_dirname);
    // free(cwd);
    char *log_dirpath = "./log_output";

    // 判断log_output目录存不存在，不存在就创建
    if (access(log_dirpath, F_OK) != 0)  // 目录不存在
    {   
        int ret = mkdir(log_dirpath, 0755);
        if (ret == -1)
            log_debug("创建log_output目录失败");
    }

    // 把日志重定向到对应日志文件
    // char log_file[256];
    // sprintf(log_file, "%s/%s.log", log_dirpath, filename);
    // FILE *log_fp = freopen(log_file, "w", stdout);


    // 利用日志模块自动把日志写到文件
    char log_log_file[256];
    sprintf(log_log_file, "%s/%s1.log", log_dirpath, filename);
    FILE *log_log_fp = fopen(log_log_file, "w");
    log_set_quiet(true);  // 不输出到终端
    log_add_fp(log_log_fp, LOG_DEBUG);


    mars_ca_init();
    // 由于是模拟测试，需要把一些必要开关状态打开
    state_handle_t *state_handle = get_input_handle(1);
    state_handle->ignition_switch = 1;
    state_handle->total_tick = 0;
    state_hood_t *get_hood_handle();
    state_hood_t *state_hood = get_hood_handle();
    state_hood->gear = GEAR_ONE;  // 直接用温度数据模拟时，需要加这个，不然gentle状态判断不进去

    if (TEST_TYPE == 2)
        quick_cooking_switch_for_test(1);

    FILE *p = fopen(filepath, "r");
    if (p != NULL)
    {
        char c;
        char temp[4] = {0};  // 不能是3，最多3位数，要留一位给 \0
        int index = 0;
        while ((c = fgetc(p)) != EOF)
        {
            if (c == '\n' || c ==' ' || c == 1)
                continue;

            if (c == ',')
            {
                // int result = e4_node_input(atoi(temp));
                // if (result == 0)
                //     break;

                if(newfile_flag == 1)
                {
                    state_handle_t *state_handle = get_input_handle(1);
                    state_handle->ignition_switch_close_temp = atoi(temp)*10;
                    newfile_flag = 0;
                    log_debug("首次读取该文件，设置点火温度:%d",state_handle->ignition_switch_close_temp);
                }
                log_debug("右灶:%d",atoi(temp)* 10);

                cook_assistant_input(1, atoi(temp) * 10, 500);
                // Sleep(250);  // 注释掉这行，就不模拟真实温度数据频率，短时间快速模拟测试
                
                index = 0;
                memset(temp, 0, sizeof(temp));
                continue;
            }

            if (0 <= c - 48  && c - 48 <= 9)
            {
                temp[index] = c;
                index += 1;
            }
        }

        // 如果要跑固定数据，就把上面的while注释，用这个
        // unsigned short int temp_array[8] = {230, 450, 640, 720, 950, 1900, 1000, 450};
        // int temp_index = 0;
        // while (1)
        // {
        //     cook_assistant_input(1, temp_array[temp_index], 500);
        //     temp_index = (temp_index + 1) % 8;
        // }
        
        fclose(p);
        fclose(log_log_fp);
    }

    // 跑完一个文件，就模拟把灶和风机关掉
    state_handle->ignition_switch = 0;
    state_hood->gear = GEAR_CLOSE;
    cook_assistant_input(1, 50 * 10, 500);  // 再跑一个数据，把这些参数带进去做相应逻辑

    if (TEST_TYPE == 2)
        quick_cooking_switch_for_test(0);

    // fflush(log_fp);
    // freopen("/dev/tty", "w", stdout);
}


// 通过正则获取干烧后最近10个温度数据
void get_dryburn_temps(FILE *fp, char str[], int n)
{
    regex_t regex;
    int reti;
    char msgbuf[100];

    // 编译正则表达式
    reti = regcomp(&regex, "([0-9]{1,4}\\s){10}", REG_EXTENDED);  // regex库貌似不支持\d表示数字
    if (reti)
    {
        log_debug("编译不通过");
        return;
    }

    while ((fgets(str, n, fp)) != NULL)
    {
        reti = regexec(&regex, str, 0, NULL, 0);
        if (!reti)
        {
            log_debug("干烧后最近的10个温度为：%s", str);
            return;
        }
        else if (reti != REG_NOMATCH)
        {
            regerror(reti, &regex, msgbuf, sizeof(msgbuf));
            log_debug("字符串：%s 正则匹配错误：%s", str, msgbuf);
        }
    }
    log_debug("没有匹配到温度");
    regfree(&regex);
}


// 一行一行分析，看是否包含current status:dry_burn，包含就认为成功触发干烧了
void analyse_logfile(char *filepath)
{
    FILE *result_fp = freopen("./ganshao_test_result.txt", "a", stdout);

    char str[1024];
    char c;
    unsigned short find_flag = 0;
    int line_num = 0;
    int i = 0; 
    FILE *fp = fopen(filepath, "r");

    if (fp == NULL)
        log_debug("读取日志文件失败：%s", filepath);

    while ((c = fgetc(fp)) != EOF)
    {   
        // 本来用fgets直接读取一行，但是因为烹饪助手源代码中换行有\r\n，导致这里的line_num不准确，因此用fgetc挨个字符读取判断
        if (c != '\n' && c != '\r')
        {
            str[i] = c;
            i++;
            continue;
        }
        str[i] = '\0';

        ++line_num;
        if (strstr(str, "current status:dry_burn") != NULL)
        {
            log_debug("%-40s 检验到了干烧，第一次出现current status:dry_burn的行号为: %-8d  ", filepath, line_num);
            // 出现了后，去获取最近10个温度数据（后面一个ring_buffer_peek）
            get_dryburn_temps(fp, str, sizeof(str));
            find_flag = 1;
            break;
        }
        i = 0;
        memset(str, 0, sizeof(str));
    }
    if (find_flag == 0)
        log_debug("%-40s FAILD，没有出现current status:dry_burn", filepath);

    fclose(fp);

    fflush(result_fp);
    freopen("/dev/tty", "w", stdout);
}


// 遍历文件夹  type：1表示遍历温度数据文件夹，做模拟操作逻辑；type：2表示遍历日志文件夹，分析输出文件内容逻辑
void list_dir(const char *path, unsigned short type)
{
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char filepath[280];

    dir = opendir(path);
    if (dir == NULL)
    {
        log_debug("打不开目录：%s", path);
        return;
    }
    while ((entry = readdir(dir)) != NULL)
    {
        sprintf(filepath, "%s/%s", path, entry->d_name);  // 构造文件或者目录路径
        if(stat(filepath, &statbuf) == -1)  // 获取文件或者目录信息
        {
            log_debug("获取文件或者目录信息失败，%s", filepath);
            continue;
        }
        if(S_ISREG(statbuf.st_mode))
        {
            if (type == 1)
            {
                if (strstr(filepath, ".txt") == NULL)  // 温度数据文件都是.txt结尾的
                    continue;
                entry->d_name[strlen(entry->d_name) - strlen(".txt")] = '\0';  // 把文件名的扩展名去掉
                newfile_flag = 1;
                read_temp_file(entry->d_name, filepath);
            }
            else if (type == 2)
            {
                if (strstr(filepath, ".log") == NULL)
                    continue;
                analyse_logfile(filepath);
            }
        }
        else if (S_ISDIR(statbuf.st_mode) && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            list_dir(filepath, type);
        }
    }
    closedir(dir);
}

int main()
{
    if (TEST_TYPE == 2)
        list_dir("./data/一键烹饪/run", 1);  // 遍历所有的温度数据，模拟跑整个烹饪过程

    if (TEST_TYPE == 1)
        list_dir("./log_output", 2);  // 校验日志文件里面是否包含字符串：current status:dry_burn

    state_handle_t *state_handle = get_input_handle(1);
    free(state_handle->ring_buffer.buffer);
    return 0;
}