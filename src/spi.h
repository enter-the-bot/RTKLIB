#ifndef __SPI_H__
#define __SPI_H__

#include "port.h"

struct port_dev_s *spi_initialize();
void spi_deinitialize(struct port_dev_s *port);

#endif
