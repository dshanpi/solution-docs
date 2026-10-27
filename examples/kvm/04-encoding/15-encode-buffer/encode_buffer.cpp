/* 第十五课：把采集缓冲区导入硬件编码器。
 * 采集一帧 NV12，导出 DMA-BUF，经 /dev/cedar_dev 映射成编码器可用的物理地址，
 * 直接交给编码器，不复制图像；回调里统计 SPS/PPS 和码流包。
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

#include "AWVideoEncoder.h"

using namespace awvideoencoder;

#define WIDTH 1920
#define HEIGHT 1080
#define BUFFER_COUNT 4
#define TIMEOUT_MS 2000
#define BITRATE_KBPS 12000

/* Tina 5.0 cedar-ve UAPI，定义见 bsp/drivers/ve/cedar-ve/cedar_ve.h。
 * VIN 的物理地址不能直接给编码器，要先导入 VE 的地址空间。 */
struct CedarDmaBuf {
	int fd;
	unsigned int phy_addr;
};
static const unsigned long CEDAR_MAP_DMA_BUF = 0x504;
static const unsigned long CEDAR_UNMAP_DMA_BUF = 0x505;

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

static double now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

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

class PacketCounter : public AWVideoEncoderDataCallback {
public:
	int config_bytes = 0;
	int frames = 0;
	long total_bytes = 0;

	int encoderDataReady(AVPacket *packet) override
	{
		if (!packet || !packet->pAddrVir0 || !packet->dataLen0)
			return -1;
		if (packet->pts == -1) {
			config_bytes += packet->dataLen0 + packet->dataLen1;
			printf("config: data0=%u data1=%u\n", packet->dataLen0,
			       packet->dataLen1);
			return 0;
		}
		frames++;
		total_bytes += packet->dataLen0 + packet->dataLen1;
		printf("frame=%d data0=%u data1=%u\n", frames, packet->dataLen0,
		       packet->dataLen1);
		return 0;
	}
};

int main(int argc, char **argv)
{
	const char *dev = argc > 1 ? argv[1] : "/dev/video0";
	struct v4l2_format fmt = {0};
	struct v4l2_requestbuffers req = {0};
	struct buffer buffers[BUFFER_COUNT] = {{0}};
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	AWVideoEncoder *encoder = NULL;
	PacketCounter callback;
	int fd = -1, ve_fd = -1, streaming = 0, exported_fd = -1, ret = 1;
	unsigned int imported_phy = 0;

	struct v4l2_streamparm parm;
	struct pollfd p;
	struct v4l2_plane plane;
	struct v4l2_buffer buf;
	struct v4l2_exportbuffer exp;
	struct CedarDmaBuf mapping;
	EncodeParam param;
	AVPacket packet;
	int ready, enc_ret;
	double t0, enc_ms;

	fd = open(dev, O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		perror(dev);
		return 1;
	}

	int input_src = 0;
	if (xioctl(fd, VIDIOC_S_INPUT, &input_src) < 0) {
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

	memset(&parm, 0, sizeof(parm));
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

	p.fd = fd;
	p.events = POLLIN;
	do {
		ready = poll(&p, 1, TIMEOUT_MS);
	} while (ready < 0 && errno == EINTR);
	if (!ready) {
		fprintf(stderr, "等待 %d ms 没有收到帧\n", TIMEOUT_MS);
		goto out;
	}
	if (ready < 0) {
		perror("poll");
		goto out;
	}

	memset(&plane, 0, sizeof(plane));
	memset(&buf, 0, sizeof(buf));
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
	    plane.bytesused > buffers[buf.index].length) {
		fprintf(stderr, "收到无效帧\n");
		goto out;
	}
	printf("captured: index=%u sequence=%u bytesused=%u\n", buf.index,
	       buf.sequence, plane.bytesused);

	/* 导出这一帧的 DMA-BUF，再导入 VE 地址空间。 */
	memset(&exp, 0, sizeof(exp));
	exp.type = type;
	exp.index = buf.index;
	exp.plane = 0;
	exp.flags = O_CLOEXEC;
	if (xioctl(fd, VIDIOC_EXPBUF, &exp) < 0) {
		perror("VIDIOC_EXPBUF");
		goto out;
	}
	exported_fd = exp.fd;

	ve_fd = open("/dev/cedar_dev", O_RDWR | O_CLOEXEC);
	if (ve_fd < 0) {
		perror("/dev/cedar_dev");
		goto out;
	}
	mapping.fd = exported_fd;
	mapping.phy_addr = 0;
	if (ioctl(ve_fd, CEDAR_MAP_DMA_BUF, &mapping) < 0) {
		perror("CEDAR_MAP_DMA_BUF");
		goto out;
	}
	imported_phy = mapping.phy_addr;
	printf("VE import: fd=%d phy=0x%x\n", exported_fd, imported_phy);

	memset(&param, 0, sizeof(param));
	param.codecType = CODEC_H264;
	param.pixelFormat = PIXEL_YUV420SP;
	param.srcW = WIDTH;
	param.srcH = HEIGHT;
	param.dstW = WIDTH;
	param.dstH = HEIGHT;
	param.rotation = Angle_0;
	param.bitRate = BITRATE_KBPS * 1000;
	param.frameRate = 60;
	param.maxKeyFrame = 60;
	param.frameCount = MULTI_FRAMES;
	param.rcMode = VBR;
	param.minQp = 20;
	param.maxQp = 28;

	encoder = AWVideoEncoder::create();
	if (!encoder) {
		fprintf(stderr, "AWVideoEncoder::create 失败\n");
		goto out;
	}
	if (encoder->init(&param, &callback) != 0) {
		fprintf(stderr, "编码器 init 失败\n");
		goto out;
	}
	printf("encoder: H.264 %dx%d %dfps %dkbps VBR QP %d-%d\n", WIDTH,
	       HEIGHT, param.frameRate, BITRATE_KBPS, param.minQp, param.maxQp);

	memset(&packet, 0, sizeof(packet));
	packet.pAddrPhy0 = (unsigned char *)(uintptr_t)imported_phy;
	packet.dataLen0 = WIDTH * HEIGHT;
	packet.pAddrPhy1 = (unsigned char *)(uintptr_t)(imported_phy +
							WIDTH * HEIGHT);
	packet.dataLen1 = WIDTH * HEIGHT / 2;
	packet.pts = 0;
	packet.id = 0;
	packet.isKeyframe = true;

	t0 = now_ms();
	enc_ret = encoder->encode(&packet);
	enc_ms = now_ms() - t0;
	if (enc_ret < 0) {
		fprintf(stderr, "encode 失败: %d\n", enc_ret);
		goto out;
	}
	printf("encode: ret=%d elapsed=%.1fms config=%dB frames=%d bytes=%ld\n",
	       enc_ret, enc_ms, callback.config_bytes, callback.frames,
	       callback.total_bytes);
	ret = 0;

out:
	if (encoder)
		AWVideoEncoder::destroy(encoder);
	if (ve_fd >= 0 && imported_phy) {
		struct CedarDmaBuf unmap = {exported_fd, 0};
		ioctl(ve_fd, CEDAR_UNMAP_DMA_BUF, &unmap);
	}
	if (ve_fd >= 0)
		close(ve_fd);
	if (exported_fd >= 0)
		close(exported_fd);
	if (streaming && xioctl(fd, VIDIOC_STREAMOFF, &type) < 0)
		perror("VIDIOC_STREAMOFF");
	for (uint32_t i = 0; i < BUFFER_COUNT; i++) {
		if (buffers[i].start)
			munmap(buffers[i].start, buffers[i].length);
	}
	if (req.count) {
		req.count = 0;
		xioctl(fd, VIDIOC_REQBUFS, &req);
	}
	if (fd >= 0)
		close(fd);
	return ret;
}
