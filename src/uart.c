#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define __USE_MISC
#include <termios.h>
#include <string.h>

#include "rtklib.h"
#include "uart.h"

#define UART_IO_DEBUG 1
#ifdef  UART_IO_DEBUG
#define debug(fmt, args ...)  do {fprintf(stderr,"%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ## args); } while(0)
#else
#define debug(fmt, args ...)
#endif

#ifdef WIN32
#define dev_t               HANDLE
#else
#define dev_t               int
#endif

struct serial_t {            /* serial control type */
    dev_t dev;              /* serial device */
    int error;              /* error state */
#ifdef WIN32
    int state,wp,rp;        /* state,write/read pointer */
    int buffsize;           /* write buffer size (bytes) */
    HANDLE thread;          /* write thread */
    lock_t lock;            /* lock flag */
    unsigned char *buff;    /* write buffer */
#endif
};

struct uart_dev_s
{
   struct port_dev_s port;
   struct serial_t handle;
};

static int  uart_open  (struct port_dev_s *port, const char *path, int mode, char *msg);
static int  uart_write (struct port_dev_s *port, const unsigned char *buff, int n, char *msg);
static int  uart_read  (struct port_dev_s *port, unsigned char *buff, int n, char *msg);
static int  uart_state (struct port_dev_s *port);
static void uart_close (struct port_dev_s *port);

struct port_dev_s *uart_initialize()
{
    struct uart_dev_s *port;

    port = malloc(sizeof(struct uart_dev_s));

    port->port.ops.open  = uart_open;
    port->port.ops.write = uart_write;
    port->port.ops.read  = uart_read;
    port->port.ops.state = uart_state;
    port->port.ops.close = uart_close;

    return (struct port_dev_s*) port;
}

void uart_deinitialize(struct port_dev_s *port)
{
    struct uart_dev_s *device;

    device = (struct uart_dev_s *) port;
}

/* open serial ---------------------------------------------------------------*/
int uart_open(struct port_dev_s *port, const char *path, int mode, char *msg)
{
    const int br[]={
        300,600,1200,2400,4800,9600,19200,38400,57600,115200,230400
    };
    struct uart_dev_s *device = (struct uart_dev_s*) port;
    struct serial_t *serial = &device->handle;

    int i,brate=9600,bsize=8,stopb=1;
    char *p,parity='N',dev[128],port_path[128],fctr[64]="";
#ifdef WIN32
    DWORD error,rw=0,siz=sizeof(COMMCONFIG);
    COMMCONFIG cc={0};
    COMMTIMEOUTS co={MAXDWORD,0,0,0,0}; /* non-block-read */
    char dcb[64]="";
#else
    const speed_t bs[]={
        B300,B600,B1200,B2400,B4800,B9600,B19200,B38400,B57600,B115200,B230400
    };
    struct termios ios={0};
    int rw=0;
#endif
    tracet(3,"openserial: path=%s mode=%d\n",path,mode);
    
    if ((p=strchr(path,':'))) {
        strncpy(port_path,path,p-path); port_path[p-path]='\0';
        sscanf(p,":%d:%d:%c:%d:%s",&brate,&bsize,&parity,&stopb,fctr);
    }
    else strcpy(port_path,path);
    
    for (i=0;i<11;i++) if (br[i]==brate) break;
    if (i>=12) {
        sprintf(msg,"bitrate error (%d)",brate);
        tracet(1,"openserial: %s path=%s\n",msg,path);
        free(serial);
        return 0;
    }
    parity=(char)toupper((int)parity);
    
#ifdef WIN32
    sprintf(dev,"\\\\.\\%s",port_path);
    if (mode&STR_MODE_R) rw|=GENERIC_READ;
    if (mode&STR_MODE_W) rw|=GENERIC_WRITE;
    
    serial->dev=CreateFile(dev,rw,0,0,OPEN_EXISTING,0,NULL);
    if (serial->dev==INVALID_HANDLE_VALUE) {
        sprintf(msg,"device open error (%d)",(int)GetLastError());
        tracet(1,"openserial: %s path=%s\n",msg,path);
        free(serial);
        return 0;
    }
    if (!GetDefaultCommConfig(port_path,&cc,&siz)) {
        sprintf(msg,"getconfig error (%d)",(int)GetLastError());
        tracet(1,"openserial: %s\n",msg);
        CloseHandle(serial->dev);
        free(serial);
        return 0;
    }
    sprintf(dcb,"baud=%d parity=%c data=%d stop=%d",brate,parity,bsize,stopb);
    if (!BuildCommDCB(dcb,&cc.dcb)) {
        sprintf(msg,"buiddcb error (%d)",(int)GetLastError());
        tracet(1,"openserial: %s\n",msg);
        CloseHandle(serial->dev);
        free(serial);
        return 0;
    }
    if (!strcmp(fctr,"rts")) {
        cc.dcb.fRtsControl=RTS_CONTROL_HANDSHAKE;
    }
    SetCommConfig(serial->dev,&cc,siz); /* ignore error to support_path novatel */
    SetCommTimeouts(serial->dev,&co);
    ClearCommError(serial->dev,&error,NULL);
    PurgeComm(serial->dev,PURGE_TXABORT|PURGE_RXABORT|PURGE_TXCLEAR|PURGE_RXCLEAR);
    
    /* create write thread */
    initlock(&serial->lock);
    serial->state=serial->wp=serial->rp=serial->error=0;
    serial->buffsize=buffsize;
    if (!(serial->buff=(unsigned char *)malloc(buffsize))) {
        CloseHandle(serial->dev);
        free(serial);
        return 0;
    }
    if (!(serial->thread=CreateThread(NULL,0,serialthread,serial,0,NULL))) {
        sprintf(msg,"serial thread error (%d)",(int)GetLastError());
        tracet(1,"openserial: %s\n",msg);
        CloseHandle(serial->dev);
        free(serial);
        return 0;
    }
    return 1;
#else
    sprintf(dev,"/dev/%s",port_path);
    
    if ((mode&STR_MODE_R)&&(mode&STR_MODE_W)) rw=O_RDWR;
    else if (mode&STR_MODE_R) rw=O_RDONLY;
    else if (mode&STR_MODE_W) rw=O_WRONLY;
    
    if ((serial->dev=open(dev,rw|O_NOCTTY|O_NONBLOCK))<0) {
        sprintf(msg,"device open error (%d)",errno);
        tracet(1,"openserial: %s dev=%s\n",msg,dev);
        free(serial);
        return 0;
    }
    tcgetattr(serial->dev,&ios);
    ios.c_iflag=0;
    ios.c_oflag=0;
    ios.c_lflag=0;     /* non-canonical */
    ios.c_cc[VMIN ]=0; /* non-block-mode */
    ios.c_cc[VTIME]=0;
    cfsetospeed(&ios,bs[i]);
    cfsetispeed(&ios,bs[i]);
    ios.c_cflag|=bsize==7?CS7:CS8;
    ios.c_cflag|=parity=='O'?(PARENB|PARODD):(parity=='E'?PARENB:0);
    ios.c_cflag|=stopb==2?CSTOPB:0;
    ios.c_cflag|= !strcmp(fctr,"rts") ? CRTSCTS: 0;
    tcsetattr(serial->dev,TCSANOW,&ios);
    tcflush(serial->dev,TCIOFLUSH);

    return 1;
#endif
}

/* close serial --------------------------------------------------------------*/
void uart_close(struct port_dev_s *port)
{

    struct uart_dev_s *device = (struct uart_dev_s *)port;
    struct serial_t *serial = &device->handle;
    tracet(3,"closeserial: dev=%d\n",serial->dev);
    
    if (!serial) return;
#ifdef WIN32
    serial->state=0;
    WaitForSingleObject(serial->thread,10000);
    CloseHandle(serial->dev);
    CloseHandle(serial->thread);
#else
    close(serial->dev);
#endif
    free(serial);
}
/* read serial ---------------------------------------------------------------*/
int uart_read(struct port_dev_s *port, unsigned char *buff, int n, char *msg)
{
    struct uart_dev_s *device = (struct uart_dev_s *)port;
    struct serial_t *serial = &device->handle;
#ifdef WIN32
    DWORD nr;
#else
    int nr;
#endif
    tracet(4,"readserial: dev=%d n=%d\n",serial->dev,n);
    if (!serial) return 0;
#ifdef WIN32
    if (!ReadFile(serial->dev,buff,n,&nr,NULL)) return 0;
#else
    if ((nr=read(serial->dev,buff,n))<0) return 0;
#endif
    tracet(5,"readserial: exit dev=%d nr=%d\n",serial->dev,nr);
    return nr;
}

/* write serial --------------------------------------------------------------*/
int uart_write(struct port_dev_s *port, const unsigned char *buff, int n, char *msg)
{
    int ns;
    struct uart_dev_s *device = (struct uart_dev_s *)port;
    struct serial_t *serial = &device->handle;
    
    tracet(3,"writeserial: dev=%d n=%d\n",serial->dev,n);

    if (!serial) return 0;
#ifdef WIN32
    if ((ns=writeseribuff(serial,buff,n))<n) serial->error=1;
#else
    if ((ns=write(serial->dev,buff,n))<0) return 0;
#endif
    tracet(5,"writeserial: exit dev=%d ns=%d\n",serial->dev,ns);
    return ns;
}
/* get state serial ----------------------------------------------------------*/
int  uart_state (struct port_dev_s *port)
{
    struct uart_dev_s *device = (struct uart_dev_s *)port;
    struct serial_t *serial = &device->handle;
    return !serial?0:(serial->error?-1:2);
}

/* read/write serial buffer --------------------------------------------------*/
#ifdef WIN32
static int readseribuff(struct port_dev_s *port, unsigned char *buff, int nmax)
{
    int ns;
    struct uart_dev_s *device = (struct uart_dev_s *)port;
    struct serial_t *serial = &device->handle;
    
    tracet(5,"readseribuff: dev=%d\n",serial->dev);
    
    lock(&serial->lock);
    for (ns=0;serial->rp!=serial->wp&&ns<nmax;ns++) {
       buff[ns]=serial->buff[serial->rp];
       if (++serial->rp>=serial->buffsize) serial->rp=0;
    }
    unlock(&serial->lock);
    tracet(5,"readseribuff: ns=%d rp=%d wp=%d\n",ns,serial->rp,serial->wp);
    return ns;
}
static int writeseribuff(serial_t *serial, unsigned char *buff, int n)
{
    int ns,wp;
    
    tracet(5,"writeseribuff: dev=%d n=%d\n",serial->dev,n);
    
    lock(&serial->lock);
    for (ns=0;ns<n;ns++) {
        serial->buff[wp=serial->wp]=buff[ns];
        if (++wp>=serial->buffsize) wp=0;
        if (wp!=serial->rp) serial->wp=wp;
        else {
            tracet(2,"serial buffer overflow: size=%d\n",serial->buffsize);
            break;
        }
    }
    unlock(&serial->lock);
    tracet(5,"writeseribuff: ns=%d rp=%d wp=%d\n",ns,serial->rp,serial->wp);
    return ns;
}
#endif /* WIN32 */

/* write serial thread -------------------------------------------------------*/
#ifdef WIN32
static DWORD WINAPI serialthread(void *arg)
{
    serial_t *serial=(serial_t *)arg;
    unsigned char buff[128];
    unsigned int tick;
    DWORD ns;
    int n;
    
    tracet(3,"serialthread:\n");
    
    serial->state=1;
    
    for (;;) {
        tick=tickget();
        while ((n=readseribuff(serial,buff,sizeof(buff)))>0) {
            if (!WriteFile(serial->dev,buff,n,&ns,NULL)) serial->error=1;
        }
        if (!serial->state) break;
        sleepms(10-(int)(tickget()-tick)); /* cycle=10ms */
    }
    free(serial->buff);
    return 0;
}
#endif /* WIN32 */

