#ifndef __PORT_H__
#define __PORT_H__

#define port_open(port, path, mode, msg) port->ops.open(port, path, mode, msg)
#define port_read(port, buff, n, msg) port->ops.read(port, buff, n, msg)
#define port_write(port, buff, n, msg) port->ops.write(port, buff, n, msg)
#define port_state(port) port->ops.state(port)
#define port_close(port) port->ops.close(port)

struct port_dev_s;
struct port_ops_s
{
    int  (*open)(struct port_dev_s *device, const char *path, int mode, char *msg);
    int  (*write)(struct port_dev_s *device, const unsigned char *buff, int n, char *msg);
    int  (*read)(struct port_dev_s *device, unsigned char *buff, int n, char *msg);
    int  (*state)(struct port_dev_s *device);
    void (*close)(struct port_dev_s *device);
};

struct port_dev_s
{
    struct port_ops_s ops;
};


#endif /* __PORT_H__ */
