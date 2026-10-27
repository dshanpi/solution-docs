#!/bin/sh
# 第 21 课：USB HID 异常检查与恢复。
# 用法：usb-recover.sh status|release|rebind
set -eu

K=${K:-/sys/kernel/config/usb_gadget/kvm}
UDC=${UDC:-$(ls /sys/class/udc | head -n 1)}

usage() {
	echo "用法：$0 status|release|rebind" >&2
	exit 2
}

show_status() {
	echo "== 控制器 =="
	echo "UDC: $UDC"
	cat "/sys/class/udc/$UDC/state" 2>/dev/null || echo "读不到状态"

	echo "== Gadget 绑定 =="
	for f in /sys/kernel/config/usb_gadget/*/UDC; do
		[ -f "$f" ] || continue
		printf '%s: ' "$f"
		cat "$f"
	done

	echo "== 进程 =="
	for name in adbd kvm_hid kvm_video kvm_app kvm_platform; do
		pid=$(pidof "$name" 2>/dev/null || true)
		printf '%s: %s\n' "$name" "${pid:-未运行}"
	done

	echo "== HID 节点 =="
	for g in /sys/kernel/config/usb_gadget/*; do
		[ -d "$g" ] || continue
		for f in "$g"/functions/hid.usb*; do
			[ -d "$f" ] || continue
			dev=$(cat "$f/dev" 2>/dev/null || true)
			[ -n "$dev" ] || continue
			for n in /sys/class/hidg/hidg*; do
				[ -e "$n" ] || continue
				if [ "$(cat "$n/dev")" = "$dev" ]; then
					echo "$(basename "$g"):$(basename "$f") -> /dev/$(basename "$n")，report_length=$(cat "$f/report_length")"
				fi
			done
		done
	done
}

release_inputs() {
	echo "向所有 HID 节点写全零报告："
	for g in /sys/kernel/config/usb_gadget/*; do
		[ -d "$g" ] || continue
		for f in "$g"/functions/hid.usb*; do
			[ -d "$f" ] || continue
			dev=$(cat "$f/dev" 2>/dev/null || true)
			[ -n "$dev" ] || continue
			len=$(cat "$f/report_length")
			for n in /sys/class/hidg/hidg*; do
				[ -e "$n" ] || continue
				if [ "$(cat "$n/dev")" = "$dev" ]; then
					node=/dev/$(basename "$n")
					printf '%s: ' "$node"
					dd if=/dev/zero of="$node" bs="$len" count=1 2>/dev/null \
						&& echo "已释放" || echo "写入失败"
				fi
			done
		done
	done
}

rebind_kvm() {
	if [ -e /sys/kernel/config/usb_gadget/kvm_tutorial ]; then
		echo "教学 Gadget 还在，先执行第 18 课的 teardown" >&2
		exit 1
	fi

	echo "停止 KVM 并解绑所有 Gadget："
	/etc/init.d/S99kvm stop
	for f in /sys/kernel/config/usb_gadget/*/UDC; do
		[ -f "$f" ] || continue
		printf '\n' > "$f" 2>/dev/null || true
	done

	echo "重新启动 KVM："
	/etc/init.d/S99kvm start

	i=0
	while [ "$i" -lt 8 ]; do
		state=$(cat "/sys/class/udc/$UDC/state" 2>/dev/null || echo unknown)
		[ "$state" = "configured" ] && break
		sleep 1
		i=$((i + 1))
	done
	echo "UDC 状态：$state"
}

case "${1:-}" in
status)
	show_status
	;;
release)
	release_inputs
	;;
rebind)
	rebind_kvm
	show_status
	;;
*)
	usage
	;;
esac