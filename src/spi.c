#include <stddef.h>

#include "spi.h"

spi_t *openspi(const char *path, int mode, char *msg)
{
    return NULL;
}

int  writespi (spi_t *spi, unsigned char *buff, int n, char *msg)
{
    return 0;
}

int  readspi  (spi_t *spi, unsigned char *buff, int n, char *msg)
{
    return 0;
}

int  statespi (spi_t *spi)
{
    return 0;
}

void closespi (spi_t *spi)
{
}
