#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__

struct ringbuffer_s
{
    uint8_t  *data;
    uint32_t count;
    uint32_t first;
    uint32_t last;
};

int ringbuffer_create();


#endif
