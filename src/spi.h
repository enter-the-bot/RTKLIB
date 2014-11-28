#ifndef __SPI_H__
#define __SPI_H__

#include <stdint.h>
#include "ringbuffer.h"

struct spi_s
{
   int fd;
   uint16_t mode;
   struct ringbuffer_s *inputb;
   struct ringbuffer_s *outputb;
};

typedef struct spi_s spi_t;

spi_t *openspi(const char *path, int mode, char *msg);
int  writespi (spi_t *device, unsigned char *buff, int n, char *msg);
int  readspi  (spi_t *device, unsigned char *buff, int n, char *msg);
int  statespi (spi_t *device);
void closespi (spi_t *device);

#endif
