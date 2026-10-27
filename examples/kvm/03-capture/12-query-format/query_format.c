/* 第一课：只读查询 /dev/video0 的能力和当前格式。
 * 不修改格式、不申请缓冲区、不启动采集，KVM 服务运行时也可以执行。
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* ioctl 可能被信号打断，返回 EINTR 时重试，其他错误原样返回。 */
static int xioctl(int fd, unsigned long req, void *arg)
{
	int ret;
	do {
		ret = ioctl(fd, req, arg);
	} while (ret < 0 && errno == EINTR);
	return ret;
}

static void print_fourcc(uint32_t value)
{
	printf("%c%c%c%c", value & 0xff, (value >> 8) & 0xff,
	       (value >> 16) & 0xff, (value >> 24) & 0xff);
}

int main(int argc, char **argv)
{
	const char *dev = argc > 1 ? argv[1] : "/dev/video0";
	struct v4l2_capability cap = {0};
	struct v4l2_format fmt = {0};
	int fd, ret = 1;

	fd = open(dev, O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		perror(dev);
		return 1;
	}

	if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
		perror("VIDIOC_QUERYCAP");
		goto out;
	}
	printf("driver      = %s\n", cap.driver);
	printf("card        = %s\n", cap.card);
	printf("bus_info    = %s\n", cap.bus_info);
	printf("version     = %u.%u.%u\n", (cap.version >> 16) & 0xff,
	       (cap.version >> 8) & 0xff, cap.version & 0xff);
	printf("capabilities= 0x%08x\n", cap.capabilities);

	/* 有 DEVICE_CAPS 时，节点能力看 device_caps；capabilities 是整块芯片的。 */
	if (cap.capabilities & V4L2_CAP_DEVICE_CAPS) {
		printf("device_caps = 0x%08x\n", cap.device_caps);
		if (!(cap.device_caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE) ||
		    !(cap.device_caps & V4L2_CAP_STREAMING)) {
			fprintf(stderr, "该节点不支持多平面采集或流式 I/O\n");
			goto out;
		}
	}

	/* G_FMT 只读当前格式，不会改变驱动状态。 */
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (xioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
		perror("VIDIOC_G_FMT");
		goto out;
	}

	printf("pixelformat = ");
	print_fourcc(fmt.fmt.pix_mp.pixelformat);
	printf("\nwidth       = %u\n", fmt.fmt.pix_mp.width);
	printf("height      = %u\n", fmt.fmt.pix_mp.height);
	printf("field       = %u\n", fmt.fmt.pix_mp.field);
	printf("num_planes  = %u\n", fmt.fmt.pix_mp.num_planes);

	for (uint32_t i = 0; i < fmt.fmt.pix_mp.num_planes; i++) {
		printf("plane[%u]    = bytesperline %u, sizeimage %u\n", i,
		       fmt.fmt.pix_mp.plane_fmt[i].bytesperline,
		       fmt.fmt.pix_mp.plane_fmt[i].sizeimage);
	}

	/* 紧密 NV12 的算法只用于理解驱动返回的容量，不能代替 sizeimage。 */
	if (fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV12 &&
	    fmt.fmt.pix_mp.num_planes == 1) {
		uint32_t width = fmt.fmt.pix_mp.width;
		uint32_t height = fmt.fmt.pix_mp.height;
		printf("NV12 tight  = %u bytes (只用来说明布局，映射按 QUERYBUF 返回长度)\n",
		       width * height * 3 / 2);
	}

	ret = 0;
out:
	close(fd);
	return ret;
}
