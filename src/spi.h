#ifndef __SPI_H__
#define __SPI_H__

struct spi_s
{
   int fd;   
};

typedef struct spi_s spi_t;

spi_t *openspi(const char *path, int mode, char *msg);
int  writespi (spi_t *spi, unsigned char *buff, int n, char *msg);
int  readspi  (spi_t *spi, unsigned char *buff, int n, char *msg);
int  statespi (spi_t *spi);
void closespi (spi_t *spi);

#endif
