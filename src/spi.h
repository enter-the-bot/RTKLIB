#ifndef __SPI_H__
#define __SPI_H__

#include <stdint.h>

struct spi_s
{
   int fd;   
   uint16_t mode;
};

typedef struct spi_s spi_t;

spi_t *openspi(const char *path, int mode, char *msg);
int  writespi (spi_t *spi, unsigned char *buff, int n, char *msg);
int  readspi  (spi_t *spi, unsigned char *buff, int n, char *msg);
int  statespi (spi_t *spi);
void closespi (spi_t *spi);

#endif