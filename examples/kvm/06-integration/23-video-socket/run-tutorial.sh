#!/bin/sh
# 用 Go 后端加载本课的 C 视频端；Ctrl-C 恢复生产服务。
set -e
cd "$(dirname "$0")"
BIN=${KVM_VIDEO_BINARY:-$PWD/kvm_video_node}
PROFILE=/tmp/kvm-tutorial.env
SUP=

restore() {
    if [ -n "$SUP" ]; then
        kill "$SUP" 2>/dev/null || true
        wait "$SUP" 2>/dev/null || true
    fi
    sleep 2
    /etc/init.d/S99kvm start >/dev/null 2>&1 || true
}
trap restore EXIT INT TERM

[ -x "$BIN" ] || { echo "missing executable: $BIN" >&2; exit 1; }
cp /userdata/100ask_kvm/platform.env "$PROFILE"
printf 'export KVM_VIDEO_BINARY=%s\n' "$BIN" >> "$PROFILE"

/etc/init.d/S99kvm stop
sleep 2
rm -f /run/kvm/supervisor.pid

KVM_PLATFORM_PROFILE="$PROFILE" /userdata/100ask_kvm/run-kvm.sh \
    >/tmp/kvm-tutorial-supervisor.log 2>&1 &
SUP=$!

echo "C video node started. Open the KVM web page, then press Ctrl-C to restore."
wait "$SUP"
