/* 对应项目 8 字节、无 Report ID 的键盘描述符。默认只展示报告。 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int report(int fd, const uint8_t data[8]) {
    /* HID 是报告接口；短写不能像 Socket 一样拆成下一份报告。 */
    struct pollfd p={.fd=fd,.events=POLLOUT};
    int n;
    do {n=poll(&p,1,2000);} while(n<0 && errno==EINTR);
    if(n<=0 || !(p.revents&POLLOUT)) {
        fprintf(stderr,"HID not writable; check USB enumeration\n");return -1;
    }
    ssize_t written;
    do {written=write(fd,data,8);} while(written<0 && errno==EINTR);
    if(written!=8){
        if(written<0)perror("HID write");
        else fprintf(stderr,"short HID report: %zd/8\n",written);
        return -1;
    }
    return 0;
}
int main(int argc,char **argv) {
    const uint8_t press[8]={0,0,0x04,0,0,0,0,0};
    const uint8_t release[8]={0};
    if(argc==1){
        puts("press:   00 00 04 00 00 00 00 00");
        puts("release: 00 00 00 00 00 00 00 00");
        puts("To send into a blank editor: hid_keyboard --send /dev/hidg0");
        return 0;
    }
    if(argc!=3 || strcmp(argv[1],"--send"))return 2;
    int fd=open(argv[2],O_WRONLY|O_NONBLOCK);
    if(fd<0){perror("open HID");return 1;}
    int a=report(fd,press);
    struct timespec pause={.tv_sec=0,.tv_nsec=100000000};
    while(nanosleep(&pause,&pause)<0 && errno==EINTR){}
    /* 即使按下失败也尝试释放，避免测试留下按键状态。 */
    int b=report(fd,release);
    close(fd);return a || b ? 1 : 0;
}
