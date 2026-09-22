"""KVM2 教学接收器：验证生产格式和真实流式分段，不触碰设备。"""
import contextlib
import importlib.util
import io
from pathlib import Path
import socket
import struct
import threading
import unittest

path = Path(__file__).resolve().parents[1] / 'static/examples/kvm/receive_video.py'
spec = importlib.util.spec_from_file_location('receive_video', path)
receiver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(receiver)

def packet(data=b'\x00\x00\x00\x01\x65\x88', timestamp=1234567, flags=0):
    return struct.pack('<4sIQI', b'KVM2', len(data), timestamp, flags) + data

class Fragmented:
    # 一次只返回三个字节，覆盖拆开的包头和 payload。
    def __init__(self, data): self.data = io.BytesIO(data)
    def recv(self, size): return self.data.read(min(3, size))

class ReceiverTest(unittest.TestCase):
    def run_receive(self, data, count=1):
        output = io.BytesIO()
        with contextlib.redirect_stdout(io.StringIO()):
            receiver.receive(Fragmented(data), output, count)
        return output.getvalue()

    def test_split_and_coalesced_config_frames(self):
        config=b'\x00\x00\x01\x67\x64\x00\x2a'
        frame=b'\x00\x00\x01\x65\x88'
        self.assertEqual(self.run_receive(packet(config,0,1)+packet(frame),2),config+frame)

    def test_real_socket_pair(self):
        a,b=socket.socketpair();a.settimeout(2);b.settimeout(2)
        data=packet(timestamp=2**40)
        def send():
            with a:
                for pos in range(0,len(data),5): a.sendall(data[pos:pos+5])
        t=threading.Thread(target=send);t.start()
        try:
            with b, contextlib.redirect_stdout(io.StringIO()):
                output=io.BytesIO();receiver.receive(b,output,1)
            self.assertEqual(output.getvalue(),data[20:])
        finally:t.join(3)

    def test_invalid_lengths(self):
        for size in [0,receiver.MAX_PACKET+1,0xffffffff]:
            with self.subTest(size=size),self.assertRaises(ValueError):
                self.run_receive(struct.pack('<4sIQI',b'KVM2',size,1,0))

    def test_wrong_magic(self):
        with self.assertRaises(ValueError):self.run_receive(b'BAD!'+packet()[4:])

    def test_timestamp_and_reserved_flags(self):
        for ts,flags in [(0,0),(1,1),(1,2)]:
            with self.subTest(ts=ts,flags=flags),self.assertRaises(ValueError):
                self.run_receive(packet(timestamp=ts,flags=flags))

    def test_truncated_header_and_payload(self):
        for data in [b'',packet()[:19],packet()[:-1]]:
            with self.subTest(length=len(data)),self.assertRaises(EOFError):
                self.run_receive(data)

if __name__=='__main__':unittest.main()
