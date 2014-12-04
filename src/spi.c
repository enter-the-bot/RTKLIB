#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#include <linux/spi/spidev.h>
#include <linux/limits.h>

#include "spi.h"
#include "rtklib.h"

#define SPI_IO_DEBUG 0
#ifdef  SPI_IO_DEBUG
#define debug(fmt, args ...)  do {fprintf(stderr,"%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ## args); } while(0)
#else
#define debug(fmt, args ...)
#endif

#define BUFFER_LENGTH 200
#define SPI_DEFAULT_SPEED 245000

struct spi_dev_s
{
   struct port_dev_s port;
   int fd;
   uint16_t mode;
   uint32_t speed;
};

static int  spi_open  (struct port_dev_s *port, const char *path, int mode, char *msg);
static int  spi_write (struct port_dev_s *device, const unsigned char *buff, int n, char *msg);
static int  spi_read  (struct port_dev_s *device, unsigned char *buff, int n, char *msg);
static int  spi_state (struct port_dev_s *device);
static void spi_close (struct port_dev_s *device);

struct port_dev_s *spi_initialize()
{
    struct spi_dev_s *port;

    port = malloc(sizeof(struct spi_dev_s));
    assert(port != NULL);

    port->port.ops->open =  spi_open;
    port->port.ops->write = spi_write;
    port->port.ops->read  = spi_read;
    port->port.ops->state = spi_state;
    port->port.ops->close = spi_close;

    return (struct port_dev_s *) port;
}

void spi_deinitialize(struct port_dev_s *port)
{
    struct spi_dev_s *device;
    device = (struct spi_dev_s *) port;

    free(device);
}

static int spi_parse_path(const char *path, char *device_path, uint32_t *speed, uint16_t *mode)
{
    char *p;
    size_t path_length;

    /* Check if any additional parameters have been provided */
    if ((p = strchr(path,':')) == NULL) {

        /* If there's no additional parameters, use default */
        strcpy(device_path, path);
        *speed = SPI_DEFAULT_SPEED;

        return 1;
    }

    sscanf(p + 1, "%" SCNu32 ":%" SCNu16, speed, mode); /* p points to the delimiter. */
    debug("%" SCNu32 ":%" SCNu16, *speed, *mode); 

    switch (*mode) {
        case 0:
            *mode = SPI_MODE_0;
            break;
        case 1:
            *mode = SPI_MODE_1;
            break;
        case 2:
            *mode = SPI_MODE_2;
            break;
        case 3:
            *mode = SPI_MODE_3;
            break;
        default:
            fprintf(stderr, "Wrong mode for SPI\n");
            return 0;
    }

    path_length = p - path;
    strncpy(device_path, path, path_length);
    device_path[path_length] = '\0'; /* strncpy doesn't null-terminate the string */

    return 1;
}

int spi_open(struct port_dev_s *port, const char *path, int mode, char *msg)
{
    struct spi_dev_s *device = (struct spi_dev_s *) port;

    int flags = O_RDWR;
    uint32_t speed = 0;
    uint16_t spi_mode = SPI_MODE_0;
    char device_path[PATH_MAX];

    if (!spi_parse_path(path, device_path, &speed, &spi_mode)) {
        return 0;
    }

    if ((mode & STR_MODE_R) &&
         (mode & STR_MODE_W)) {
        flags = O_RDWR;
    } else if (mode & STR_MODE_R) {
        flags = O_RDONLY;
    } else if (mode & STR_MODE_W) {
        flags = O_WRONLY;
    }

    debug("%s", device_path);
    device->fd = open(device_path, flags);

    if (device->fd < 0) {
        perror("open: ");
        goto errout;
    }

    /* hardcoded spi mode for Ublox */
    device->mode = mode;
    device->speed = speed;

    return 1;

errout:
    return 0;
}


static const uint8_t io_buffer[BUFFER_LENGTH];

int  spi_write (struct port_dev_s *port, const unsigned char *buff, int n, char *msg)
{
    int rc;
    struct spi_ioc_transfer transaction = {0};

    struct spi_dev_s *device = (struct spi_dev_s *) port;

    if (n > BUFFER_LENGTH) {
        n = BUFFER_LENGTH;
    }

    transaction.tx_buf = (unsigned long) buff;
    transaction.rx_buf = (unsigned long) NULL;
    transaction.len = n;
    transaction.speed_hz = 245000;
    transaction.bits_per_word = 8;

    rc = ioctl(device->fd, SPI_IOC_MESSAGE(1), &transaction);
    if (rc < 0) {
        debug("n: %d\n", n);
        perror("ioctl: ");
        return 0;
    }

    return n;
}

int spi_read(struct port_dev_s *port, unsigned char *buff, int n, char *msg)
{
    int rc;
    int i;
    struct spi_ioc_transfer transaction = {0};

    struct spi_dev_s *device = (struct spi_dev_s *) port;

    if (n > BUFFER_LENGTH) {
        n = BUFFER_LENGTH;
    }

    transaction.tx_buf = (unsigned long) NULL;
    transaction.rx_buf = (unsigned long) buff;
    transaction.len = n;
    transaction.speed_hz = 245000;
    transaction.bits_per_word = 8;

    rc = ioctl(device->fd, SPI_IOC_MESSAGE(1), &transaction);
    if (rc < 0) {
        debug("n: %d\n", n);
        perror("ioctl: ");
        return 0;
    }

    for (i = 0; i < n; i++) {
        fprintf(stderr, "0x%x ", buff[i]);
    }
    fprintf(stderr, "\n");

    return n;
}

int spi_state (struct port_dev_s *port)
{
    struct spi_dev_s *device = (struct spi_dev_s *) port;
    return 3;
}

void spi_close (struct port_dev_s *port)
{
    int rc;

    struct spi_dev_s *device = (struct spi_dev_s *) port;

    rc = close(device->fd);

    if (rc < 0) {
        perror("close: ");
    }
}
