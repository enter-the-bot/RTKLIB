#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__

struct ring_buffer_s
{
    uint8_t  *data;
    uint32_t count;
    uint32_t first;
    uint32_t last;
};


#endif
