#ifndef __PORT_H__
#define __PORT_H__

struct port_dev_s;
struct port_ops_s
{
    int  (*open)(char *path, int mode, char *msg);
    int  (*write)(struct port_dev_s *device, const unsigned char *buff, int n, char *msg);
    int  (*read)(struct port_dev_s *device, unsigned char *buff, int n, char *msg);
    int  (*state)(struct port_dev_s *device);
    void (*close)(struct port_dev_s *device);
};

struct port_dev_s
{
    struct port_ops_s *ops;
};


#endif /* __PORT_H__ */
