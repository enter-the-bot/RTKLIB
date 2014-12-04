#ifndef __FILE_H__
#define __FILE_H__

#include "port.h"

struct port_dev_s *file_initialize();
void file_deinitialize(struct port_dev_s *port);

#endif /* __FILE_H__ */
