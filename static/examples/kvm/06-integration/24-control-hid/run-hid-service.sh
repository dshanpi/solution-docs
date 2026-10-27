#!/bin/sh
# 用教程根目录替换 HID 服务，其余平台、Go 后端和视频保持生产版本；Ctrl-C 恢复。
set -e
cd "$(dirname "$0")"
BIN=${KVM_HID_BINARY:-$PWD/kvm_hid_service}
VIDEO_BIN=${KVM_VIDEO_BINARY:-/userdata/100ask_kvm/bin/kvm_video}
ROOT=/tmp/kvm24root
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

/etc/init.d/S99kvm stop
sleep 2
rm -f /run/kvm/supervisor.pid
rm -rf "$ROOT"
mkdir -p "$ROOT/bin"
for b in kvm-run kvm_platform kvm_app; do
    ln -s "/userdata/100ask_kvm/bin/$b" "$ROOT/bin/$b"
done
ln -s "$BIN" "$ROOT/bin/kvm_hid"
cp /userdata/100ask_kvm/run-kvm.sh "$ROOT/run-kvm.sh"
cp /userdata/100ask_kvm/platform.env "$ROOT/platform.env"
cp /userdata/100ask_kvm/board.json "$ROOT/board.json"
printf 'export KVM_VIDEO_BINARY=%s\n' "$VIDEO_BIN" >> "$ROOT/platform.env"
chmod +x "$ROOT/run-kvm.sh"

KVM_INSTALL_DIR="$ROOT" "$ROOT/run-kvm.sh" \
    >/tmp/kvm-tutorial-supervisor.log 2>&1 &
SUP=$!

echo "C HID service started. Use the KVM web page, then press Ctrl-C to restore."
wait "$SUP"
