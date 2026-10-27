/* 第 23 课：把第 12–17 课的采集与编码接到 Go 网络服务。
 * 程序连接控制 Socket 接收 start_video / stop_video / request_keyframe，
 * 把编码结果按 KVM2 包头写到数据 Socket。
 */
#define _POSIX_C_SOURCE 200809L
#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <mutex>
#include <poll.h>
#include <signal.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <vector>

#include "AWVideoEncoder.h"

using namespace awvideoencoder;

static constexpr int WIDTH = 1920;
static constexpr int HEIGHT = 1080;
static constexpr int FPS = 60;
static constexpr int BUFFER_COUNT = 4;
static constexpr int TIMEOUT_MS = 2000;
static constexpr int BITRATE_KBPS = 12000;
static constexpr size_t MAX_HEADER = 64 * 1024;

struct CedarDmaBuf {
	int fd;
	unsigned int phy_addr;
};
static constexpr unsigned long CEDAR_MAP_DMA_BUF = 0x504;
static constexpr unsigned long CEDAR_UNMAP_DMA_BUF = 0x505;

struct buffer {
	void *start = nullptr;
	size_t length = 0;
	int exported_fd = -1;
	unsigned int phy = 0;
};

static std::string g_ctrl_path;
static std::string g_data_path;
static std::atomic<bool> g_running{true};
static std::atomic<bool> g_active{false};
static std::atomic<bool> g_capture_running{false};
static std::atomic<bool> g_force_idr{true};
static std::atomic<unsigned long> g_sent_frames{0};
static std::atomic<unsigned long> g_encoded_frames{0};
static long g_max_frames = 0;
static int g_ctrl_fd = -1;
static std::atomic<bool> g_ctrl_broken{false};
static std::mutex g_ctrl_mutex;
static int g_data_fd = -1;
static std::mutex g_data_mutex;
static std::vector<uint8_t> g_codec_header;
static uint64_t g_capture_us = 0;
static int64_t g_last_data_log_ms = 0;

static int64_t now_ms()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static uint64_t now_us()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static void log_line(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "[%lld] ", (long long)now_ms());
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	fflush(stderr);
	va_end(ap);
}

static const char *env_or(const char *name, const char *fallback)
{
	const char *value = getenv(name);
	return value && *value ? value : fallback;
}

static int xioctl(int fd, unsigned long req, void *arg)
{
	int ret;
	do {
		ret = ioctl(fd, req, arg);
	} while (ret < 0 && errno == EINTR);
	return ret;
}

static bool write_all(int fd, const void *data, size_t size)
{
	const unsigned char *p = (const unsigned char *)data;
	while (size) {
		ssize_t n = send(fd, p, size, MSG_NOSIGNAL);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			return false;
		p += n;
		size -= (size_t)n;
	}
	return true;
}

static void set_send_timeout(int fd)
{
	struct timeval tv = {0, 100 * 1000};
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

static int input_locked(int fd)
{
	struct v4l2_dv_timings t = {};
	if (ioctl(fd, VIDIOC_QUERY_DV_TIMINGS, &t) < 0)
		return 0;
	if (t.type != V4L2_DV_BT_656_1120)
		return 0;
	const struct v4l2_bt_timings *b = &t.bt;
	if (!b->width || !b->height || !b->pixelclock || b->interlaced)
		return 0;
	uint64_t line = (uint64_t)b->width + b->hfrontporch + b->hsync + b->hbackporch;
	uint64_t rows = (uint64_t)b->height + b->vfrontporch + b->vsync + b->vbackporch;
	double rate = (double)b->pixelclock / (double)(line * rows);
	return b->width == WIDTH && b->height == HEIGHT && rate >= 59.0 && rate <= 61.0;
}

/* 返回 0 锁定，-1 超时，-2 收到停止命令 */
static int wait_for_input(int fd, int timeout_ms)
{
	int stable = 0;
	for (int waited = 0; waited <= timeout_ms; waited += 100) {
		if (!g_running || !g_active)
			return -2;
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

static std::string json_string(const std::string &line, const char *key)
{
	std::string needle = std::string("\"") + key + "\"";
	size_t p = line.find(needle);
	if (p == std::string::npos)
		return {};
	p = line.find(':', p + needle.size());
	if (p == std::string::npos)
		return {};
	p = line.find('"', p + 1);
	if (p == std::string::npos)
		return {};
	size_t e = line.find('"', p + 1);
	if (e == std::string::npos)
		return {};
	return line.substr(p + 1, e - p - 1);
}

static bool json_int(const std::string &line, const char *key, long *out)
{
	std::string needle = std::string("\"") + key + "\"";
	size_t p = line.find(needle);
	if (p == std::string::npos)
		return false;
	p = line.find(':', p + needle.size());
	if (p == std::string::npos)
		return false;
	char *end = nullptr;
	long value = strtol(line.c_str() + p + 1, &end, 10);
	if (end == line.c_str() + p + 1)
		return false;
	if (out)
		*out = value;
	return true;
}

/* 调用者持有 g_ctrl_mutex */
static bool ctrl_send_locked(const std::string &line)
{
	if (g_ctrl_fd < 0)
		return false;
	if (!write_all(g_ctrl_fd, line.data(), line.size())) {
		/* 控制线程负责关闭和重连，这里只标记，避免 fd 被复用时另一线程还在 poll。 */
		g_ctrl_broken = true;
		return false;
	}
	return true;
}

static void send_ctrl_line(const std::string &line)
{
	std::lock_guard<std::mutex> lock(g_ctrl_mutex);
	ctrl_send_locked(line);
}

static void send_video_state(bool ready, const char *error, int width, int height, int fps)
{
	char line[256];
	snprintf(line, sizeof(line),
		 "{\"event\":\"video_input_state\",\"data\":{\"ready\":%s,\"error\":\"%s\","
		 "\"width\":%d,\"height\":%d,\"fps\":%d}}\n",
		 ready ? "true" : "false", error, width, height, fps);
	send_ctrl_line(line);
}

/* 调用者持有 g_data_mutex。返回 false 时连接已关闭，下一帧再重连。 */
static bool send_packet_locked(const uint8_t *a, size_t an, const uint8_t *b,
			       size_t bn, uint64_t capture_us, bool config)
{
	if (g_data_fd < 0)
		return false;
	size_t total = an + bn;
	if (!total || total > 4147200) {
		log_line("invalid video length %zu", total);
		return false;
	}
	uint8_t header[20];
	memcpy(header, "KVM2", 4);
	uint32_t length = (uint32_t)total;
	uint64_t timestamp = config ? 0 : capture_us;
	uint32_t flags = config ? 1 : 0;
	memcpy(header + 4, &length, 4);
	memcpy(header + 8, &timestamp, 8);
	memcpy(header + 16, &flags, 4);

	if (!write_all(g_data_fd, header, sizeof(header)) ||
	    !write_all(g_data_fd, a, an) ||
	    (bn && !write_all(g_data_fd, b, bn))) {
		log_line("video send failed: %s; closing data socket", strerror(errno));
		close(g_data_fd);
		g_data_fd = -1;
		g_force_idr = true;
		return false;
	}
	return true;
}

static bool ensure_data_connected()
{
	std::lock_guard<std::mutex> lock(g_data_mutex);
	if (g_data_fd >= 0)
		return true;
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return false;
	struct sockaddr_un addr = {};
	addr.sun_family = AF_UNIX;
	if (g_data_path.size() >= sizeof(addr.sun_path)) {
		close(fd);
		return false;
	}
	strncpy(addr.sun_path, g_data_path.c_str(), sizeof(addr.sun_path) - 1);
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(fd);
		int64_t now = now_ms();
		if (now - g_last_data_log_ms > 2000) {
			g_last_data_log_ms = now;
			log_line("waiting for data socket %s: %s", g_data_path.c_str(), strerror(errno));
		}
		return false;
	}
	set_send_timeout(fd);
	g_data_fd = fd;
	if (!g_codec_header.empty())
		send_packet_locked(g_codec_header.data(), g_codec_header.size(), nullptr, 0, 0, true);
	g_force_idr = true;
	log_line("data socket connected: %s", g_data_path.c_str());
	return true;
}

class Sender : public AWVideoEncoderDataCallback {
public:
	int encoderDataReady(AVPacket *packet) override
	{
		if (!packet || !packet->pAddrVir0 || !packet->dataLen0 ||
		    (packet->dataLen1 && !packet->pAddrVir1))
			return -1;

		std::lock_guard<std::mutex> lock(g_data_mutex);
		if (packet->pts == -1) {
			if ((size_t)packet->dataLen0 + packet->dataLen1 > MAX_HEADER)
				return -1;
			g_codec_header.assign(packet->pAddrVir0,
					      packet->pAddrVir0 + packet->dataLen0);
			if (packet->dataLen1)
				g_codec_header.insert(g_codec_header.end(), packet->pAddrVir1,
						      packet->pAddrVir1 + packet->dataLen1);
			if (g_data_fd >= 0)
				send_packet_locked(g_codec_header.data(),
						   g_codec_header.size(), nullptr, 0, 0, true);
			log_line("encoder config packet %zu bytes", g_codec_header.size());
			return 0;
		}

		if (send_packet_locked((const uint8_t *)packet->pAddrVir0,
				       (size_t)packet->dataLen0,
				       (const uint8_t *)packet->pAddrVir1,
				       (size_t)packet->dataLen1, g_capture_us, false))
			++g_sent_frames;
		++g_encoded_frames;
		return 0;
	}
};

static Sender g_sender;
static AWVideoEncoder *g_encoder = nullptr;

static bool setup_capture(int &fd, int &ve_fd, struct buffer *buffers,
			  uint32_t &count, int &width, int &height)
{
	fd = open("/dev/video0", O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		log_line("open /dev/video0 failed: %s", strerror(errno));
		return false;
	}
	int input = 0;
	if (xioctl(fd, VIDIOC_S_INPUT, &input) < 0) {
		log_line("VIDIOC_S_INPUT failed: %s", strerror(errno));
		return false;
	}
	send_video_state(false, "no_lock", 0, 0, 0);
	log_line("waiting for HDMI lock");
	int lock = wait_for_input(fd, 10000);
	if (lock != 0) {
		send_video_state(false, lock == -1 ? "no_signal" : "", 0, 0, 0);
		return false;
	}

	struct v4l2_format fmt = {};
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	fmt.fmt.pix_mp.width = WIDTH;
	fmt.fmt.pix_mp.height = HEIGHT;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
	fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
	if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
		log_line("VIDIOC_S_FMT failed: %s", strerror(errno));
		return false;
	}

	struct v4l2_streamparm parm = {};
	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	parm.parm.capture.timeperframe.numerator = 1;
	parm.parm.capture.timeperframe.denominator = FPS;
	parm.parm.capture.capturemode = 0x0002;
	if (xioctl(fd, VIDIOC_S_PARM, &parm) < 0) {
		log_line("VIDIOC_S_PARM failed: %s", strerror(errno));
		return false;
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (xioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
		log_line("VIDIOC_G_FMT failed: %s", strerror(errno));
		return false;
	}
	if (fmt.fmt.pix_mp.pixelformat != V4L2_PIX_FMT_NV12 ||
	    fmt.fmt.pix_mp.width != WIDTH || fmt.fmt.pix_mp.height != HEIGHT ||
	    fmt.fmt.pix_mp.num_planes != 1) {
		log_line("driver format does not match request");
		return false;
	}
	width = (int)fmt.fmt.pix_mp.width;
	height = (int)fmt.fmt.pix_mp.height;

	struct v4l2_requestbuffers req = {};
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	req.memory = V4L2_MEMORY_MMAP;
	req.count = BUFFER_COUNT;
	if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
		log_line("VIDIOC_REQBUFS failed: %s", strerror(errno));
		return false;
	}
	if (req.count < 2) {
		log_line("driver allocated only %u buffers", req.count);
		return false;
	}
	count = req.count;

	ve_fd = open("/dev/cedar_dev", O_RDWR | O_CLOEXEC);
	if (ve_fd < 0) {
		log_line("open /dev/cedar_dev failed: %s", strerror(errno));
		return false;
	}

	for (uint32_t i = 0; i < count; i++) {
		struct v4l2_plane plane = {};
		struct v4l2_buffer buf = {};
		buf.type = req.type;
		buf.memory = req.memory;
		buf.index = i;
		buf.length = 1;
		buf.m.planes = &plane;
		if (xioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
			log_line("VIDIOC_QUERYBUF[%u] failed: %s", i, strerror(errno));
			return false;
		}
		buffers[i].length = plane.length;
		buffers[i].start = mmap(NULL, plane.length, PROT_READ | PROT_WRITE,
					MAP_SHARED, fd, plane.m.mem_offset);
		if (buffers[i].start == MAP_FAILED) {
			buffers[i].start = nullptr;
			log_line("mmap[%u] failed: %s", i, strerror(errno));
			return false;
		}

		struct v4l2_exportbuffer exp = {};
		exp.type = req.type;
		exp.index = i;
		exp.plane = 0;
		exp.flags = O_CLOEXEC;
		if (xioctl(fd, VIDIOC_EXPBUF, &exp) < 0) {
			log_line("VIDIOC_EXPBUF[%u] failed: %s", i, strerror(errno));
			return false;
		}
		buffers[i].exported_fd = exp.fd;
		CedarDmaBuf mapping = {exp.fd, 0};
		if (ioctl(ve_fd, CEDAR_MAP_DMA_BUF, &mapping) < 0) {
			log_line("CEDAR_MAP_DMA_BUF[%u] failed: %s", i, strerror(errno));
			return false;
		}
		buffers[i].phy = mapping.phy_addr;
		log_line("capture buffer[%u] size=%zu phy=0x%x", i, plane.length, buffers[i].phy);

		if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
			log_line("VIDIOC_QBUF[%u] failed: %s", i, strerror(errno));
			return false;
		}
	}
	return true;
}

static void release_capture(int fd, int ve_fd, struct buffer *buffers, uint32_t count,
			    bool streaming)
{
	if (streaming) {
		enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		if (xioctl(fd, VIDIOC_STREAMOFF, &type) < 0)
			log_line("VIDIOC_STREAMOFF failed: %s", strerror(errno));
	}
	for (uint32_t i = 0; i < count; i++) {
		if (ve_fd >= 0 && buffers[i].phy) {
			CedarDmaBuf unmap = {buffers[i].exported_fd, 0};
			ioctl(ve_fd, CEDAR_UNMAP_DMA_BUF, &unmap);
		}
		if (buffers[i].exported_fd >= 0)
			close(buffers[i].exported_fd);
		if (buffers[i].start)
			munmap(buffers[i].start, buffers[i].length);
		buffers[i] = {};
		buffers[i].exported_fd = -1;
	}
	if (ve_fd >= 0)
		close(ve_fd);
	if (fd >= 0) {
		struct v4l2_requestbuffers req = {};
		req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		req.memory = V4L2_MEMORY_MMAP;
		req.count = 0;
		xioctl(fd, VIDIOC_REQBUFS, &req);
		close(fd);
	}
}

static int capture_session()
{
	int fd = -1, ve_fd = -1;
	uint32_t count = 0;
	int width = 0, height = 0;
	bool streaming = false;
	struct buffer buffers[BUFFER_COUNT];
	for (auto &b : buffers)
		b.exported_fd = -1;

	if (!g_encoder) {
		log_line("encoder not initialized");
		return -1;
	}
	g_force_idr = true;
	g_capture_running = true;
	if (!setup_capture(fd, ve_fd, buffers, count, width, height)) {
		release_capture(fd, ve_fd, buffers, count, false);
		g_capture_running = false;
		return -1;
	}

	ensure_data_connected();
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (xioctl(fd, VIDIOC_STREAMON, &type) < 0) {
		log_line("VIDIOC_STREAMON failed: %s", strerror(errno));
		release_capture(fd, ve_fd, buffers, count, false);
		g_capture_running = false;
		return -1;
	}
	streaming = true;
	send_video_state(true, "", width, height, FPS);
	log_line("capture started %dx%d@%d", width, height, FPS);

	int64_t window_start = now_ms();
	unsigned long window_frames = 0;
	while (g_running && g_active) {
		if (!ensure_data_connected()) {
			/* 没有网络服务时仍然采集，重连后从 IDR 恢复。 */
		}
		struct pollfd pfd = {.fd = fd, .events = POLLIN};
		int ready;
		do {
			ready = poll(&pfd, 1, TIMEOUT_MS);
		} while (ready < 0 && errno == EINTR);
		if (ready < 0) {
			log_line("poll failed: %s", strerror(errno));
			break;
		}
		if (!ready) {
			log_line("no frame within %d ms", TIMEOUT_MS);
			send_video_state(false, "no_signal", 0, 0, 0);
			break;
		}

		struct v4l2_plane plane = {};
		struct v4l2_buffer buf = {};
		buf.type = type;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.length = 1;
		buf.m.planes = &plane;
		if (xioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
			if (errno == EAGAIN)
				continue;
			log_line("VIDIOC_DQBUF failed: %s", strerror(errno));
			break;
		}
		if (buf.index >= count || plane.data_offset > plane.bytesused ||
		    plane.bytesused > buffers[buf.index].length) {
			log_line("invalid frame");
			break;
		}
		uint32_t index = buf.index;
		struct timeval ts = buf.timestamp;
		g_capture_us = ts.tv_sec || ts.tv_usec
			? (uint64_t)ts.tv_sec * 1000000 + ts.tv_usec
			: now_us();

		AVPacket packet = {};
		packet.pAddrPhy0 = (unsigned char *)(uintptr_t)buffers[index].phy;
		packet.dataLen0 = WIDTH * HEIGHT;
		packet.pAddrPhy1 = (unsigned char *)(uintptr_t)(buffers[index].phy +
								WIDTH * HEIGHT);
		packet.dataLen1 = WIDTH * HEIGHT / 2;
		packet.pts = (int64_t)(g_capture_us / 1000);
		packet.id = g_encoded_frames.load();
		packet.isKeyframe = g_force_idr.exchange(false);

		if (g_encoder->encode(&packet) < 0) {
			log_line("encode failed");
			xioctl(fd, VIDIOC_QBUF, &buf);
			break;
		}
		if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
			log_line("QBUF return failed: %s", strerror(errno));
			break;
		}
		++window_frames;
		int64_t now = now_ms();
		if (now - window_start >= 2000) {
			log_line("capture fps=%.1f sent=%lu encoded=%lu",
			     window_frames * 1000.0 / (now - window_start),
			     g_sent_frames.load(), g_encoded_frames.load());
			window_start = now;
			window_frames = 0;
		}
		if (g_max_frames > 0 && g_encoded_frames >= (unsigned long)g_max_frames) {
			g_running = false;
			break;
		}
	}

	release_capture(fd, ve_fd, buffers, count, streaming);
	g_capture_running = false;
	send_video_state(false, "", 0, 0, 0);
	log_line("capture stopped");
	return 0;
}

static bool connect_ctrl()
{
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return false;
	struct sockaddr_un addr = {};
	addr.sun_family = AF_UNIX;
	if (g_ctrl_path.size() >= sizeof(addr.sun_path)) {
		close(fd);
		return false;
	}
	strncpy(addr.sun_path, g_ctrl_path.c_str(), sizeof(addr.sun_path) - 1);
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(fd);
		return false;
	}
	set_send_timeout(fd);
	std::lock_guard<std::mutex> lock(g_ctrl_mutex);
	if (g_ctrl_fd >= 0)
		close(g_ctrl_fd);
	g_ctrl_fd = fd;
	log_line("ctrl socket connected: %s", g_ctrl_path.c_str());
	return true;
}

static void control_loop()
{
	std::string pending;
	int64_t last_retry = 0;
	while (g_running) {
		if (g_ctrl_broken.exchange(false)) {
			std::lock_guard<std::mutex> lock(g_ctrl_mutex);
			if (g_ctrl_fd >= 0)
				close(g_ctrl_fd);
			g_ctrl_fd = -1;
			pending.clear();
		}
		if (g_ctrl_fd < 0) {
			if (now_ms() - last_retry >= 2000) {
				last_retry = now_ms();
				if (!connect_ctrl() && now_ms() - last_retry >= 2000)
					log_line("waiting for ctrl socket %s: %s", g_ctrl_path.c_str(),
					     strerror(errno));
			}
			struct timespec ts = {.tv_sec = 0, .tv_nsec = 100 * 1000 * 1000};
			nanosleep(&ts, NULL);
			continue;
		}

		struct pollfd pfd = {.fd = g_ctrl_fd, .events = POLLIN};
		int ready = poll(&pfd, 1, 500);
		if (ready < 0 && errno == EINTR)
			continue;
		if (ready < 0) {
			log_line("ctrl socket poll failed: %s", strerror(errno));
			break;
		}
		if (!ready)
			continue;

		char buffer[4096];
		ssize_t n = recv(g_ctrl_fd, buffer, sizeof(buffer), 0);
		if (n <= 0) {
			log_line("ctrl socket closed; reconnecting");
			std::lock_guard<std::mutex> lock(g_ctrl_mutex);
			if (g_ctrl_fd >= 0)
				close(g_ctrl_fd);
			g_ctrl_fd = -1;
			pending.clear();
			continue;
		}
		pending.append(buffer, (size_t)n);
		if (pending.size() > 8192) {
			std::lock_guard<std::mutex> lock(g_ctrl_mutex);
			if (g_ctrl_fd >= 0)
				close(g_ctrl_fd);
			g_ctrl_fd = -1;
			pending.clear();
			continue;
		}
		size_t newline;
		while ((newline = pending.find('\n')) != std::string::npos) {
			std::string line = pending.substr(0, newline);
			pending.erase(0, newline + 1);
			std::string action = json_string(line, "action");
			long seq = 0;
			if (!action.empty())
				log_line("ctrl command: %s", action.c_str());
			if (action == "start_video") {
				g_active = true;
				g_force_idr = true;
				if (!g_capture_running)
					log_line("start capture requested");
			} else if (action == "stop_video") {
				g_active = false;
				g_force_idr = false;
				log_line("stop capture requested");
			} else if (action == "request_keyframe") {
				g_force_idr = true;
				log_line("keyframe requested");
			} else if (action.empty()) {
				log_line("ignoring unparsable control message");
			}
			if (json_int(line, "seq", &seq) && seq > 0) {
				char reply[96];
				bool unsupported = action != "start_video" &&
						   action != "stop_video" &&
						   action != "request_keyframe";
				snprintf(reply, sizeof(reply),
					 "{\"seq\":%ld%s}\n", seq,
					 unsupported ? ",\"error\":\"unsupported action\"" : "");
				send_ctrl_line(reply);
			}
		}
	}
}

static void on_signal(int)
{
	g_running = false;
	g_active = false;
}

int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--frames") && i + 1 < argc) {
			char *end = nullptr;
			g_max_frames = strtol(argv[++i], &end, 10);
			if (!end || *end || g_max_frames < 1) {
				fprintf(stderr, "--frames 需要正整数\n");
				return 2;
			}
		} else {
			fprintf(stderr, "用法: %s [--frames N]\n", argv[0]);
			return 2;
		}
	}

	g_ctrl_path = env_or("KVM_VIDEO_CTRL_SOCKET", "/var/run/kvm_ctrl.sock");
	g_data_path = env_or("KVM_VIDEO_DATA_SOCKET", "/tmp/kvm_video_stream.sock");
	log_line("video node start ctrl=%s data=%s", g_ctrl_path.c_str(), g_data_path.c_str());

	ensure_data_connected();
	EncodeParam param = {};
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
	g_encoder = AWVideoEncoder::create();
	if (!g_encoder || g_encoder->init(&param, &g_sender) != 0) {
		log_line("encoder init failed");
		return 1;
	}
	/* Go 端要拿到 SPS/PPS 才会建立会话，所以启动时就要连数据通道。 */
	for (int i = 0; i < 60 && g_running; i++) {
		if (ensure_data_connected())
			break;
		struct timespec wait = {.tv_sec = 1, .tv_nsec = 0};
		nanosleep(&wait, NULL);
	}

	struct sigaction sa = {};
	sa.sa_handler = on_signal;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	std::thread control(control_loop);
	std::thread capture([] {
		while (g_running) {
			if (!g_active) {
				struct timespec ts = {.tv_sec = 0, .tv_nsec = 100 * 1000 * 1000};
				nanosleep(&ts, NULL);
				continue;
			}
			capture_session();
			if (g_running && g_active) {
				struct timespec ts = {.tv_sec = 2, .tv_nsec = 0};
				nanosleep(&ts, NULL);
			}
		}
	});

	while (g_running) {
		struct timespec ts = {.tv_sec = 0, .tv_nsec = 200 * 1000 * 1000};
		nanosleep(&ts, NULL);
	}
	g_active = false;
	if (control.joinable())
		control.join();
	if (capture.joinable())
		capture.join();

	if (g_encoder) {
		AWVideoEncoder::destroy(g_encoder);
		g_encoder = nullptr;
	}
	std::lock_guard<std::mutex> cl(g_ctrl_mutex);
	if (g_ctrl_fd >= 0)
		close(g_ctrl_fd);
	std::lock_guard<std::mutex> dl(g_data_mutex);
	if (g_data_fd >= 0)
		close(g_data_fd);
	log_line("video node exit");
	return 0;
}
