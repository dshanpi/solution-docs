#!/bin/sh
# 第 18 课：用 configfs 把 T527 配成一套 USB 键鼠（键盘 + 绝对鼠标 + 相对鼠标）。
# 用法：hid-gadget.sh setup|teardown|status
# 说明：内核 HIDG_MINORS=4，全局只允许 4 个 HID 功能；项目 Gadget 已占 3 个，
#       所以 setup 会先停 KVM 并释放它的三个功能，teardown 后可用 S99kvm start 恢复。
set -eu

G=${G:-/sys/kernel/config/usb_gadget/kvm_tutorial}
K=${K:-/sys/kernel/config/usb_gadget/kvm}
UDC=${UDC:-$(ls /sys/class/udc | head -n 1)}
VID=${VID:-0x1d6b}
PID=${PID:-0x0104}

# 三份报告描述符：键盘 8 字节、绝对鼠标 6 字节、相对鼠标 4 字节
KBD_B64='BQEJBqEBBQcZ4CnnFQAlAXUBlQiBApUBdQiBA5UFdQEFCBkBKQWRApUBdQORA5UGdQgVACVlBQcZACllgQDA'
ABS_B64='BQEJAqEBhQEJAaEABQkZASkDFQAlAXUBlQOBApUBdQWBAwUBCTAJMRYAACb/fzYAAEb/f3UQlQKBAsCFAgk4FYElfzUARQB1CJUBgQbA'
REL_B64='BQEJAqEBCQGhAAUJGQEpCBUAJQGVCHUBgQIFAQkwCTEJOBWBJX91CJUDgQbAwA=='

usage() {
	echo "用法：$0 setup|teardown|status" >&2
	exit 2
}

free_service_functions() {
	for i in 0 1 2; do
		rm -f "$K/configs/c.1/hid.usb$i"
		rmdir "$K/functions/hid.usb$i" 2>/dev/null || true
	done
}

find_node() {
	dev=$(cat "$G/functions/hid.usb$1/dev" 2>/dev/null || true)
	[ -n "$dev" ] || return 0
	for n in /sys/class/hidg/hidg*; do
		[ -e "$n" ] || continue
		if [ "$(cat "$n/dev")" = "$dev" ]; then
			echo "/dev/$(basename "$n")"
			return 0
		fi
	done
}

add_function() {
	i=$1
	protocol=$2
	subclass=$3
	length=$4
	b64=$5
	no_out=$6

	mkdir "$G/functions/hid.usb$i"
	printf '%s' "$protocol" > "$G/functions/hid.usb$i/protocol"
	printf '%s' "$subclass" > "$G/functions/hid.usb$i/subclass"
	printf '%s' "$length" > "$G/functions/hid.usb$i/report_length"
	printf '%s' "$b64" | base64 -d > "$G/functions/hid.usb$i/report_desc"
	[ "$no_out" = "1" ] && printf '1' > "$G/functions/hid.usb$i/no_out_endpoint"
	ln -s "$G/functions/hid.usb$i" "$G/configs/c.1/hid.usb$i"
}

case "${1:-}" in
setup)
	[ -e "$G" ] && { echo "$G 已存在，先 teardown" >&2; exit 1; }
	[ -n "$UDC" ] || { echo "找不到 UDC" >&2; exit 1; }

	/etc/init.d/S99kvm stop
	printf '\n' > "$K/UDC" 2>/dev/null || true
	free_service_functions

	mkdir -p "$G"
	printf '%s' "$VID" > "$G/idVendor"
	printf '%s' "$PID" > "$G/idProduct"
	printf '%s' "0x0200" > "$G/bcdUSB"
	printf '%s' "0x0100" > "$G/bcdDevice"

	mkdir -p "$G/strings/0x409"
	printf '%s' "100ASK" > "$G/strings/0x409/manufacturer"
	printf '%s' "KVM tutorial HID" > "$G/strings/0x409/product"
	printf '%s' "KVM-TUTORIAL-1" > "$G/strings/0x409/serialnumber"

	mkdir -p "$G/configs/c.1/strings/0x409"
	printf '%s' "Keyboard and mice" > "$G/configs/c.1/strings/0x409/configuration"

	add_function 0 1 1 8 "$KBD_B64" 0
	add_function 1 2 0 6 "$ABS_B64" 1
	add_function 2 2 1 4 "$REL_B64" 0

	printf '%s' "$UDC" > "$G/UDC"

	i=0
	state=unknown
	while [ "$i" -lt 5 ]; do
		state=$(cat "/sys/class/udc/$UDC/state" 2>/dev/null || echo unknown)
		[ "$state" = "configured" ] && break
		sleep 1
		i=$((i + 1))
	done

	echo "已绑定 $UDC"
	echo "键盘：$(find_node 0)（8 字节）"
	echo "绝对鼠标：$(find_node 1)（6 字节）"
	echo "相对鼠标：$(find_node 2)（4 字节）"
	echo "UDC 状态：$state"
	;;

teardown)
	[ -e "$G" ] || exit 0
	printf '\n' > "$G/UDC" 2>/dev/null || true
	for i in 0 1 2; do
		rm -f "$G/configs/c.1/hid.usb$i"
		rmdir "$G/functions/hid.usb$i" 2>/dev/null || true
	done
	rmdir "$G/configs/c.1/strings/0x409" 2>/dev/null || true
	rmdir "$G/configs/c.1" 2>/dev/null || true
	rmdir "$G/strings/0x409" 2>/dev/null || true
	rmdir "$G" 2>/dev/null || true
	echo "已清理 $G；执行 /etc/init.d/S99kvm start 恢复 KVM"
	;;

status)
	[ -e "$G" ] || { echo "$G 不存在"; exit 1; }
	echo "UDC 状态：$(cat "/sys/class/udc/$(cat "$G/UDC")/state" 2>/dev/null || echo 未绑定)"
	echo "键盘：$(find_node 0)（8 字节）"
	echo "绝对鼠标：$(find_node 1)（6 字节）"
	echo "相对鼠标：$(find_node 2)（4 字节）"
	;;

*)
	usage
	;;
esac