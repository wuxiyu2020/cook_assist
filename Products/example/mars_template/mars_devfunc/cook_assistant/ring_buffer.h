#ifndef _RING_BUFFER_H_
#define _RING_BUFFER_H_

typedef struct
{
    void *buffer;       //存放的数据
    int element_size;   //单个数据元素的大小
    int read_offset;    //读取地址相对buffer的偏移量
    int write_offset;   //写入地址相对buffer的偏移量
    int valid_size;     // buffer的有效size
    int total_size;     // buffer的数据元素数量

} ring_buffer_t;

void ring_buffer_init(ring_buffer_t *ring_buffer, int buff_size, int element_size);
void ring_buffer_deinit(ring_buffer_t *ring_buffer);
int ring_buffer_write(ring_buffer_t *ring_buffer, void *buff, int size);
int ring_buffer_push(ring_buffer_t *ring_buffer, void *buff);
int ring_buffer_peek(ring_buffer_t *ring_buffer, void *buff, int size);
int ring_buffer_read(ring_buffer_t *ring_buffer, void *buff, int size);
int ring_buffer_is_full(ring_buffer_t *ring_buffer);
int ring_buffer_valid_size(ring_buffer_t *ring_buffer);
void ring_buffer_clear(ring_buffer_t *ring_buffer);

#endif