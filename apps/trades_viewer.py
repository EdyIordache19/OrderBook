import socket
import struct

def parse_snapshot(payload):
    num_bids, num_asks = struct.unpack_from('<BB', payload)

    offset = struct.calcsize('<BB')
    bids = []
    for i in range(10):
        bids.append(struct.unpack_from('<QI', payload, offset))
        offset += struct.calcsize('<QI')

    asks = []
    for i in range(10):
        asks.append(struct.unpack_from('<QI', payload, offset))
        offset += struct.calcsize('<QI')

    return num_bids, num_asks, bids, asks

def print_snapshot(num_bids, num_asks, bids, asks):
    print(f"\n{'--- ORDERBOOK SNAPSHOT ---':^30}")
    print(f"{'Bids Count: ' + str(num_bids):<15} {'Asks Count: ' + str(num_asks):>14}")
    print("-" * 30)
    print(f"{'Side':<8} | {'Price':>10} | {'Qty':>7}")
    print("-" * 30)

    # Print Asks (Sellers) - usually displayed top-down (highest price to lowest)
    # We only print up to 'num_asks' to ignore empty padding in the array
    for i in range(num_asks - 1, -1, -1):
        p, q = asks[i]
        print(f"\033[91m{'ASK':<8}\033[0m | {p:>10} | {q:>7}")

    print("-" * 30)

    # Print Bids (Buyers) - usually highest price first
    for i in range(num_bids):
        p, q = bids[i]
        print(f"\033[92m{'BID':<8}\033[0m | {p:>10} | {q:>7}")

    print("-" * 30 + "\n")

def main():
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
            num_bids, num_asks, bids, asks = parse_snapshot(payload)
            print_snapshot(num_bids, num_asks, bids, asks)
if __name__ == "__main__":
    main()