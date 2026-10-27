/* 第十四课：连续采集。
 * 循环使用同一组 buffer：poll 等帧、DQBUF 取帧、处理后 QBUF 还回驱动。
 * 支持指定帧数，Ctrl-C 可提前结束，退出前统一释放资源。
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <signal.h>
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

static volatile sig_atomic_t g_stop;

static void on_signal(int sig)
{
	(void)sig;
	g_stop = 1;
}

static int xioctl(int fd, unsigned long req, void *arg)
{
	int ret;
	do {
		ret = ioctl(fd, req, arg);
	} while (ret < 0 && errno == EINTR);
	return ret;
}

static double now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
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
	long target = 300;
	struct v4l2_format fmt = {0};
	struct v4l2_requestbuffers req = {0};
	struct buffer buffers[BUFFER_COUNT] = {{0}};
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	int fd = -1, streaming = 0, ret = 1;
	long received = 0, gaps = 0, errors = 0;
	uint32_t last_sequence = 0;
	int have_last = 0;

	if (argc > 2) {
		char *end = NULL;
		errno = 0;
		target = strtol(argv[2], &end, 10);
		if (errno || !end || *end || target < 1 || target > 100000) {
			fprintf(stderr, "帧数用法：%s [设备] [帧数]\n", argv[0]);
			return 2;
		}
	}

	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

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

	/* 1/60 秒一帧；capturemode 0x0002 是这块板 VIN 的采集模式。 */
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

	/* VIN 在 REQBUFS 里才计算 sizeimage 和 bytesperline。 */
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

	double start = now_ms();
	while (received < target && !g_stop) {
		struct pollfd p = {.fd = fd, .events = POLLIN};
		int ready;
		do {
			ready = poll(&p, 1, TIMEOUT_MS);
		} while (ready < 0 && errno == EINTR && !g_stop);

		if (g_stop)
			break;
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
			if (errno == EAGAIN)
				continue;
			perror("VIDIOC_DQBUF");
			goto out;
		}
		if (buf.index >= req.count ||
		    plane.data_offset > plane.bytesused ||
		    plane.bytesused > buffers[buf.index].length) {
			fprintf(stderr, "收到无效帧\n");
			goto out;
		}
		if (buf.flags & V4L2_BUF_FLAG_ERROR)
			errors++;

		/* QBUF 之后驱动会清空结构体字段，先把序号取出来。 */
		uint32_t sequence = buf.sequence;
		if (have_last && sequence != last_sequence + 1)
			gaps += sequence - last_sequence - 1;
		last_sequence = sequence;
		have_last = 1;

		/* 这一课只统计，不保存图像；处理完必须把 buffer 还回驱动。 */
		if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
			perror("VIDIOC_QBUF return");
			goto out;
		}

		received++;
		if (received % 60 == 0) {
			double elapsed = (now_ms() - start) / 1000.0;
			printf("frame=%ld sequence=%u fps=%.1f\n", received,
			       sequence, received / elapsed);
		}
	}

	double seconds = (now_ms() - start) / 1000.0;
	printf("frames=%ld elapsed=%.3fs fps=%.2f gaps=%ld errors=%ld\n",
	       received, seconds, seconds > 0 ? received / seconds : 0.0,
	       gaps, errors);

	ret = (received == target && errors == 0) ? 0 : 1;

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
	close(fd);
	return ret;
}
