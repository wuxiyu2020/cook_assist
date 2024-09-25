#include <stdio.h>
#include "../ca_apps.h"

#define log_module_name "water_boil"
#define log_debug(...) LOGD(log_module_name, ##__VA_ARGS__)
#define log_info(...)  LOGI(log_module_name, ##__VA_ARGS__)
#define log_warn(...)  LOGW(log_module_name, ##__VA_ARGS__)
#define log_error(...) LOGE(log_module_name, ##__VA_ARGS__)
#define log_fatal(...) LOGF(log_module_name, ##__VA_ARGS__)

sig_water_boil_t *g_sig_water_boil = NULL;

void reset_water_boil_signals()
{
    if (g_sig_water_boil != NULL)
    {
        free(g_sig_water_boil);
        g_sig_water_boil = NULL;
    }
}

/************************************************************* start信号触发逻辑　*************************************************************/
static bool is_water_boil(sig_water_boil_t *signal)
{
    if (g_water_is_boil)
        return true;
    return false;
}

/************************************************************* 信号触发逻辑end　*************************************************************/
// 遍历信号链表 针对信号做相应计算
int water_boil_signals_worker(ca_signal_list_t *p)  // 返回int类型,0：说明信号类型属于该函数所在的业务模块；1：说明不是该模块的信号
{
    log_debug("%s", __func__);
    switch (p->type)
    {
        case SIG_WATER_BOIL:
            is_water_boil(p->sig);
            return 0;
        default:
            return 1;
    }
}


/************************************************************* stat信号定义　*************************************************************/
static void water_boil_add_signal_water_boil()
{
    if (g_sig_water_boil == NULL)
    {
        g_sig_water_boil = malloc(sizeof(sig_water_boil_t));
        if (g_sig_water_boil == NULL)
        {
            log_debug("g_sig_water_boil信号初始化失败");
            return;
        }
        g_sig_water_boil->type = SIG_WATER_BOIL;

        CA_ADD_SIGNAL(g_sig_water_boil);
    }
}
/************************************************************* 信号定义end　*************************************************************/

void water_boil_add_signals()
{
    log_debug("%s", __func__);
    if (!g_water_boil_app_is_open)
        return;
    
    water_boil_add_signal_water_boil();
}