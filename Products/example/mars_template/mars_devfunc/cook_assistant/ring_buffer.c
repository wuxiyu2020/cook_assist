/*
 * @Date: 2023-02-27 16:58:40
 * @LastEditors: Zhouxc
 * @LastEditTime: 2023-05-12 17:32:54
 * @FilePath: /now/Products/example/mars_template/mars_devfunc/cook_assistant/ring_buffer.c
 * @Description: Do not edit
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ring_buffer.h"
#include "ca_apps.h"

#define log_module_name "fsyd"
#define log_debug(fmt, ...) LOGD(log_module_name, fmt, ##__VA_ARGS__)
#define log_info(fmt, ...)  LOGI(log_module_name, fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...)  LOGW(log_module_name, fmt, ##__VA_ARGS__)
#define log_error(fmt, ...) LOGE(log_module_name, fmt, ##__VA_ARGS__)
#define log_fatal(fmt, ...) LOGF(log_module_name, fmt, ##__VA_ARGS__)

/**
 1. 初始化ring_buffer
 2. malloc开辟传入的buff_size大小的空间存放buffer
 3. read_offset write_offset valid_size均置为０
*/
void ring_buffer_init(ring_buffer_t *ring_buffer, int buff_size, int element_size)
{
#ifdef PRODUCTION
    ring_buffer->buffer = aos_malloc(buff_size * element_size);
#else
    ring_buffer->buffer = malloc(buff_size * element_size);
#endif
    memset(ring_buffer->buffer, 0, buff_size * element_size);
    ring_buffer->element_size = element_size;
    ring_buffer->read_offset = 0;
    ring_buffer->write_offset = 0;
    ring_buffer->valid_size = 0;
    ring_buffer->total_size = buff_size;
}

/**
 *释放ring_buffer
 */
void ring_buffer_deinit(ring_buffer_t *ring_buffer)
{
    if (ring_buffer->buffer != NULL)
    {
#ifdef PRODUCTION
        aos_free(ring_buffer->buffer);
#else
        free(ring_buffer->buffer);
#endif
    }
    // memset(ring_buffer, 0, sizeof(ring_buffer_t));
}

/**
 * buff:需要写入的数据的地址
 * size:需要写入的数据的大小
 */
int ring_buffer_write(ring_buffer_t *ring_buffer, void *buff, int size)
{
    if (size == 0)
        return 0;
    int write_offset = ring_buffer->write_offset;
    int total_size = ring_buffer->total_size;
    int element_size = ring_buffer->element_size;
    int first_write_size = 0;

    if (size + write_offset <= total_size) // ring_buffer->buffer的后段未写入的空间不小于size
    {
        memcpy(ring_buffer->buffer + write_offset * element_size, buff, size * element_size);
    }
    else // ring_buffer->buffer的后段未写入的空间小于size,这时候需要先在后面写入一部分，然后返回头部，从前面接着写入
    {
        first_write_size = total_size - write_offset;           //剩余可写长度
        //从后写入可用空间
        memcpy(ring_buffer->buffer + write_offset * element_size, buff, first_write_size * element_size);
        
        //从前写入覆盖原数据
        memcpy(ring_buffer->buffer, buff + first_write_size * element_size, (size - first_write_size) * element_size);
    }

    //重置写入偏移地址
    ring_buffer->write_offset += size;
    ring_buffer->write_offset %= total_size;

    if (ring_buffer->valid_size + size > total_size) //
    {
        log_debug("total_size:%d valid_size:%d size:%d", ring_buffer->total_size, ring_buffer->valid_size, size);
        ring_buffer->valid_size = total_size;
        ring_buffer->read_offset = ring_buffer->write_offset;
    }
    else
    {
        ring_buffer->valid_size += size;
    }
    
    return size;
}

int ring_buffer_push(ring_buffer_t *ring_buffer, void *buff)
{
    return ring_buffer_write(ring_buffer, buff, 1);
}

//读取数据到buff中
int ring_buffer_peek(ring_buffer_t *ring_buffer, void *buff, int size)
{
    if (ring_buffer->valid_size == 0)
        return 0;
    int read_offset = ring_buffer->read_offset;
    int total_size = ring_buffer->total_size;
    int element_size = ring_buffer->element_size;
    int first_read_size = 0;

    if (size > ring_buffer->valid_size)
    {
        log_debug("valid size < read size");
        log_debug("valid size:%d read size:%d", ring_buffer->valid_size, size);
        size = ring_buffer->valid_size;
    }

    if (total_size - read_offset >= size)
    {
        memcpy(buff, ring_buffer->buffer + read_offset * element_size, size * element_size);
    }
    else
    {
        first_read_size = total_size - read_offset;
        memcpy(buff, ring_buffer->buffer + read_offset * element_size, first_read_size * element_size);
        memcpy(buff + first_read_size * element_size, ring_buffer->buffer, (size - first_read_size) * element_size);
    }

    return size;
}


int ring_buffer_read(ring_buffer_t *ring_buffer, void *buff, int size)
{
    if (ring_buffer->valid_size == 0)
        return 0;
    int total_size = ring_buffer->total_size;
    size = ring_buffer_peek(ring_buffer, buff, size);

    ring_buffer->read_offset += size;
    ring_buffer->read_offset %= total_size;
    ring_buffer->valid_size -= size;
    return size;
}

int ring_buffer_is_full(ring_buffer_t *ring_buffer)
{
    return ring_buffer->valid_size == ring_buffer->total_size;
}

int ring_buffer_valid_size(ring_buffer_t *ring_buffer)
{
    return ring_buffer->valid_size;
}

void ring_buffer_clear(ring_buffer_t *ring_buffer)
{
    ring_buffer->read_offset = 0;
    ring_buffer->write_offset = 0;
    ring_buffer->valid_size = 0;
}
