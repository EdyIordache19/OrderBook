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

def parse_open_orders(payload):
    if len(payload) < 1:
        return 0, []

    count = struct.unpack_from('<B', payload)[0]
    offset = struct.calcsize('<B')

    ROW_FMT = '<QQBBQIB'
    ROW_SIZE = struct.calcsize(ROW_FMT)

    if len(payload) < 1 + count * ROW_SIZE:
        return 0, []

    open_orders = []
    for i in range(count):
        o_id, o_timestamp, o_type, o_side, o_price, o_qty, o_filled = struct.unpack_from(ROW_FMT, payload, offset)
        offset += ROW_SIZE

        open_orders.append((o_id, o_timestamp, o_type, o_side, o_price, o_qty, o_filled))

    return count, open_orders

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
    latest_account_info = None
    latest_open_orders = None
    latest_orders_history = None

    while True:
        try:
            while True:
                data = sock.recv(65535)
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
                elif msg_type == 4:
                    latest_account_info = payload
                elif msg_type == 5:
                    latest_open_orders = payload
                elif msg_type == 6:
                    latest_orders_history = payload

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
            if latest_account_info is not None:
                usd, equity = struct.unpack('<qq', latest_account_info)
                account_info_packet = {
                    "type": "ACCOUNT_INFO",
                    "usd": usd,
                    "equity": equity
                }
                websockets.broadcast(clients, json.dumps(account_info_packet))
                latest_account_info = None
            if latest_open_orders is not None:
                count, open_orders = parse_open_orders(latest_open_orders)
                open_orders_packet = {
                    "type": "OPEN_ORDERS",
                    "count": count,
                    "open_orders": [{
                        "id": o_id,
                        "time": o_timestamp,
                        "type": o_type,
                        "side": o_side,
                        "price": o_price,
                        "amount": o_qty,
                        "filled": o_filled,
                        "status": "filled"
                    } for o_id, o_timestamp, o_type, o_side, o_price, o_qty, o_filled in open_orders]
                }

                websockets.broadcast(clients, json.dumps(open_orders_packet))
                latest_open_orders = None
            if latest_orders_history is not None:
                id, user_id, time, type, side, price, init_qty, status = struct.unpack('<QQQBBQIB', latest_orders_history)
                orders_history_packet = {
                    "type": "ORDERS_HISTORY",
                    "order_history": {
                        "id": id,
                        "user_id": user_id,
                        "timestamp": time,
                        "type": type,
                        "side": side,
                        "price": price,
                        "amount": init_qty,
                        "status": status
                    }
                }

                websockets.broadcast(clients, json.dumps(orders_history_packet))
                latest_orders_history = None
        await asyncio.sleep(0.005)

engine_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
engine_sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

def ensure_connected():
    try:
        engine_sock.getpeername()
    except socket.error:
        try:
            engine_sock.connect(('127.0.0.1', 1234))
        except socket.error:
            pass

async def handle_client(websocket, path=""):
    print("New client")
    clients.add(websocket)
    try:
        async for message in websocket:
            data = json.loads(message)

            random_id = random.randint(100000, 999999)
            if data.get("action") == "PLACE_ORDER":
                ensure_connected()
                payload = struct.pack("<QIQIBBB",
                                      random_id,
                                      1, # User ID
                                      data['price'],
                                      data['quantity'],
                                      data['side'],
                                      data['type'],
                                      data['tif'])
                try:
                    engine_sock.sendall(payload)
                    print(f"Sent manual order to engine: {data}")
                except Exception as e:
                    print(f"Error sending order: {e}")

            if data.get("action") == "CANCEL_ORDER":
                ensure_connected()
                payload = struct.pack("<QIQIBBB",
                                      data['id'],
                                      1, # User ID
                                      data['price'],
                                      0, # Quantity 0 (doesn't matter)
                                      data['side'],
                                      2, # CANCEL Type,
                                      0, # TIF 0 (doesn't matter)
                                      )
                try:
                    engine_sock.sendall(payload)
                    print(f"Sent CANCEL order to engine: {data}")
                except Exception as e:
                    print(f"Error sending cancel: {e}")

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