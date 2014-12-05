#ifndef __FILE_H__
#define __FILE_H__

#include "port.h"
#include "rtklib.h"

struct port_dev_s *file_initialize();
void file_deinitialize(struct port_dev_s *port);
void file_sync(struct port_dev_s *port2, struct port_dev_s *port1);
double file_get_start(struct port_dev_s *port);
gtime_t file_get_time(struct port_dev_s *port);

extern int fswapmargin;  /* file swap margin (s) */

#endif /* __FILE_H__ */
