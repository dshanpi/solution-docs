/* 第 19 课：往键盘节点写 8 字节 HID 报告。
 * 默认只打印报告内容；加 --send 才真正写入 /dev/hidgN。
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

#define REPORT_LEN 8

/* 报告长度必须是描述符声明的 8 字节：短写不会被拆成下一份报告。 */
static int send_report(int fd, const uint8_t data[REPORT_LEN])
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
		written = write(fd, data, REPORT_LEN);
	} while (written < 0 && errno == EINTR);
	if (written != REPORT_LEN) {
		if (written < 0)
			perror("HID write");
		else
			fprintf(stderr, "报告只写了 %zd/%d 字节\n", written, REPORT_LEN);
		return -1;
	}
	return 0;
}

static void print_report(const char *name, const uint8_t data[REPORT_LEN])
{
	printf("%s: ", name);
	for (int i = 0; i < REPORT_LEN; i++)
		printf("%02x%s", data[i], i + 1 == REPORT_LEN ? "\n" : " ");
}

int main(int argc, char **argv)
{
	/* 第 0 字节是修饰键，第 1 字节保留，第 2–7 字节是同时按下的键码。 */
	uint8_t press[REPORT_LEN] = {0};
	uint8_t release[REPORT_LEN] = {0};
	uint8_t keycode = 0x04; /* 0x04 是键盘上的 a */
	int fd, ret;

	if (argc == 1) {
		press[2] = keycode;
		print_report("press  ", press);
		print_report("release", release);
		puts("真正发送：hid_keyboard --send /dev/hidg0 [键码]");
		return 0;
	}
	if (argc < 3 || strcmp(argv[1], "--send") != 0) {
		fprintf(stderr, "用法：%s [--send 设备 [键码]]\n", argv[0]);
		return 2;
	}
	if (argc == 4) {
		char *end = NULL;
		long value = strtol(argv[3], &end, 0);
		if (errno || !end || *end || value < 4 || value > 0x65) {
			fprintf(stderr, "键码超出常用范围\n");
			return 2;
		}
		keycode = (uint8_t)value;
	}

	fd = open(argv[2], O_WRONLY | O_NONBLOCK);
	if (fd < 0) {
		perror("open HID");
		return 1;
	}

	press[2] = keycode;
	print_report("press  ", press);
	ret = send_report(fd, press);

	/* 间隔一下，让主机能区分按下和释放两次报告。 */
	struct timespec pause = {.tv_sec = 0, .tv_nsec = 100 * 1000 * 1000};
	while (nanosleep(&pause, &pause) < 0 && errno == EINTR)
		;

	/* 即使按下失败也要释放，避免在主机侧留下按键状态。 */
	print_report("release", release);
	if (send_report(fd, release) < 0)
		ret = -1;

	printf("已发送键码 0x%02x 并释放\n", keycode);
	close(fd);
	return ret ? 1 : 0;
}