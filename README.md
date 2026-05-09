# Ultra-Low Latency Spot Market Matching Engine & Trading Terminal

![Build Status](https://img.shields.io/badge/build-passing-brightgreen)
![C++](https://img.shields.io/badge/c++-17%2F20-blue)
![React](https://img.shields.io/badge/React-18-blueviolet)
![Latency](https://img.shields.io/badge/latency-230ns-success)
![Throughput](https://img.shields.io/badge/throughput-10M%20ops%2Fsec-success)

A high-performance, ultra-low latency multi-threaded spot market matching engine and institutional-grade trading terminal. Built from scratch to replicate real-world exchange infrastructure, this project leverages advanced C++ optimization techniques to achieve sub-microsecond matching latencies, paired with a modern React frontend for real-time market data visualization.

---

## System Architecture

The trading platform is split into three highly decoupled, performant layers:

### 1. The Backend (C++ Matching Engine)
An ultra-low latency, multi-threaded matching engine strictly optimized for the hot-path:
- **Zero-Allocation Hot Path:** Completely bypasses the OS memory manager using **custom pre-allocated memory pools** (`OrdersPool`).
- **Data Structures:** Built on **lock-free ring buffers** (`RingBuffer`) for concurrent thread message passing.
- **Cache Optimization:** Deeply optimized for modern CPU architectures using **cache-line padding and alignment** to prevent false sharing and minimize L3 cache misses.
- **Math & Precision:** Strictly bypasses floating-point arithmetic. Features a **Zero-Latency Engine-Level Ledger** relying purely on `uint64_t`/`int64_t` for high-frequency integer speed. Safely manages pre-trade risk and post-trade spot balance updates dynamically.
- **Network I/O:** Receives inbound order flow via **TCP** to guarantee deterministic order delivery minimizing packet loss overhead.

### 2. The Broadcaster Bridge (C++ & Python)
- **C++ Publisher:** A dedicated thread blasts state updates (Orderbook Snapshots, Candlesticks, EXECUTED/CANCELED Trades, and Account Info) via **UDP Multicast**, strictly offloading network I/O from the core matching thread.
- **Python Websocket Bridge:** A lightweight middleware (`trades_viewer.py`) that acts as a feed handler. It listens to the UDP multicast, aggregates market data, and broadcasts it to the frontend via WebSockets.

### 3. The Frontend Terminal (React / Vite)
A dark-mode, institutional-grade web trading terminal designed for active spot trading:
- **Real-Time DOM:** A mathematically reactive Limit Order Book depth-of-market (DOM) rendering at 60 FPS.
- **Components:** Interactive Candlestick charts, dynamic Trade Panel featuring Market/Limit order toggles, and live dynamically calculated P&L / estimated balances.
- **Zero-Latency Feel:** Updates balances and asset quantities instantly based on real-time WS streaming with mathematically reactive estimates.

---

## Performance & Microbenchmarks

The engine has been aggressively profiled and micro-optimized. Currently, core matching throughput peaks at **~5 Million ops/sec** under standard testing.

### Benchmark Results (1,000,000 Orders)
| Metric | No Batching (Single Order Dispatches) | High-Throughput Batching (Batches of 100) |
| :--- | :--- | :--- |
| **Throughput** | ~200,000 ops/sec | **~10,000,000 ops/sec** |
| **Median Latency**| **230 ns** | ~600 ns |
| **99% Latency** | 400 ns | ~55us |
| **99.9% Latency** | 5,000 ns | ~350us |

> *Note: By batching incoming packets, the engine hits max theoretical bandwidth. Without batching, median latency stays strictly around 200–230ns.*

---

## Visuals & Metrices

### Trading Terminal UI
*Live institutional dark-mode terminal with Real-time L2 orderbook, charting, and mathematically reactive P&L limit & market executions.*

![Trading Terminal UI Placeholder](docs/main_chart.png)
![Open Orders UI Placeholder](docs/open_orders.png)
![Order History UI Placeholder](docs/orders_history.png)
![Depth of Market UI Placeholder](docs/DOM.png)

### CPU Flamegraph & Profiling
*Analyzing the hot-path zero-allocation engine efficiency.*

![Performance Flamegraph Placeholder](docs/orderbook_flamegraph.svg)

---

## Build & Run Instructions

### Prerequisites
- **C++ Backend:** CMake 3.10+, GCC/Clang with C++17 support.
- **Python Bridge:** Python 3.8+
- **React Frontend:** Node.js 18+ and `npm`.

### Quick Start (Using `run.sh`)
The easiest way to bootstrap the entire environment (Engine, Broadcaster, and React GUI) is using the provided shell script:
```bash
# Ensure execution permissions
chmod +x run.sh
./run.sh
```

### Manual Execution

**1. Build & Run the Backend Engine:**
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./orderbook
```

**2. Start the Python WebSocket Bridge (Broadcaster):**
```bash
python3 apps/trades_viewer.py
```

**3. Launch the React Frontend:**
```bash
cd frontend
npm install
npm run dev
# Open http://localhost:5173
```

**4. Blast Orders (Benchmark Client):**
```bash
# Push 1,000,000 orders
./build/client
```

---

## Immediate Roadmap & Next Steps

- **Microsecond Optimization:** Implement CPU thread pinning (`isolcpus`) for the matching thread, optimize L3 cache hit rates, and batch RingBuffer pushes to eliminate remaining latency spikes, aiming for a consistent < 170ns matching time.
- **Cloud Deployment:** Architect a DevOps strategy to separate concerns: hosting the React UI on Vercel/Netlify while deploying the C++ Matrix and Python components on a highly optimized VPS (AWS/GCP/DigitalOcean).

---
*Developed as a demonstration of high-frequency trading infrastructure patterns, real-time networking, and full-stack performance tuning.*
