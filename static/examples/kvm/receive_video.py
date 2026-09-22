"""教学用 KVM2 接收器，使用独立测试路径，不替换运行中的服务。"""
import argparse
import os
from pathlib import Path
import socket
import struct

MAX_PACKET = 1920 * 1080 * 2
HEADER = struct.Struct('<4sIQI')

def recv_exact(conn, size):
    data = bytearray()
    while len(data) < size:
        part = conn.recv(size - len(data))
        if not part:
            raise EOFError(f"connection closed at {len(data)}/{size} bytes")
        data.extend(part)
    return bytes(data)

def receive(conn, output, count):
    for i in range(count):
        # stream 不保留消息边界，必须先读满包头，再按长度读取 payload。
        magic, length, timestamp, flags = HEADER.unpack(recv_exact(conn, HEADER.size))
        if magic != b'KVM2':
            raise ValueError('expected KVM2 header')
        if not 0 < length <= MAX_PACKET:
            raise ValueError(f'invalid length {length}')
        if flags & ~1:
            raise ValueError(f'unknown flags {flags}')
        config = bool(flags & 1)
        if (config and timestamp != 0) or (not config and timestamp == 0):
            raise ValueError('timestamp does not match packet type')
        data = recv_exact(conn, length)
        output.write(data)
        print(f'packet={i} type={"config" if config else "frame"} '
              f'capture_us={timestamp} bytes={length}', flush=True)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--socket', default='/tmp/kvm-tutorial-video.sock')
    parser.add_argument('--output', default='capture.h264')
    parser.add_argument('--packets', type=int, default=120)
    args = parser.parse_args()
    if args.packets < 1:
        parser.error('--packets must be positive')
    path = Path(args.socket)
    if path.exists() or path.is_symlink():
        parser.error('socket path already exists; choose an unused test path')
    bound = False
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as server:
            server.bind(str(path)); bound = True
            identity = path.stat()
            os.chmod(path,0o600)
            server.listen(1); server.settimeout(30)
            print(f'listening: {path}',flush=True)
            with server.accept()[0] as conn, open(args.output,'xb') as output:
                conn.settimeout(10)
                receive(conn,output,args.packets)
    finally:
        # 只删除本进程建立、身份未变化的临时 Socket。
        if bound and path.exists():
            now=path.stat()
            if (now.st_dev,now.st_ino)==(identity.st_dev,identity.st_ino):
                path.unlink()

if __name__=='__main__':
    main()
