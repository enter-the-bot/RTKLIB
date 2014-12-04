#ifndef __UART_H__
#define __UART_H__

#include "port.h"

struct port_dev_s *uart_initialize();
void uart_deinitialize(struct port_dev_s *port);

#endif /* __UART_H__ */
