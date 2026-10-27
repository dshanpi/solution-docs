/* 第 20 课：往鼠标节点写报告。
 * 默认只打印示例报告；加 --abs / --rel / --wheel / --click 才真正写入。
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ABS_LEN 6   /* 绝对鼠标：Report ID 1，按键 + X + Y */
#define WHEEL_LEN 2 /* 绝对鼠标：Report ID 2，滚轮 */
#define REL_LEN 4   /* 相对鼠标：按键 + X + Y + 滚轮，无 Report ID */

static int send_report(int fd, const uint8_t *data, int len)
{
	struct pollfd p = {.fd = fd, .events = POLLOUT};
	int ready;

	do {
		ready = poll(&p, 1, 2000);
	} while (ready < 0 && errno == EINTR);
	if (ready <= 0 || !(p.revents & POLLOUT)) {
		fprintf(stderr, "HID 节点不可写，检查 USB 是否枚举成功\n");
		return -1;
	}

	ssize_t written;
	do {
		written = write(fd, data, len);
	} while (written < 0 && errno == EINTR);
	if (written != len) {
		if (written < 0)
			perror("HID write");
		else
			fprintf(stderr, "报告只写了 %zd/%d 字节\n", written, len);
		return -1;
	}
	return 0;
}

static void print_report(const char *name, const uint8_t *data, int len)
{
	printf("%s: ", name);
	for (int i = 0; i < len; i++)
		printf("%02x%s", data[i], i + 1 == len ? "\n" : " ");
}

static int open_node(const char *path)
{
	int fd = open(path, O_WRONLY | O_NONBLOCK);
	if (fd < 0)
		perror("open HID");
	return fd;
}

/* 绝对鼠标：坐标 0–32767，按键位图在第 1 字节。 */
static int send_abs(int fd, int x, int y, int buttons)
{
	uint8_t report[ABS_LEN] = {0x01, 0, 0, 0, 0, 0};

	report[1] = (uint8_t)buttons;
	report[2] = (uint8_t)(x & 0xff);
	report[3] = (uint8_t)((x >> 8) & 0xff);
	report[4] = (uint8_t)(y & 0xff);
	report[5] = (uint8_t)((y >> 8) & 0xff);

	print_report("abs   ", report, ABS_LEN);
	return send_report(fd, report, ABS_LEN);
}

/* 绝对鼠标的滚轮走 Report ID 2。 */
static int send_wheel_abs(int fd, int delta)
{
	uint8_t report[WHEEL_LEN] = {0x02, (uint8_t)(int8_t)delta};

	print_report("wheel ", report, WHEEL_LEN);
	return send_report(fd, report, WHEEL_LEN);
}

/* 相对鼠标：按键、X、Y、滚轮各一字节，位移是有符号数。 */
static int send_rel(int fd, int dx, int dy, int wheel)
{
	uint8_t report[REL_LEN] = {0, (uint8_t)(int8_t)dx, (uint8_t)(int8_t)dy,
				   (uint8_t)(int8_t)wheel};

	print_report("rel   ", report, REL_LEN);
	return send_report(fd, report, REL_LEN);
}

static void pause_ms(int ms)
{
	struct timespec pause = {.tv_sec = ms / 1000,
				 .tv_nsec = (long)(ms % 1000) * 1000000};
	while (nanosleep(&pause, &pause) < 0 && errno == EINTR)
		;
}

int main(int argc, char **argv)
{
	int fd, ret;

	if (argc == 1) {
		uint8_t abs[ABS_LEN] = {0x01, 0x00, 0x00, 0x40, 0x00, 0x40};
		uint8_t wheel[WHEEL_LEN] = {0x02, 0x01};
		uint8_t rel[REL_LEN] = {0x00, 0x64, 0x32, 0x00};

		puts("绝对鼠标（6 字节，hidg1）：");
		print_report("  abs  ", abs, ABS_LEN);
		print_report("  wheel", wheel, WHEEL_LEN);
		puts("相对鼠标（4 字节，hidg2）：");
		print_report("  rel  ", rel, REL_LEN);
		puts("用法：");
		puts("  hid_mouse --abs /dev/hidg1 X Y [按键]   绝对坐标 0–32767");
		puts("  hid_mouse --rel /dev/hidg2 DX DY [滚轮] 相对位移 -127–127");
		puts("  hid_mouse --wheel /dev/hidg1 D          滚轮（绝对鼠标 Report ID 2）");
		puts("  hid_mouse --click /dev/hidg1 X Y        左键按下再释放");
		return 0;
	}

	if (argc < 3) {
		fprintf(stderr, "用法见 hid_mouse（不带参数）\n");
		return 2;
	}

	if (strcmp(argv[1], "--abs") == 0 && argc >= 5) {
		int x = atoi(argv[3]), y = atoi(argv[4]);
		int buttons = argc > 5 ? atoi(argv[5]) : 0;
		if (x < 0 || x > 32767 || y < 0 || y > 32767) {
			fprintf(stderr, "坐标范围是 0–32767\n");
			return 2;
		}
		fd = open_node(argv[2]);
		if (fd < 0)
			return 1;
		ret = send_abs(fd, x, y, buttons);
		close(fd);
		return ret ? 1 : 0;
	}

	if (strcmp(argv[1], "--rel") == 0 && argc >= 5) {
		int dx = atoi(argv[3]), dy = atoi(argv[4]);
		int wheel = argc > 5 ? atoi(argv[5]) : 0;
		if (dx < -127 || dx > 127 || dy < -127 || dy > 127) {
			fprintf(stderr, "相对位移范围是 -127–127\n");
			return 2;
		}
		fd = open_node(argv[2]);
		if (fd < 0)
			return 1;
		ret = send_rel(fd, dx, dy, wheel);
		close(fd);
		return ret ? 1 : 0;
	}

	if (strcmp(argv[1], "--wheel") == 0 && argc >= 4) {
		int delta = atoi(argv[3]);
		if (delta < -127 || delta > 127) {
			fprintf(stderr, "滚轮范围是 -127–127\n");
			return 2;
		}
		fd = open_node(argv[2]);
		if (fd < 0)
			return 1;
		ret = send_wheel_abs(fd, delta);
		close(fd);
		return ret ? 1 : 0;
	}

	if (strcmp(argv[1], "--click") == 0 && argc >= 5) {
		int x = atoi(argv[3]), y = atoi(argv[4]);
		fd = open_node(argv[2]);
		if (fd < 0)
			return 1;
		/* 按下左键（bit0），保持 100 ms 再释放，避免主机看不到点击。 */
		print_report("click+", (uint8_t[]){0x01, 0x01, 0, 0, 0, 0}, ABS_LEN);
		ret = send_abs(fd, x, y, 0x01);
		pause_ms(100);
		print_report("click-", (uint8_t[]){0x01, 0x00, 0, 0, 0, 0}, ABS_LEN);
		if (send_abs(fd, x, y, 0x00) < 0)
			ret = -1;
		close(fd);
		return ret ? 1 : 0;
	}

	fprintf(stderr, "用法见 hid_mouse（不带参数）\n");
	return 2;
}