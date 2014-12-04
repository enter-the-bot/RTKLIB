#include <stdlib.h>

#include "file.h"

#define FILE_IO_DEBUG 1
#ifdef  FILE_IO_DEBUG
#define debug(fmt, args ...)  do {fprintf(stderr,"%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ## args); } while(0)
#else
#define debug(fmt, args ...)
#endif

struct file_dev_s
{
   struct port_dev_s port;
};

static int  file_open  (struct port_dev_s *port, const char *path, int mode, char *msg);
static int  file_write (struct port_dev_s *port, const unsigned char *buff, int n, char *msg);
static int  file_read  (struct port_dev_s *port, unsigned char *buff, int n, char *msg);
static int  file_state (struct port_dev_s *port);
static void file_close (struct port_dev_s *port);

struct port_dev_s *file_initialize()
{
    struct file_dev_s *port;

    port = malloc(sizeof(struct file_dev_s));

    port->port.ops.open  = file_open;
    port->port.ops.write = file_write;
    port->port.ops.read  = file_read;
    port->port.ops.state = file_state;
    port->port.ops.close = file_close;

    return (struct port_dev_s*) port;
}

void uart_deinitialize(struct port_dev_s *port)
{
    struct uart_dev_s *device;

    device = (struct uart_dev_s *) port;

    free(device);
}

int file_open(struct port_dev_s *port, const char *path, int mode, char *msg)
{
    struct file_dev_s *device = (struct file_dev_s*) port;

    return 1;
}

/* close serial --------------------------------------------------------------*/
void uart_close(struct port_dev_s *port)
{
    struct file_dev_s *device = (struct file_dev_s*) port;

}
