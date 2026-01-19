import socket
import struct

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
server_addr = ('localhost', 1234)

fmt = "<QQIc"

i = 0
while True:
    data = struct.pack(fmt, i, 100, 10, b'B')
    sock.sendto(data, server_addr)
    i = i + 1