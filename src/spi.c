#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "spi.h"
#include "rtklib.h"

#define SPI_IO_DEBUG 1
#ifdef  SPI_IO_DEBUG
#define debug(fmt, args ...)  do {fprintf(stderr,"%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ## args); } while(0)
#else
#define debug(fmt, args ...)
#endif

spi_t *openspi(const char *path, int mode, char *msg)
{
    spi_t *device;
    int flags;
    
    device = malloc(sizeof(spi_t));

    if (device == NULL) {
        perror("malloc: ");
        return NULL;
    }

    if ((mode & STR_MODE_R) &&
         (mode & STR_MODE_W)) {
        flags = O_RDWR;
    } else if (mode & STR_MODE_R) {
        flags = O_RDONLY;
    } else if (mode & STR_MODE_W) {
        flags = O_WRONLY;
    }

    device->fd = open(path, flags);            

    if (device->fd < 0) {
        perror("open: ");
        goto errout_with_free;
    }

    /* hardcoded spi mode for Ublox */
    device->mode = SPI_MODE_0;

    return device;

errout_with_free:
    free(device);
    return NULL;
}

int  writespi (spi_t *device, unsigned char *buff, int n, char *msg)
{
    return 0;
}

int  readspi  (spi_t *device, unsigned char *buff, int n, char *msg)
{
    return 0;
}

int  statespi (spi_t *device)
{
    return 0;
}

void closespi (spi_t *device)
{
}
