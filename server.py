import socket
import struct

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
server_addr = ('localhost', 1234)

fmt = "<QQIcHH"

i = 0
while True and i < 100:
    data = struct.pack(fmt, i, 100, 10, b'B', 1, 1)
    sock.sendto(data, server_addr)
    i = i + 1