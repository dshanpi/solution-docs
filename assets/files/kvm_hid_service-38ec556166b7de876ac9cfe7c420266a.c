/* 第 24 课：C 实现的 USB HID 服务，接 Go 后端的 HID IPC。
 * configfs 建键盘 + 绝对鼠标 + 相对鼠标，按行 JSON 接收输入报告。
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define ROOT "/sys/kernel/config/usb_gadget/kvm"
#define FRAME_MAX 4096
#define LEASE_MS 3000

static const uint8_t kbd_desc[] = {
	0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7,
	0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01,
	0x75, 0x08, 0x81, 0x03, 0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01,
	0x29, 0x05, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x03, 0x95, 0x06,
	0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65,
	0x81, 0x00, 0xc0,
};
static const uint8_t abs_desc[] = {
	0x05, 0x01, 0x09, 0x02, 0xa1, 0x01, 0x85, 0x01, 0x09, 0x01, 0xa1, 0x00,
	0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01,
	0x95, 0x03, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x03, 0x05, 0x01,
	0x09, 0x30, 0x09, 0x31, 0x16, 0x00, 0x00, 0x26, 0xff, 0x7f, 0x36, 0x00,
	0x00, 0x46, 0xff, 0x7f, 0x75, 0x10, 0x95, 0x02, 0x81, 0x02, 0xc0, 0x85,
	0x02, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7f, 0x35, 0x00, 0x45, 0x00, 0x75,
	0x08, 0x95, 0x01, 0x81, 0x06, 0xc0,
};
static const uint8_t rel_desc[] = {
	0x05, 0x01, 0x09, 0x02, 0xa1, 0x01, 0x09, 0x01, 0xa1, 0x00, 0x05, 0x09,
	0x19, 0x01, 0x29, 0x08, 0x15, 0x00, 0x25, 0x01, 0x95, 0x08, 0x75, 0x01,
	0x81, 0x02, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81,
	0x25, 0x7f, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06, 0xc0, 0xc0,
};

typedef struct {
	bool keyboard, absolute, relative;
	char vendor[16], product_id[16], serial[128], manufacturer[128], product[128];
} Config;

static volatile sig_atomic_t g_running = 1;
static int g_listen = -1;
static int g_client = -1;
static int g_hid[3] = {-1, -1, -1};
static bool g_configured;
static bool g_gadget_ready;
static bool g_held[3];
static int g_x = 16384, g_y = 16384;
static uint8_t g_led;
static char g_udc[128];
static Config g_config;
static int64_t g_deadline;
static char g_socket[108] = "/run/kvm/hid.sock";

static int64_t now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
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

static int write_all(int fd, const void *data, size_t size)
{
	const uint8_t *p = data;
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

static int write_attr(const char *path, const void *data, size_t size)
{
	int fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;
	int rc = write_all(fd, data, size);
	int saved = errno;
	close(fd);
	errno = saved;
	return rc;
}

static int write_text(const char *path, const char *text)
{
	return write_attr(path, text, strlen(text));
}

static int read_text(const char *path, char *out, size_t size)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;
	ssize_t n = read(fd, out, size - 1);
	close(fd);
	if (n < 0)
		return -1;
	out[n] = 0;
	while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' '))
		out[--n] = 0;
	return 0;
}

static void ensure_dir(const char *path)
{
	if (mkdir(path, 0755) < 0 && errno != EEXIST)
		log_line("mkdir %s failed: %s", path, strerror(errno));
}

static void remove_gadget(void)
{
	char path[256];
	write_text(ROOT "/UDC", "\n");
	usleep(100 * 1000);
	for (int i = 0; i < 3; i++) {
		snprintf(path, sizeof(path), ROOT "/configs/c.1/hid.usb%d", i);
		unlink(path);
		snprintf(path, sizeof(path), ROOT "/functions/hid.usb%d", i);
		rmdir(path);
	}
	rmdir(ROOT "/configs/c.1/strings/0x409");
	rmdir(ROOT "/configs/c.1");
	rmdir(ROOT "/strings/0x409");
	rmdir(ROOT);
}

static int select_udc(void)
{
	const char *env = getenv("KVM_USB_UDC");
	if (env && *env) {
		snprintf(g_udc, sizeof(g_udc), "%s", env);
		return 0;
	}
	DIR *dir = opendir("/sys/class/udc");
	if (!dir)
		return -1;
	struct dirent *entry;
	int count = 0;
	while ((entry = readdir(dir))) {
		if (entry->d_name[0] == '.')
			continue;
		count++;
		snprintf(g_udc, sizeof(g_udc), "%.127s", entry->d_name);
	}
	closedir(dir);
	if (count != 1) {
		log_line("set KVM_USB_UDC: found %d UDCs", count);
		return -1;
	}
	return 0;
}

static bool is_bound(void)
{
	char value[128];
	return g_udc[0] && !read_text(ROOT "/UDC", value, sizeof(value)) &&
	       !strcmp(value, g_udc);
}

static int open_hid_nodes(void)
{
	for (int i = 0; i < 3; i++) {
		if (g_hid[i] >= 0)
			close(g_hid[i]);
		g_hid[i] = -1;
		if (i == 0 && !g_config.keyboard)
			continue;
		if (i == 1 && !g_config.absolute)
			continue;
		if (i == 2 && !g_config.relative)
			continue;
		char path[256], dev[64], target[256];
		snprintf(path, sizeof(path), ROOT "/functions/hid.usb%d/dev", i);
		if (read_text(path, dev, sizeof(dev)))
			return -1;
		snprintf(path, sizeof(path), "/sys/dev/char/%s", dev);
		ssize_t n = readlink(path, target, sizeof(target) - 1);
		if (n < 0)
			return -1;
		target[n] = 0;
		char *name = strrchr(target, '/');
		name = name ? name + 1 : target;
		if (strncmp(name, "hidg", 4))
			return -1;
		snprintf(path, sizeof(path), "/dev/%.32s", name);
		g_hid[i] = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
		if (g_hid[i] < 0)
			return -1;
		log_line("open %s", path);
	}
	return 0;
}

static void close_hid_nodes(void)
{
	for (int i = 0; i < 3; i++) {
		if (g_hid[i] >= 0)
			close(g_hid[i]);
		g_hid[i] = -1;
	}
}

static bool config_equal(const Config *a, const Config *b)
{
	return a->keyboard == b->keyboard && a->absolute == b->absolute &&
	       a->relative == b->relative && !strcmp(a->vendor, b->vendor) &&
	       !strcmp(a->product_id, b->product_id) && !strcmp(a->serial, b->serial) &&
	       !strcmp(a->manufacturer, b->manufacturer) && !strcmp(a->product, b->product);
}

static int create_function(int index, int protocol, int subclass, int length,
			   const uint8_t *desc, size_t desc_size, bool no_out)
{
	char path[256];
	snprintf(path, sizeof(path), ROOT "/functions/hid.usb%d", index);
	if (mkdir(path, 0755) < 0 && errno != EEXIST)
		return -1;
	snprintf(path, sizeof(path), ROOT "/functions/hid.usb%d/protocol", index);
	if (write_text(path, protocol == 1 ? "1" : "2"))
		return -1;
	snprintf(path, sizeof(path), ROOT "/functions/hid.usb%d/subclass", index);
	if (write_text(path, subclass ? "1" : "0"))
		return -1;
	snprintf(path, sizeof(path), ROOT "/functions/hid.usb%d/report_length", index);
	char len[8];
	snprintf(len, sizeof(len), "%d", length);
	if (write_text(path, len))
		return -1;
	snprintf(path, sizeof(path), ROOT "/functions/hid.usb%d/report_desc", index);
	if (write_attr(path, desc, desc_size))
		return -1;
	if (no_out) {
		snprintf(path, sizeof(path), ROOT "/functions/hid.usb%d/no_out_endpoint", index);
		write_text(path, "1");
	}
	snprintf(path, sizeof(path), ROOT "/functions/hid.usb%d", index);
	char link[256];
	snprintf(link, sizeof(link), ROOT "/configs/c.1/hid.usb%d", index);
	unlink(link);
	if (symlink(path, link) < 0)
		return -1;
	return 0;
}

static void release_reports(void);

static int gadget_configure(Config *cfg)
{
	if (g_gadget_ready && config_equal(cfg, &g_config))
		return open_hid_nodes();

	release_reports();
	close_hid_nodes();
	g_config = *cfg;
	g_configured = false;
	g_gadget_ready = false;
	remove_gadget();

	ensure_dir(ROOT);
	ensure_dir(ROOT "/strings/0x409");
	ensure_dir(ROOT "/configs/c.1");
	ensure_dir(ROOT "/configs/c.1/strings/0x409");

	char path[256];
	if (write_text(ROOT "/idVendor", cfg->vendor) ||
	    write_text(ROOT "/idProduct", cfg->product_id) ||
	    write_text(ROOT "/bcdUSB", "0x0200") ||
	    write_text(ROOT "/bcdDevice", "0x0100"))
		return -1;
	snprintf(path, sizeof(path), ROOT "/strings/0x409/manufacturer");
	if (write_text(path, cfg->manufacturer) ||
	    write_text(ROOT "/strings/0x409/product", cfg->product) ||
	    write_text(ROOT "/strings/0x409/serialnumber", cfg->serial) ||
	    write_text(ROOT "/configs/c.1/strings/0x409/configuration", "Keyboard and mice"))
		return -1;

	if (cfg->keyboard &&
	    create_function(0, 1, 1, 8, kbd_desc, sizeof(kbd_desc), false))
		return -1;
	if (cfg->absolute &&
	    create_function(1, 2, 0, 6, abs_desc, sizeof(abs_desc), true))
		return -1;
	if (cfg->relative &&
	    create_function(2, 2, 1, 4, rel_desc, sizeof(rel_desc), false))
		return -1;

	if (write_text(ROOT "/UDC", g_udc))
		return -1;
	for (int i = 0; i < 30; i++) {
		if (!open_hid_nodes()) {
			g_gadget_ready = true;
			log_line("gadget configured udc=%s", g_udc);
			return 0;
		}
		usleep(100 * 1000);
	}
	log_line("gadget nodes not ready");
	return -1;
}

static void release_reports(void)
{
	uint8_t kbd[8] = {0};
	uint8_t abs[6] = {1, 0, (uint8_t)g_x, (uint8_t)(g_x >> 8),
			  (uint8_t)g_y, (uint8_t)(g_y >> 8)};
	uint8_t rel[4] = {0};
	if (g_hid[0] >= 0)
		write_all(g_hid[0], kbd, sizeof(kbd));
	if (g_hid[1] >= 0)
		write_all(g_hid[1], abs, sizeof(abs));
	if (g_hid[2] >= 0)
		write_all(g_hid[2], rel, sizeof(rel));
	memset(g_held, 0, sizeof(g_held));
}

static int report(const uint8_t *data, size_t size)
{
	if (size != 8 && size != 6 && size != 4 && size != 2)
		return -1;
	int index = size == 8 ? 0 : (size == 6 || size == 2 ? 1 : 2);
	if (g_hid[index] < 0)
		return -1;
	struct pollfd p = {.fd = g_hid[index], .events = POLLOUT};
	int ready = poll(&p, 1, 100);
	if (ready <= 0 || !(p.revents & POLLOUT))
		return -1;
	return write_all(g_hid[index], data, size) ? -1 : 0;
}

static bool json_bool(const char *line, const char *key, bool *out)
{
	char pattern[64];
	snprintf(pattern, sizeof(pattern), "\"%s\"", key);
	const char *p = strstr(line, pattern);
	if (!p)
		return false;
	p = strchr(p + strlen(pattern), ':');
	if (!p)
		return false;
	p++;
	while (*p == ' ' || *p == '\t')
		p++;
	if (!strncmp(p, "true", 4)) {
		*out = true;
		return true;
	}
	if (!strncmp(p, "false", 5)) {
		*out = false;
		return true;
	}
	return false;
}

static bool json_str(const char *line, const char *key, char *out, size_t size)
{
	char pattern[64];
	snprintf(pattern, sizeof(pattern), "\"%s\"", key);
	const char *p = strstr(line, pattern);
	if (!p)
		return false;
	p = strchr(p + strlen(pattern), ':');
	if (!p)
		return false;
	p = strchr(p, '"');
	if (!p)
		return false;
	p++;
	const char *e = strchr(p, '"');
	if (!e || (size_t)(e - p) >= size)
		return false;
	memcpy(out, p, (size_t)(e - p));
	out[e - p] = 0;
	return true;
}

static bool json_int(const char *line, const char *key, long *out)
{
	char pattern[64];
	snprintf(pattern, sizeof(pattern), "\"%s\"", key);
	const char *p = strstr(line, pattern);
	if (!p)
		return false;
	p = strchr(p + strlen(pattern), ':');
	if (!p)
		return false;
	char *end = NULL;
	long value = strtol(p + 1, &end, 10);
	if (end == p + 1)
		return false;
	*out = value;
	return true;
}

static int json_keys(const char *line, int *keys, int *count)
{
	const char *p = strstr(line, "\"keys\"");
	if (!p)
		return -1;
	p = strchr(p, '[');
	if (!p)
		return -1;
	int n = 0;
	p++;
	while (*p && *p != ']') {
		while (*p == ' ' || *p == '\t' || *p == ',')
			p++;
		if (*p == ']')
			break;
		char *end = NULL;
		long value = strtol(p, &end, 10);
		if (end == p || n >= 6 || value < 0 || value > 255)
			return -1;
		keys[n++] = (int)value;
		p = end;
	}
	*count = n;
	return 0;
}

static int send_line(const char *line)
{
	if (g_client < 0)
		return -1;
	size_t size = strlen(line);
	const char *p = line;
	while (size) {
		ssize_t n = send(g_client, p, size, MSG_NOSIGNAL);
		if (n <= 0)
			return -1;
		p += n;
		size -= (size_t)n;
	}
	return 0;
}

static int reply(const char *error, bool with_state)
{
	char line[512];
	if (error) {
		snprintf(line, sizeof(line), "{\"v\":1,\"error\":\"%s\"}\n", error);
	} else {
		snprintf(line, sizeof(line),
			 "{\"v\":1%s,\"bound\":%s,\"led\":{"
			 "\"num_lock\":%s,\"caps_lock\":%s,\"scroll_lock\":%s,"
			 "\"compose\":false,\"kana\":false}}\n",
			 with_state ? ",\"state\":\"configured\"" : "",
			 is_bound() ? "true" : "false",
			 g_led & 1 ? "true" : "false",
			 g_led & 2 ? "true" : "false",
			 g_led & 4 ? "true" : "false");
	}
	return send_line(line);
}

static int parse_config(const char *line, Config *cfg)
{
	Config c;
	memset(&c, 0, sizeof(c));
	c.keyboard = true;
	c.absolute = true;
	c.relative = true;
	json_bool(line, "keyboard", &c.keyboard);
	json_bool(line, "absolute_mouse", &c.absolute);
	json_bool(line, "relative_mouse", &c.relative);
	json_str(line, "vendor_id", c.vendor, sizeof(c.vendor));
	json_str(line, "product_id", c.product_id, sizeof(c.product_id));
	json_str(line, "serial_number", c.serial, sizeof(c.serial));
	json_str(line, "manufacturer", c.manufacturer, sizeof(c.manufacturer));
	json_str(line, "product", c.product, sizeof(c.product));
	if (!c.vendor[0])
		snprintf(c.vendor, sizeof(c.vendor), "0x1d6b");
	if (!c.product_id[0])
		snprintf(c.product_id, sizeof(c.product_id), "0x0104");
	if (!c.serial[0])
		snprintf(c.serial, sizeof(c.serial), "KVM-TUTORIAL-1");
	if (!c.manufacturer[0])
		snprintf(c.manufacturer, sizeof(c.manufacturer), "100ASK");
	if (!c.product[0])
		snprintf(c.product, sizeof(c.product), "KVM tutorial HID");
	if (!c.keyboard && !c.absolute && !c.relative)
		return -1;
	*cfg = c;
	return 0;
}

static int handle_line(const char *line)
{
	char op[32];
	long value;
	if (!json_str(line, "op", op, sizeof(op)))
		return reply("missing op", false);
	if (!strcmp(op, "configure") || !strcmp(op, "reconfigure")) {
		Config cfg;
		if (parse_config(line, &cfg) < 0)
			return reply("configure requires keyboard or mouse", false);
		if (gadget_configure(&cfg) < 0)
			return reply("gadget setup failed", false);
		g_configured = true;
		log_line("configured keyboard=%d absolute=%d relative=%d",
			 cfg.keyboard, cfg.absolute, cfg.relative);
		return reply(NULL, true);
	}
	if (!strcmp(op, "status"))
		return reply(g_configured ? NULL : "configure handshake required", true);
	if (!g_configured)
		return reply("configure handshake required", false);

	if (!strcmp(op, "keyboard")) {
		int keys[6], count = 0;
		long modifier = 0;
		json_int(line, "modifier", &modifier);
		if (json_keys(line, keys, &count) < 0 || modifier < 0 || modifier > 255)
			return reply("invalid keyboard report", false);
		uint8_t b[8] = {(uint8_t)modifier, 0};
		bool held = modifier != 0;
		for (int i = 0; i < count; i++) {
			b[i + 2] = (uint8_t)keys[i];
			held = held || keys[i] != 0;
		}
		if (report(b, sizeof(b)) < 0)
			return reply("keyboard write failed", false);
		g_held[0] = held;
		return reply(NULL, false);
	}
	if (!strcmp(op, "absolute")) {
		long x, y, buttons = 0;
		if (!json_int(line, "x", &x) || !json_int(line, "y", &y) ||
		    x < 0 || x > 32767 || y < 0 || y > 32767)
			return reply("invalid absolute report", false);
		json_int(line, "buttons", &buttons);
		if (buttons < 0 || buttons > 7)
			return reply("invalid absolute buttons", false);
		uint8_t b[6] = {1, (uint8_t)buttons, (uint8_t)x, (uint8_t)(x >> 8),
				(uint8_t)y, (uint8_t)(y >> 8)};
		if (report(b, sizeof(b)) < 0)
			return reply("absolute write failed", false);
		g_x = (int)x;
		g_y = (int)y;
		g_held[1] = buttons != 0;
		return reply(NULL, false);
	}
	if (!strcmp(op, "relative")) {
		long dx, dy, buttons = 0;
		if (!json_int(line, "dx", &dx) || !json_int(line, "dy", &dy) ||
		    dx < -128 || dx > 127 || dy < -128 || dy > 127)
			return reply("invalid relative report", false);
		json_int(line, "buttons", &buttons);
		if (buttons < 0 || buttons > 255)
			return reply("invalid relative buttons", false);
		uint8_t b[4] = {(uint8_t)buttons, (uint8_t)dx, (uint8_t)dy, 0};
		if (report(b, sizeof(b)) < 0)
			return reply("relative write failed", false);
		g_held[2] = buttons != 0;
		return reply(NULL, false);
	}
	if (!strcmp(op, "wheel")) {
		if (!json_int(line, "wheel", &value) || value < -128 || value > 127)
			return reply("invalid wheel report", false);
		if (value) {
			uint8_t b[2] = {2, (uint8_t)value};
			if (report(b, sizeof(b)) < 0)
				return reply("wheel write failed", false);
		}
		return reply(NULL, false);
	}
	if (!strcmp(op, "release")) {
		release_reports();
		return reply(NULL, false);
	}
	return reply("unsupported op", false);
}

static void disconnect_client(void)
{
	if (g_client >= 0) {
		release_reports();
		close(g_client);
		g_client = -1;
	}
	g_configured = false;
}

static void on_signal(int sig)
{
	(void)sig;
	g_running = 0;
}

int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--socket") && i + 1 < argc) {
			snprintf(g_socket, sizeof(g_socket), "%s", argv[++i]);
		} else {
			fprintf(stderr, "usage: %s [--socket PATH]\n", argv[0]);
			return 2;
		}
	}

	struct sigaction sa = {.sa_handler = on_signal};
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	signal(SIGPIPE, SIG_IGN);

	if (select_udc() < 0)
		return 1;
	mkdir("/run/kvm", 0750);
	int lock = open("/run/kvm/usb-gadget.lock", O_CREAT | O_RDWR | O_CLOEXEC, 0600);
	if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB)) {
		log_line("HID service lock failed: %s", strerror(errno));
		return 1;
	}

	struct sockaddr_un addr = {.sun_family = AF_UNIX};
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", g_socket);
	struct stat st;
	if (!lstat(g_socket, &st)) {
		if (!S_ISSOCK(st.st_mode)) {
			log_line("refusing to replace non-socket %s", g_socket);
			return 1;
		}
		unlink(g_socket);
	}
	g_listen = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (g_listen < 0 || bind(g_listen, (struct sockaddr *)&addr, sizeof(addr)) ||
	    chmod(g_socket, 0600) || listen(g_listen, 4)) {
		log_line("listen %s failed: %s", g_socket, strerror(errno));
		return 1;
	}
	log_line("HID service listening socket=%s udc=%s", g_socket, g_udc);

	char pending[FRAME_MAX + 1];
	size_t used = 0;
	while (g_running) {
		struct pollfd p[3] = {
			{.fd = g_listen, .events = POLLIN},
			{.fd = g_client, .events = POLLIN},
			{.fd = g_hid[0], .events = POLLIN},
		};
		int64_t remain = g_client < 0 ? 500 : g_deadline - now_ms();
		if (remain < 0)
			remain = 0;
		int ready = poll(p, 3, (int)remain);
		if (ready < 0 && errno == EINTR)
			continue;
		if (ready < 0)
			break;
		if (g_client >= 0 && now_ms() >= g_deadline) {
			log_line("input lease expired");
			disconnect_client();
			used = 0;
		}
		if (p[0].revents & POLLIN) {
			int fd = accept(g_listen, NULL, NULL);
			if (fd >= 0) {
				if (g_client >= 0) {
					const char *busy = "{\"v\":1,\"error\":\"USB input owner already connected\"}\n";
					send(fd, busy, strlen(busy), MSG_NOSIGNAL);
					close(fd);
				} else {
					g_client = fd;
					g_configured = false;
					used = 0;
					g_deadline = now_ms() + LEASE_MS;
					log_line("input owner connected");
				}
			}
		}
		if (p[2].revents & POLLIN && g_hid[0] >= 0) {
			uint8_t led[8];
			ssize_t n = read(g_hid[0], led, sizeof(led));
			if (n > 0 && !(led[0] & ~31))
				g_led = led[0];
		}
		if (g_client < 0)
			continue;
		if (p[1].revents & POLLIN) {
			ssize_t n = recv(g_client, pending + used, FRAME_MAX - used, 0);
			if (n <= 0) {
				log_line("input owner disconnected");
				disconnect_client();
				used = 0;
				continue;
			}
			used += (size_t)n;
			char *newline;
			while ((newline = memchr(pending, '\n', used))) {
				size_t len = (size_t)(newline - pending);
				pending[len] = 0;
				if (handle_line(pending) < 0) {
					disconnect_client();
					used = 0;
					break;
				}
				g_deadline = now_ms() + LEASE_MS;
				memmove(pending, newline + 1, used - len - 1);
				used -= len + 1;
			}
			if (used == FRAME_MAX) {
				log_line("oversized request");
				disconnect_client();
				used = 0;
			}
		}
		if (p[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			disconnect_client();
			used = 0;
		}
	}

	disconnect_client();
	for (int i = 0; i < 3; i++)
		if (g_hid[i] >= 0)
			close(g_hid[i]);
	if (g_listen >= 0)
		close(g_listen);
	unlink(g_socket);
	close(lock);
	log_line("HID service exit");
	return 0;
}
