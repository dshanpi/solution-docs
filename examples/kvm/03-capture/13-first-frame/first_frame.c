/* 第十三课：采集一帧 NV12 并保存。
 * 只走 V4L2 的标准流程：S_FMT、REQBUFS、QUERYBUF、mmap、QBUF、
 * STREAMON、DQBUF、保存、STREAMOFF、释放。
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

#define WIDTH 1920
#define HEIGHT 1080
#define BUFFER_COUNT 4
#define TIMEOUT_MS 2000

struct buffer {
	void *start;
	size_t length;
};

static int xioctl(int fd, unsigned long req, void *arg)
{
	int ret;
	do {
		ret = ioctl(fd, req, arg);
	} while (ret < 0 && errno == EINTR);
	return ret;
}

static int write_all(int fd, const void *data, size_t size)
{
	const unsigned char *p = data;
	while (size) {
		ssize_t n = write(fd, p, size);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			return -1;
		p += n;
		size -= (size_t)n;
	}
	return 0;
}

/* 停掉 KVM 或插拔 HDMI 后，输入会先掉锁再重新锁定。
 * 连续三次确认 1080p60，避免在锁定过程中启动采集。 */
static int input_locked(int fd)
{
	struct v4l2_dv_timings t = {0};
	if (ioctl(fd, VIDIOC_QUERY_DV_TIMINGS, &t) < 0)
		return 0;
	if (t.type != V4L2_DV_BT_656_1120)
		return 0;
	const struct v4l2_bt_timings *b = &t.bt;
	if (!b->width || !b->height || !b->pixelclock || b->interlaced)
		return 0;
	uint64_t line = (uint64_t)b->width + b->hfrontporch + b->hsync +
			b->hbackporch;
	uint64_t rows = (uint64_t)b->height + b->vfrontporch + b->vsync +
			b->vbackporch;
	double rate = (double)b->pixelclock / (double)(line * rows);
	return b->width == WIDTH && b->height == HEIGHT &&
	       rate >= 59.0 && rate <= 61.0;
}

static int wait_for_input(int fd, int timeout_ms)
{
	int stable = 0;
	for (int waited = 0; waited <= timeout_ms; waited += 100) {
		if (input_locked(fd)) {
			if (++stable >= 3)
				return 0;
		} else {
			stable = 0;
		}
		struct timespec ts = {.tv_sec = 0, .tv_nsec = 100 * 1000 * 1000};
		nanosleep(&ts, NULL);
	}
	return -1;
}

int main(int argc, char **argv)
{
	const char *dev = argc > 1 ? argv[1] : "/dev/video0";
	const char *out_path = argc > 2 ? argv[2] : "first-frame.nv12";
	struct v4l2_format fmt = {0};
	struct v4l2_requestbuffers req = {0};
	struct buffer buffers[BUFFER_COUNT] = {{0}};
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	int fd = -1, out = -1, streaming = 0, ret = 1;

	fd = open(dev, O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		perror(dev);
		return 1;
	}

	int input = 0;
	if (xioctl(fd, VIDIOC_S_INPUT, &input) < 0) {
		perror("VIDIOC_S_INPUT");
		goto out;
	}

	printf("waiting for HDMI input...\n");
	if (wait_for_input(fd, 10000) < 0) {
		fprintf(stderr, "HDMI 输入 10 秒内没有锁定\n");
		goto out;
	}
	printf("input locked: %dx%d@60\n", WIDTH, HEIGHT);

	fmt.type = type;
	fmt.fmt.pix_mp.width = WIDTH;
	fmt.fmt.pix_mp.height = HEIGHT;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
	fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
	if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
		perror("VIDIOC_S_FMT");
		goto out;
	}

	/* 1/60 秒一帧；capturemode 0x0002 是这块板 VIN 的采集模式，
	 * 不是 V4L2 通用常量，换平台要重新确认。 */
	struct v4l2_streamparm parm = {0};
	parm.type = type;
	parm.parm.capture.timeperframe.numerator = 1;
	parm.parm.capture.timeperframe.denominator = 60;
	parm.parm.capture.capturemode = 0x0002;
	if (xioctl(fd, VIDIOC_S_PARM, &parm) < 0) {
		perror("VIDIOC_S_PARM");
		goto out;
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = type;
	if (xioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
		perror("VIDIOC_G_FMT");
		goto out;
	}
	if (fmt.fmt.pix_mp.pixelformat != V4L2_PIX_FMT_NV12 ||
	    fmt.fmt.pix_mp.width != WIDTH || fmt.fmt.pix_mp.height != HEIGHT ||
	    fmt.fmt.pix_mp.num_planes != 1) {
		fprintf(stderr, "驱动返回的格式与请求不一致\n");
		goto out;
	}

	req.type = type;
	req.memory = V4L2_MEMORY_MMAP;
	req.count = BUFFER_COUNT;
	if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
		perror("VIDIOC_REQBUFS");
		goto out;
	}
	if (req.count < 2) {
		fprintf(stderr, "驱动只分配了 %u 个缓冲区\n", req.count);
		goto out;
	}

	/* VIN 在 REQBUFS 的 queue_setup 里才计算行跨度和缓冲区容量，
	 * 所以申请之后要再读一次，不能沿用 S_FMT 之后的零值。 */
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = type;
	if (xioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
		perror("VIDIOC_G_FMT after REQBUFS");
		goto out;
	}
	printf("buffers=%u sizeimage=%u bytesperline=%u\n", req.count,
	       fmt.fmt.pix_mp.plane_fmt[0].sizeimage,
	       fmt.fmt.pix_mp.plane_fmt[0].bytesperline);

	for (uint32_t i = 0; i < req.count; i++) {
		struct v4l2_plane plane = {0};
		struct v4l2_buffer buf = {0};

		buf.type = type;
		buf.memory = req.memory;
		buf.index = i;
		buf.length = 1;
		buf.m.planes = &plane;
		if (xioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
			perror("VIDIOC_QUERYBUF");
			goto out;
		}

		/* 映射长度用 QUERYBUF 返回的 plane.length，偏移用 mem_offset；
		 * 每个 buffer 单独查，不能套用同一个偏移。 */
		buffers[i].length = plane.length;
		buffers[i].start = mmap(NULL, plane.length, PROT_READ | PROT_WRITE,
					MAP_SHARED, fd, plane.m.mem_offset);
		if (buffers[i].start == MAP_FAILED) {
			buffers[i].start = NULL;
			perror("mmap");
			goto out;
		}

		if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
			perror("VIDIOC_QBUF");
			goto out;
		}
	}

	if (xioctl(fd, VIDIOC_STREAMON, &type) < 0) {
		perror("VIDIOC_STREAMON");
		goto out;
	}
	streaming = 1;

	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);

	struct pollfd p = {.fd = fd, .events = POLLIN};
	int ready;
	do {
		ready = poll(&p, 1, TIMEOUT_MS);
	} while (ready < 0 && errno == EINTR);
	if (ready < 0) {
		perror("poll");
		goto out;
	}
	if (!ready) {
		fprintf(stderr, "等待 %d ms 没有收到帧\n", TIMEOUT_MS);
		goto out;
	}

	struct v4l2_plane plane = {0};
	struct v4l2_buffer buf = {0};
	buf.type = type;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.length = 1;
	buf.m.planes = &plane;
	if (xioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
		perror("VIDIOC_DQBUF");
		goto out;
	}
	if (buf.index >= req.count ||
	    plane.data_offset > plane.bytesused ||
	    plane.bytesused > buffers[buf.index].length ||
	    (buf.flags & V4L2_BUF_FLAG_ERROR)) {
		fprintf(stderr, "收到无效帧\n");
		goto out;
	}

	/* 有效数据从 data_offset 开始，长度是 bytesused - data_offset；
	 * 不等于整个映射长度。 */
	size_t size = plane.bytesused - plane.data_offset;
	out = open(out_path, O_WRONLY | O_CREAT | O_EXCL, 0644);
	if (out < 0) {
		perror("open output");
		goto out;
	}
	if (write_all(out, (char *)buffers[buf.index].start + plane.data_offset,
		      size) < 0) {
		perror("write output");
		goto out;
	}

	clock_gettime(CLOCK_MONOTONIC, &end);
	double ms = (end.tv_sec - start.tv_sec) * 1000.0 +
		    (end.tv_nsec - start.tv_nsec) / 1e6;
	printf("sequence=%u bytesused=%u offset=%u saved=%u elapsed=%.1fms\n",
	       buf.sequence, plane.bytesused, plane.data_offset,
	       (unsigned)size, ms);

	if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
		perror("VIDIOC_QBUF return");
		goto out;
	}
	ret = 0;

out:
	if (streaming && xioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
		perror("VIDIOC_STREAMOFF");
		ret = 1;
	}
	for (uint32_t i = 0; i < BUFFER_COUNT; i++) {
		if (buffers[i].start)
			munmap(buffers[i].start, buffers[i].length);
	}
	if (req.count) {
		req.count = 0;
		xioctl(fd, VIDIOC_REQBUFS, &req);
	}
	if (out >= 0 && close(out) < 0) {
		perror("close output");
		ret = 1;
	}
	close(fd);
	return ret;
}
