/* 第十六课：连续采集并编码，把 Annex-B H.264 写入文件。
 * DMA-BUF 在启动时一次性导入所有采集缓冲区，循环里只按 index 取物理地址；
 * 编码回调里的两段数据在返回前写入文件。
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
#define FPS 60
#define BUFFER_COUNT 4
#define TIMEOUT_MS 2000
#define BITRATE_KBPS 12000

struct CedarDmaBuf {
	int fd;
	unsigned int phy_addr;
};
static const unsigned long CEDAR_MAP_DMA_BUF = 0x504;
static const unsigned long CEDAR_UNMAP_DMA_BUF = 0x505;

struct buffer {
	void *start;
	size_t length;
	int exported_fd;
	unsigned int phy;
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

static int write_all(int fd, const void *data, size_t size)
{
	const unsigned char *p = (const unsigned char *)data;
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

/* SDK 回调里的 isKeyframe 没有赋值（封装里的 AVPacket 未初始化），
 * 这里按 Annex-B 起始码后的 nal_unit_type 判断 IDR（类型 5）。 */
static int count_idr(const unsigned char *data, size_t size)
{
	int idrs = 0;
	size_t i = 0;
	while (i + 3 <= size) {
		size_t sc = 0;
		if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1)
			sc = 3;
		else if (i + 4 <= size && data[i] == 0 && data[i + 1] == 0 &&
			 data[i + 2] == 0 && data[i + 3] == 1)
			sc = 4;
		if (!sc) {
			i++;
			continue;
		}
		if (i + sc < size && (data[i + sc] & 0x1f) == 5)
			idrs++;
		i += sc;
	}
	return idrs;
}

class FileWriter : public AWVideoEncoderDataCallback {
public:
	int fd = -1;
	int config_bytes = 0;
	int frames = 0;
	int idr = 0;
	long frame_bytes = 0;
	int write_error = 0;

	int encoderDataReady(AVPacket *packet) override
	{
		if (!packet || !packet->pAddrVir0 || !packet->dataLen0)
			return -1;
		if (write_all(fd, packet->pAddrVir0, packet->dataLen0) < 0) {
			write_error = 1;
			return -1;
		}
		if (packet->dataLen1) {
			if (!packet->pAddrVir1 ||
			    write_all(fd, packet->pAddrVir1, packet->dataLen1) < 0) {
				write_error = 1;
				return -1;
			}
		}
		if (packet->pts == -1) {
			config_bytes += packet->dataLen0 + packet->dataLen1;
			return 0;
		}
		frames++;
		idr += count_idr((const unsigned char *)packet->pAddrVir0,
				 packet->dataLen0);
		frame_bytes += packet->dataLen0 + packet->dataLen1;
		return 0;
	}
};

int main(int argc, char **argv)
{
	const char *dev = argc > 1 ? argv[1] : "/dev/video0";
	const char *out_path = argc > 2 ? argv[2] : "capture.h264";
	long target = 120;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers req;
	struct v4l2_streamparm parm;
	struct v4l2_plane plane;
	struct v4l2_buffer buf;
	struct v4l2_exportbuffer exp;
	struct pollfd pfd;
	struct buffer buffers[BUFFER_COUNT];
	struct CedarDmaBuf mapping;
	EncodeParam param;
	AVPacket packet;
	AWVideoEncoder *encoder = NULL;
	FileWriter writer;
	int fd = -1, ve_fd = -1, out_fd = -1, streaming = 0, ret = 1;
	int ready, enc_ret;
	long captured = 0;
	double start, seconds;

	if (argc > 3) {
		char *end = NULL;
		errno = 0;
		target = strtol(argv[3], &end, 10);
		if (errno || !end || *end || target < 1 || target > 10000) {
			fprintf(stderr, "帧数用法：%s [设备] [输出文件] [帧数]\n", argv[0]);
			return 2;
		}
	}

	memset(&fmt, 0, sizeof(fmt));
	memset(&req, 0, sizeof(req));
	memset(&parm, 0, sizeof(parm));
	memset(&plane, 0, sizeof(plane));
	memset(&buf, 0, sizeof(buf));
	memset(&exp, 0, sizeof(exp));
	memset(&param, 0, sizeof(param));
	memset(&packet, 0, sizeof(packet));
	memset(buffers, 0, sizeof(buffers));
	for (int i = 0; i < BUFFER_COUNT; i++)
		buffers[i].exported_fd = -1;

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
	printf("input locked: %dx%d@%d\n", WIDTH, HEIGHT, FPS);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	fmt.fmt.pix_mp.width = WIDTH;
	fmt.fmt.pix_mp.height = HEIGHT;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
	fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
	if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
		perror("VIDIOC_S_FMT");
		goto out;
	}

	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	parm.parm.capture.timeperframe.numerator = 1;
	parm.parm.capture.timeperframe.denominator = FPS;
	parm.parm.capture.capturemode = 0x0002;
	if (xioctl(fd, VIDIOC_S_PARM, &parm) < 0) {
		perror("VIDIOC_S_PARM");
		goto out;
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
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

	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
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
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (xioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
		perror("VIDIOC_G_FMT after REQBUFS");
		goto out;
	}
	printf("buffers=%u sizeimage=%u bytesperline=%u\n", req.count,
	       fmt.fmt.pix_mp.plane_fmt[0].sizeimage,
	       fmt.fmt.pix_mp.plane_fmt[0].bytesperline);

	ve_fd = open("/dev/cedar_dev", O_RDWR | O_CLOEXEC);
	if (ve_fd < 0) {
		perror("/dev/cedar_dev");
		goto out;
	}

	/* 每个采集缓冲区只导入一次，循环里按 index 取物理地址。 */
	for (uint32_t i = 0; i < req.count; i++) {
		memset(&plane, 0, sizeof(plane));
		memset(&buf, 0, sizeof(buf));
		buf.type = req.type;
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

		memset(&exp, 0, sizeof(exp));
		exp.type = req.type;
		exp.index = i;
		exp.plane = 0;
		exp.flags = O_CLOEXEC;
		if (xioctl(fd, VIDIOC_EXPBUF, &exp) < 0) {
			perror("VIDIOC_EXPBUF");
			goto out;
		}
		buffers[i].exported_fd = exp.fd;
		mapping.fd = exp.fd;
		mapping.phy_addr = 0;
		if (ioctl(ve_fd, CEDAR_MAP_DMA_BUF, &mapping) < 0) {
			perror("CEDAR_MAP_DMA_BUF");
			goto out;
		}
		buffers[i].phy = mapping.phy_addr;
		printf("buffer[%u] size=%u phy=0x%x\n", i, plane.length,
		       buffers[i].phy);

		if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
			perror("VIDIOC_QBUF");
			goto out;
		}
	}

	out_fd = open(out_path, O_WRONLY | O_CREAT | O_EXCL, 0644);
	if (out_fd < 0) {
		perror("open output");
		goto out;
	}
	writer.fd = out_fd;

	param.codecType = CODEC_H264;
	param.pixelFormat = PIXEL_YUV420SP;
	param.srcW = WIDTH;
	param.srcH = HEIGHT;
	param.dstW = WIDTH;
	param.dstH = HEIGHT;
	param.rotation = Angle_0;
	param.bitRate = BITRATE_KBPS * 1000;
	param.frameRate = FPS;
	param.maxKeyFrame = FPS;
	param.frameCount = MULTI_FRAMES;
	param.rcMode = VBR;
	param.minQp = 20;
	param.maxQp = 28;

	encoder = AWVideoEncoder::create();
	if (!encoder) {
		fprintf(stderr, "AWVideoEncoder::create 失败\n");
		goto out;
	}
	if (encoder->init(&param, &writer) != 0) {
		fprintf(stderr, "编码器 init 失败\n");
		goto out;
	}

	if (xioctl(fd, VIDIOC_STREAMON, &req.type) < 0) {
		perror("VIDIOC_STREAMON");
		goto out;
	}
	streaming = 1;

	printf("encoding %ld frames to %s\n", target, out_path);
	start = now_ms();
	while (captured < target) {
		pfd.fd = fd;
		pfd.events = POLLIN;
		do {
			ready = poll(&pfd, 1, TIMEOUT_MS);
		} while (ready < 0 && errno == EINTR);
		if (ready < 0) {
			perror("poll");
			goto out;
		}
		if (!ready) {
			fprintf(stderr, "等待 %d ms 没有收到帧\n", TIMEOUT_MS);
			goto out;
		}

		memset(&plane, 0, sizeof(plane));
		memset(&buf, 0, sizeof(buf));
		buf.type = req.type;
		buf.memory = req.memory;
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

		memset(&packet, 0, sizeof(packet));
		packet.pAddrPhy0 = (unsigned char *)(uintptr_t)buffers[buf.index].phy;
		packet.dataLen0 = WIDTH * HEIGHT;
		packet.pAddrPhy1 = (unsigned char *)(uintptr_t)(buffers[buf.index].phy +
								WIDTH * HEIGHT);
		packet.dataLen1 = WIDTH * HEIGHT / 2;
		packet.pts = captured * 1000 / FPS;
		packet.id = (unsigned long)captured;
		packet.isKeyframe = (captured % FPS == 0);

		enc_ret = encoder->encode(&packet);
		if (enc_ret < 0 || writer.write_error) {
			fprintf(stderr, "编码失败: ret=%d write_error=%d\n",
				enc_ret, writer.write_error);
			goto out;
		}
		if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
			perror("VIDIOC_QBUF return");
			goto out;
		}
		captured++;
	}

	seconds = (now_ms() - start) / 1000.0;
	printf("captured=%ld encoded=%d idr=%d elapsed=%.3fs fps=%.2f\n",
	       captured, writer.frames, writer.idr, seconds,
	       seconds > 0 ? writer.frames / seconds : 0.0);
	printf("file=%s config=%dB frames=%ldB total=%ldB\n", out_path,
	       writer.config_bytes, writer.frame_bytes,
	       (long)writer.config_bytes + writer.frame_bytes);
	ret = (writer.frames == target) ? 0 : 1;

out:
	if (encoder)
		AWVideoEncoder::destroy(encoder);
	if (ve_fd >= 0) {
		for (uint32_t i = 0; i < req.count && i < BUFFER_COUNT; i++) {
			if (buffers[i].phy) {
				struct CedarDmaBuf unmap = {buffers[i].exported_fd, 0};
				ioctl(ve_fd, CEDAR_UNMAP_DMA_BUF, &unmap);
			}
		}
		close(ve_fd);
	}
	for (uint32_t i = 0; i < BUFFER_COUNT; i++) {
		if (buffers[i].exported_fd >= 0)
			close(buffers[i].exported_fd);
		if (buffers[i].start)
			munmap(buffers[i].start, buffers[i].length);
	}
	if (streaming) {
		enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		if (xioctl(fd, VIDIOC_STREAMOFF, &type) < 0)
			perror("VIDIOC_STREAMOFF");
	}
	if (req.count) {
		req.count = 0;
		xioctl(fd, VIDIOC_REQBUFS, &req);
	}
	if (out_fd >= 0 && close(out_fd) < 0) {
		perror("close output");
		ret = 1;
	}
	if (fd >= 0)
		close(fd);
	return ret;
}
