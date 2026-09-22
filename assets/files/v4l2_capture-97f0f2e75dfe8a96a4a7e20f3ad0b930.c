/* 教学示例：只读查询，或在设备空闲时采集单内存平面的 NV12。
 * 默认仅 QUERYCAP / G_FMT；--capture 才配置并启动采集。
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

struct mapping { void *ptr; size_t size; };
static int xioctl(int fd, unsigned long op, void *arg) {
    int r;
    do { r = ioctl(fd, op, arg); } while (r < 0 && errno == EINTR);
    return r;
}
static int save_bytes(int fd, const void *data, size_t size) {
    const unsigned char *p = data;
    while (size) {
        ssize_t n = write(fd, p, size);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        p += n; size -= (size_t)n;
    }
    return 0;
}
static void show_format(const struct v4l2_format *f) {
    const struct v4l2_pix_format_mplane *p = &f->fmt.pix_mp;
    uint32_t c = p->pixelformat;
    printf("format=%c%c%c%c size=%ux%u planes=%u\n",
           c & 255, (c >> 8) & 255, (c >> 16) & 255, (c >> 24) & 255,
           p->width, p->height, p->num_planes);
    for (unsigned i=0; i<p->num_planes; ++i)
        printf("plane[%u]: bytesperline=%u sizeimage=%u\n", i,
               p->plane_fmt[i].bytesperline, p->plane_fmt[i].sizeimage);
}
int main(int argc, char **argv) {
    int capture = argc == 5 && strcmp(argv[2], "--capture") == 0;
    if (argc != 2 && !capture) {
        fprintf(stderr,"usage: %s DEVICE [--capture OUTPUT FRAME_COUNT]\n",argv[0]);
        return 2;
    }
    long target = 0;
    if (capture) {
        char *end = NULL;
        errno = 0; target = strtol(argv[4], &end, 10);
        if (errno || !end || *end || target < 1 || target > 10000) return 2;
    }
    int fd = open(argv[1], O_RDWR | O_NONBLOCK);
    if (fd < 0) { perror("open video"); return 1; }
    int rc=1, output=-1, streaming=0;
    struct mapping maps[8] = {{0}};
    struct v4l2_requestbuffers req = {0};
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    struct v4l2_capability cap = {0};
    if (xioctl(fd, VIDIOC_QUERYCAP, &cap)<0) {perror("QUERYCAP");goto done;}
    uint32_t caps = cap.capabilities & V4L2_CAP_DEVICE_CAPS ? cap.device_caps : cap.capabilities;
    printf("driver=%.16s card=%.32s caps=0x%x\n",cap.driver,cap.card,caps);
    if (!(caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE) || !(caps & V4L2_CAP_STREAMING)) {
        fprintf(stderr,"requires capture mplane and streaming\n");goto done;
    }
    struct v4l2_format fmt = {0}; fmt.type = type;
    if (!capture) {
        if (xioctl(fd,VIDIOC_G_FMT,&fmt)<0) {perror("G_FMT");goto done;}
        show_format(&fmt); rc=0;goto done;
    }
    /* 只在已停止 KVM 采集服务、设备空闲时进入此分支。 */
    int input=0;
    if (xioctl(fd,VIDIOC_S_INPUT,&input)<0) {perror("S_INPUT");goto done;}
    fmt.fmt.pix_mp.width=1920; fmt.fmt.pix_mp.height=1080;
    fmt.fmt.pix_mp.pixelformat=V4L2_PIX_FMT_NV12;
    fmt.fmt.pix_mp.field=V4L2_FIELD_NONE;
    if (xioctl(fd,VIDIOC_S_FMT,&fmt)<0) {perror("S_FMT");goto done;}
    struct v4l2_streamparm parm={0}; parm.type=type;
    parm.parm.capture.timeperframe.numerator=1;
    parm.parm.capture.timeperframe.denominator=60;
    parm.parm.capture.capturemode=0x0002; /* 本项目 VIN 的配置，不是通用采集常量。 */
    if (xioctl(fd,VIDIOC_S_PARM,&parm)<0) {perror("S_PARM");goto done;}
    if (xioctl(fd,VIDIOC_G_FMT,&fmt)<0) {perror("G_FMT");goto done;}
    show_format(&fmt);
    if (fmt.fmt.pix_mp.pixelformat!=V4L2_PIX_FMT_NV12 || fmt.fmt.pix_mp.num_planes!=1) {
        fprintf(stderr,"example only supports single-memory-plane NV12\n");goto done;
    }
    /* 不覆盖已有实验结果。每次实验换一个输出文件名。 */
    output=open(argv[3],O_WRONLY|O_CREAT|O_EXCL,0644);
    if(output<0){perror("open output");goto done;}
    req.type=type;req.memory=V4L2_MEMORY_MMAP;req.count=4;
    if(xioctl(fd,VIDIOC_REQBUFS,&req)<0){req.count=0;perror("REQBUFS");goto done;}
    if(req.count<2 || req.count>8){fprintf(stderr,"unsupported buffer count\n");goto done;}
    for(unsigned i=0;i<req.count;++i){
        struct v4l2_plane plane={0};struct v4l2_buffer b={0};
        b.type=type;b.memory=req.memory;b.index=i;b.length=1;b.m.planes=&plane;
        if(xioctl(fd,VIDIOC_QUERYBUF,&b)<0){perror("QUERYBUF");goto done;}
        maps[i].size=plane.length;
        maps[i].ptr=mmap(NULL,plane.length,PROT_READ|PROT_WRITE,MAP_SHARED,fd,plane.m.mem_offset);
        if(maps[i].ptr==MAP_FAILED){maps[i].ptr=NULL;perror("mmap");goto done;}
        if(xioctl(fd,VIDIOC_QBUF,&b)<0){perror("initial QBUF");goto done;}
    }
    if(xioctl(fd,VIDIOC_STREAMON,&type)<0){perror("STREAMON");goto done;}
    streaming=1;
    struct timespec start, end;clock_gettime(CLOCK_MONOTONIC,&start);
    long received=0;
    while(received<target){
        struct pollfd p={.fd=fd,.events=POLLIN};
        int ready=poll(&p,1,2000);
        if(ready<0 && errno==EINTR)continue;
        if(ready<0){perror("poll");goto done;}
        if(!ready){fprintf(stderr,"timeout: no captured frame in 2 seconds\n");goto done;}
        struct v4l2_plane plane={0};struct v4l2_buffer b={0};
        b.type=type;b.memory=req.memory;b.length=1;b.m.planes=&plane;
        if(xioctl(fd,VIDIOC_DQBUF,&b)<0){
            if(errno==EAGAIN)continue;
            perror("DQBUF");goto done;
        }
        if(b.index>=req.count || plane.data_offset>plane.bytesused ||
           plane.bytesused>maps[b.index].size || (b.flags & V4L2_BUF_FLAG_ERROR)){
            fprintf(stderr,"invalid/error frame\n");goto done;
        }
        size_t size=plane.bytesused-plane.data_offset;
        if(!size){fprintf(stderr,"empty frame\n");goto done;}
        if(save_bytes(output,(char*)maps[b.index].ptr+plane.data_offset,size)<0){perror("write frame");goto done;}
        printf("frame=%ld sequence=%u bytes=%zu\n",received,b.sequence,size);
        if(xioctl(fd,VIDIOC_QBUF,&b)<0){perror("QBUF");goto done;}
        ++received;
    }
    clock_gettime(CLOCK_MONOTONIC,&end);
    double seconds=(end.tv_sec-start.tv_sec)+(end.tv_nsec-start.tv_nsec)/1e9;
    printf("saved=%ld elapsed=%.3fs rate=%.2ffps (includes disk writes)\n",received,seconds,received/seconds);
    rc=0;
done:
    if(streaming && xioctl(fd,VIDIOC_STREAMOFF,&type)<0){perror("STREAMOFF");rc=1;}
    for(unsigned i=0;i<8;++i)if(maps[i].ptr)munmap(maps[i].ptr,maps[i].size);
    if(req.count){req.count=0;xioctl(fd,VIDIOC_REQBUFS,&req);}
    if(output>=0 && close(output)<0){perror("close output");rc=1;}
    close(fd);return rc;
}
