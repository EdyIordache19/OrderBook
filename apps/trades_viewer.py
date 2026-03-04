import socket
import struct
import websockets
import asyncio
import json
import time
import random

MCAST_GRP = '239.0.0.1'
MCAST_PORT = 5000

# Setup UDP Socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(('', MCAST_PORT))

sock.setblocking(False)

# Join Multicast Group
mreq = struct.pack("4sl", socket.inet_aton(MCAST_GRP), socket.INADDR_ANY)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

clients = set()

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

async def udp_listener():
    print("📈 Listening for Live Trades...")
    print("-" * 50)

    HDR_FORMAT = '<BH'
    HDR_SIZE = struct.calcsize(HDR_FORMAT)

    current_candle = {
        "open": None,
        "high": 0,
        "low": float('inf'),
        "close": 0,
        "volume": 0,
        "time": 0
    }
    latest_snapshot = None

    while True:
        try:
            while True:
                data = sock.recv(1024)
                msg_type, msg_size = struct.unpack(HDR_FORMAT, data[:HDR_SIZE])
                payload = data[HDR_SIZE : HDR_SIZE + msg_size]

                if msg_type == 1:
                    open, high, low, close, vol, is_active, time = struct.unpack('<QQQQIBQ', payload)

                    current_candle["open"] = open
                    current_candle["high"] = high
                    current_candle["low"] = low
                    current_candle["close"] = close
                    current_candle["volume"] = vol
                    current_candle["time"] = time
                elif msg_type == 2:
                    latest_snapshot = payload

        except BlockingIOError:
            pass

        if clients:
            if current_candle["close"] is not None:
                candle_packet = {
                    "type": "CANDLE",
                    "data": current_candle
                }

                websockets.broadcast(clients, json.dumps(candle_packet))
            if latest_snapshot is not None:
                num_bids, num_asks, bids, asks = parse_snapshot(latest_snapshot)
                snapshot_packet = {
                    "type": "SNAPSHOT",
                    "num_bids": num_bids,
                    "num_asks": num_asks,
                    "bids": [{"price": p, "qty": q} for p, q in bids[:num_bids]],
                    "asks": [{"price": p, "qty": q} for p, q in asks[:num_asks]]
                }

                websockets.broadcast(clients, json.dumps(snapshot_packet))
                latest_snapshot = None

        await asyncio.sleep(0.005)

engine_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
ENGINE_ADDRESS = ('127.0.0.1', 1234)

async def handle_client(websocket, path=""):
    print("New client")
    clients.add(websocket)
    try:
        async for message in websocket:
            data = json.loads(message)

            random_id = random.randint(100000, 999999)
            if data.get("action") == "PLACE_ORDER":
                payload = struct.pack("<QQIBBB",
                                      random_id,
                                      data['price'],
                                      data['quantity'],
                                      data['side'],
                                      data['tif'],
                                      data['type'])

                engine_sock.sendto(payload, ENGINE_ADDRESS)
                print(f"Sent manual order to engine: {data}")
    finally:
        print("Client disconnected")
        clients.remove(websocket)

async def main():
    ws_server = websockets.serve(handle_client, "localhost", 8765)

    await asyncio.gather(
        ws_server,
        udp_listener()
    )

if __name__ == "__main__":
    asyncio.run(main())