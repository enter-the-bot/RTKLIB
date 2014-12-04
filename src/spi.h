#ifndef __SPI_H__
#define __SPI_H__

#include <stdint.h>

#include "port.h"

struct port_dev_s *openspi(const char *path, int mode, char *msg);

#endif
