#include <stdlib.h>
#include <stdio.h>

#include "file.h"
#include "rtklib.h"

#define FILE_IO_DEBUG 1
#ifdef  FILE_IO_DEBUG
#define debug(fmt, args ...)  do {fprintf(stderr,"%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ## args); } while(0)
#else
#define debug(fmt, args ...)
#endif

#define TIMETAGH_LEN        64          /* time tag file header length */

struct file_t {            /* file control type */
    FILE *fp;               /* file pointer */
    FILE *fp_tag;           /* file pointer of tag file */
    FILE *fp_tmp;           /* temporary file pointer for swap */
    FILE *fp_tag_tmp;       /* temporary file pointer of tag file for swap */
    char path[MAXSTRPATH];  /* file path */
    char openpath[MAXSTRPATH]; /* open file path */
    int mode;               /* file mode */
    int timetag;            /* time tag flag (0:off,1:on) */
    int repmode;            /* replay mode (0:master,1:slave) */
    int offset;             /* time offset (ms) for slave */
    gtime_t time;           /* start time */
    gtime_t wtime;          /* write time */
    unsigned int tick;      /* start tick */
    unsigned int tick_f;    /* start tick in file */
    unsigned int fpos;      /* current file position */
    double start;           /* start offset (s) */
    double speed;           /* replay speed (time factor) */
    double swapintv;        /* swap interval (hr) (0: no swap) */
    lock_t lock;            /* lock flag */
};

struct file_dev_s
{
   struct port_dev_s port;
   struct file_t handle;
};

static int  file_open  (struct port_dev_s *port, const char *path, int mode, char *msg);
static int  file_write (struct port_dev_s *port, const unsigned char *buff, int n, char *msg);
static int  file_read  (struct port_dev_s *port, unsigned char *buff, int n, char *msg);
static int  file_state (struct port_dev_s *port);
static void file_close (struct port_dev_s *port);

static void closefile_  (struct file_t *file);
static void swapfile    (struct file_t *file, gtime_t time, char *msg);
static void swapclose   (struct file_t *file);
static void syncfile    (struct file_t *file1, struct file_t *file2);
static int  openfile_   (struct file_t *file, gtime_t time, char *msg);

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

void file_deinitialize(struct port_dev_s *port)
{
    struct uart_dev_s *device;

    device = (struct uart_dev_s *) port;

    free(device);
}

/* open file (path=filepath[::T[::+<off>][::x<speed>]][::S=swapintv]) --------*/
int file_open(struct port_dev_s *port, const char *path, int mode, char *msg)
{
    struct file_dev_s *device = (struct file_dev_s*) port;
    struct file_t *file = &device->handle;

    gtime_t time,time0={0};
    double speed=0.0,start=0.0,swapintv=0.0;
    char *p;
    int timetag=0;
    
    tracet(3,"openfile: path=%s mode=%d\n",path,mode);
    
    if (!(mode&(STR_MODE_R|STR_MODE_W))) return 0;
    
    /* file options */
    for (p=(char *)path;(p=strstr(p,"::"));p+=2) { /* file options */
        if      (*(p+2)=='T') timetag=1;
        else if (*(p+2)=='+') sscanf(p+2,"+%lf",&start);
        else if (*(p+2)=='x') sscanf(p+2,"x%lf",&speed);
        else if (*(p+2)=='S') sscanf(p+2,"S=%lf",&swapintv);
    }
    if (start<=0.0) start=0.0;
    if (swapintv<=0.0) swapintv=0.0;
    
    file->fp=file->fp_tag=file->fp_tmp=file->fp_tag_tmp=NULL;
    strcpy(file->path,path);
    if ((p=strstr(file->path,"::"))) *p='\0';
    file->openpath[0]='\0';
    file->mode=mode;
    file->timetag=timetag;
    file->repmode=0;
    file->offset=0;
    file->time=file->wtime=time0;
    file->tick=file->tick_f=file->fpos=0;
    file->start=start;
    file->speed=speed;
    file->swapintv=swapintv;
    initlock(&file->lock);
    
    time=utc2gpst(timeget());
    
    /* open new file */
    if (!openfile_(file,time,msg)) {
        free(file);
        return 0;
    }
    return 1;
}
/* close file ----------------------------------------------------------------*/
void file_close(struct port_dev_s *port)
{
    struct file_dev_s *device = (struct file_dev_s*) port;
    struct file_t *file = &device->handle;

    tracet(3,"closefile: fp=%d\n",file->fp);
    
    if (!file) 
        return;

    closefile_(file);
}

static unsigned int tick_master=0; /* time tick master for replay */

/* read file -----------------------------------------------------------------*/
int file_read(struct port_dev_s *port, unsigned char *buff, int nmax, char *msg)
{
    struct file_dev_s *device = (struct file_dev_s*) port;
    struct file_t *file = &device->handle;

    unsigned int nr=0,t,tick;
    size_t fpos;
    
    tracet(4,"readfile: fp=%d nmax=%d\n",file->fp,nmax);
    
    if (!file) return 0;
    
    if (file->fp_tag) {
        if (file->repmode) { /* slave */
            t=(unsigned int)(tick_master+file->offset);
        }
        else { /* master */
            t=(unsigned int)((tickget()-file->tick)*file->speed+file->start*1000.0);
        }
        for (;;) { /* seek file position */
            if (fread(&tick,sizeof(tick),1,file->fp_tag)<1||
                fread(&fpos,sizeof(fpos),1,file->fp_tag)<1) {
                fseek(file->fp,0,SEEK_END);
                sprintf(msg,"end");
                break;
            }
            if (file->repmode||file->speed>0.0) {
                if ((int)(tick-t)<1) continue;
            }
            if (!file->repmode) tick_master=tick;
            
            sprintf(msg,"T%+.1fs",(int)tick<0?0.0:(int)tick/1000.0);
            
            if ((int)(fpos-file->fpos)>=nmax) {
               fseek(file->fp,fpos,SEEK_SET);
               file->fpos=fpos;
               return 0;
            }
            nmax=(int)(fpos-file->fpos);
            
            if (file->repmode||file->speed>0.0) {
                fseek(file->fp_tag,-(long)(sizeof(tick)+sizeof(fpos)),SEEK_CUR);
            }
            break;
        }
    }
    if (nmax>0) {
        nr=fread(buff,1,nmax,file->fp);
        file->fpos+=nr;
        if (nr<=0) sprintf(msg,"end");
    }
    tracet(5,"readfile: fp=%d nr=%d fpos=%d\n",file->fp,nr,file->fpos);
    return (int)nr;
}

static int fswapmargin=30;  /* file swap margin (s) */
int file_write(struct port_dev_s *port, const unsigned char *buff, int n, char *msg)
{
    struct file_dev_s *device = (struct file_dev_s*) port;
    struct file_t *file = &device->handle;

    gtime_t wtime;
    unsigned int ns,tick=tickget();
    int week1,week2;
    double tow1,tow2,intv;
    size_t fpos,fpos_tmp;
    
    tracet(3,"writefile: fp=%d n=%d\n",file->fp,n);
    
    if (!file) return 0;
    
    wtime=utc2gpst(timeget()); /* write time in gpst */
    
    /* swap writing file */
    if (file->swapintv>0.0&&file->wtime.time!=0) {
        intv=file->swapintv*3600.0;
        tow1=time2gpst(file->wtime,&week1);
        tow2=time2gpst(wtime,&week2);
        tow2+=604800.0*(week2-week1);
        
        /* open new swap file */
        if (floor((tow1+fswapmargin)/intv)<floor((tow2+fswapmargin)/intv)) {
            swapfile(file,timeadd(wtime,fswapmargin),msg);
        }
        /* close old swap file */
        if (floor((tow1-fswapmargin)/intv)<floor((tow2-fswapmargin)/intv)) {
            swapclose(file);
        }
    }
    if (!file->fp) return 0;
    
    ns=fwrite(buff,1,n,file->fp);
    fpos=ftell(file->fp);
    fflush(file->fp);
    file->wtime=wtime;
    
    if (file->fp_tmp) {
        fwrite(buff,1,n,file->fp_tmp);
        fpos_tmp=ftell(file->fp_tmp);
        fflush(file->fp_tmp);
    }
    if (file->fp_tag) {
        tick-=file->tick;
        fwrite(&tick,1,sizeof(tick),file->fp_tag);
        fwrite(&fpos,1,sizeof(fpos),file->fp_tag);
        fflush(file->fp_tag);
        
        if (file->fp_tag_tmp) {
            fwrite(&tick,1,sizeof(tick),file->fp_tag_tmp);
            fwrite(&fpos_tmp,1,sizeof(fpos_tmp),file->fp_tag_tmp);
            fflush(file->fp_tag_tmp);
        }
    }
    tracet(5,"writefile: fp=%d ns=%d tick=%5d fpos=%d\n",file->fp,ns,tick,fpos);
    
    return (int)ns;
}

/* get state file ------------------------------------------------------------*/
static int file_state(struct port_dev_s *port)
{
    struct file_dev_s *device = (struct file_dev_s*) port;
    struct file_t *file = &device->handle;

    return file ? 2: 0;
}

/* open file -----------------------------------------------------------------*/
int openfile_(struct file_t *file, gtime_t time, char *msg)
{    
    FILE *fp;
    char *rw,tagpath[MAXSTRPATH+4]="";
    char tagh[TIMETAGH_LEN+1]="";
    
    tracet(3,"openfile_: path=%s time=%s\n",file->path,time_str(time,0));
    
    file->time=utc2gpst(timeget());
    file->tick=file->tick_f=tickget();
    file->fpos=0;
    
    /* replace keywords */
    reppath(file->path,file->openpath,time,"","");
    
    /* create directory */
    if ((file->mode&STR_MODE_W)&&!(file->mode&STR_MODE_R)) {
        createdir(file->openpath);
    }
    if (file->mode&STR_MODE_R) rw="rb"; else rw="wb";
    
    if (!(file->fp=fopen(file->openpath,rw))) {
        sprintf(msg,"file open error: %s",file->openpath);
        tracet(1,"openfile: %s\n",msg);
        return 0;
    }
    tracet(4,"openfile_: open file %s (%s)\n",file->openpath,rw);
    
    sprintf(tagpath,"%s.tag",file->openpath);
    
    if (file->timetag) { /* output/sync time-tag */
        
        if (!(file->fp_tag=fopen(tagpath,rw))) {
            sprintf(msg,"tag open error: %s",tagpath);
            tracet(1,"openfile: %s\n",msg);
            fclose(file->fp);
            return 0;
        }
        tracet(4,"openfile_: open tag file %s (%s)\n",tagpath,rw);
        
        if (file->mode&STR_MODE_R) {
            if (fread(&tagh,TIMETAGH_LEN,1,file->fp_tag)==1&&
                fread(&file->time,sizeof(file->time),1,file->fp_tag)==1) {
                memcpy(&file->tick_f,tagh+TIMETAGH_LEN-4,sizeof(file->tick_f));
            }
            else {
                file->tick_f=0;
            }
            /* adust time to read playback file */
            timeset(file->time);
        }
        else {
            sprintf(tagh,"TIMETAG RTKLIB %s",VER_RTKLIB);
            memcpy(tagh+TIMETAGH_LEN-4,&file->tick_f,sizeof(file->tick_f));
            fwrite(&tagh,1,TIMETAGH_LEN,file->fp_tag);
            fwrite(&file->time,1,sizeof(file->time),file->fp_tag);
            /* time tag file structure   */
            /*   HEADER(60)+TICK(4)+TIME(12)+ */
            /*   TICK0(4)+FPOS0(4/8)+    */
            /*   TICK1(4)+FPOS1(4/8)+... */
        }
    }
    else if (file->mode&STR_MODE_W) { /* remove time-tag */
        if ((fp=fopen(tagpath,"rb"))) {
            fclose(fp);
            remove(tagpath);
        }
    }
    return 1;
}

/* close file ----------------------------------------------------------------*/
void closefile_(struct file_t *file)
{
    tracet(3,"closefile_: path=%s\n",file->path);
    
    if (file->fp) fclose(file->fp);
    if (file->fp_tag) fclose(file->fp_tag);
    if (file->fp_tmp) fclose(file->fp_tmp);
    if (file->fp_tag_tmp) fclose(file->fp_tag_tmp);
    file->fp=file->fp_tag=file->fp_tmp=file->fp_tag_tmp=NULL;
}
/* open new swap file --------------------------------------------------------*/
void swapfile(struct file_t *file, gtime_t time, char *msg)
{
    char openpath[MAXSTRPATH];
    
    tracet(3,"swapfile: fp=%d time=%s\n",file->fp,time_str(time,0));
    
    /* return if old swap file open */
    if (file->fp_tmp||file->fp_tag_tmp) return;
    
    /* check path of new swap file */
    reppath(file->path,openpath,time,"","");
    
    if (!strcmp(openpath,file->openpath)) {
        tracet(2,"swapfile: no need to swap %s\n",openpath);
        return;
    }
    /* save file pointer to temporary pointer */
    file->fp_tmp=file->fp;
    file->fp_tag_tmp=file->fp_tag;
    
    /* open new swap file */
    openfile_(file,time,msg);
}
/* close old swap file -------------------------------------------------------*/
void swapclose(struct file_t *file)
{
    tracet(3,"swapclose: fp_tmp=%d\n",file->fp_tmp);
    
    if (file->fp_tmp    ) fclose(file->fp_tmp    );
    if (file->fp_tag_tmp) fclose(file->fp_tag_tmp);
    file->fp_tmp=file->fp_tag_tmp=NULL;
}
/* write file ----------------------------------------------------------------*/
void syncfile(struct file_t *file1, struct file_t *file2)
{
    if (!file1->fp_tag||!file2->fp_tag) return;
    file1->repmode=0;
    file2->repmode=1;
    file2->offset=(int)(file1->tick_f-file2->tick_f);
}
