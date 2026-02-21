import socket
import struct

MCAST_GRP = '239.0.0.1'
MCAST_PORT = 5000

# Setup UDP Socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(('', MCAST_PORT))

# Join Multicast Group
mreq = struct.pack("4sl", socket.inet_aton(MCAST_GRP), socket.INADDR_ANY)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

print("📈 Listening for Live Trades...")
print("-" * 50)

HDR_FORMAT = '<BH'
HDR_SIZE = struct.calcsize(HDR_FORMAT)

while True:
    data, addr = sock.recvfrom(1024)

    msg_type, msg_size = struct.unpack(HDR_FORMAT, data[:HDR_SIZE])
    payload = data[HDR_SIZE : HDR_SIZE + msg_size]

    if msg_type == 1:
        maker, taker, price, qty, ts = struct.unpack('<QQQIQ', payload)
        print(f"🟢 TRADE | Price: {price:4} | Qty: {qty:3} | Maker: {maker:4} vs Taker: {taker:4}")
    if msg_type == 2:
        # DO SOMETHING FOR BOOK SNAPSHOT
        print("BOOK SNAPSHOT RECIEVED")
        pass