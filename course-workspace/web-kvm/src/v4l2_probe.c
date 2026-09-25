#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char **argv) {
    const char *device = argc > 1 ? argv[1] : "/dev/video0";
    int fd = open(device, O_RDWR | O_NONBLOCK);
    if (fd < 0) { fprintf(stderr, "VIDEO_DEVICE_UNAVAILABLE:%s:%s\n", device, strerror(errno)); return 2; }
    struct v4l2_capability cap = {0};
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) { fprintf(stderr, "VIDIOC_QUERYCAP_FAILED:%s\n", strerror(errno)); close(fd); return 3; }
    printf("driver=%s card=%s bus=%s capabilities=0x%08x\n", cap.driver, cap.card, cap.bus_info, cap.capabilities);
    close(fd); return 0;
}
