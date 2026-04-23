import React, { useEffect, useState } from 'react';
import OrderBook from './OrderBook';
import Chart from "./Chart";
import Header from "./Header";
import ProgressBar from './ProgressBar';
import DOM from './DOM';
import TradePanel from './TradePanel';
import OrdersTable from './OrdersTable';


function isObject(x) {
  return x !== null && typeof x === "object" && !Array.isArray(x);
}

function toFiniteNumber(x, fallback = 0) {
  const n = typeof x === "number" ? x : Number(x);
  return Number.isFinite(n) ? n : fallback;
}

function clampInt(n, min, max) {
  n = Math.trunc(toFiniteNumber(n, min));
  if (n < min) return min;
  if (n > max) return max;
  return n;
}

function safeJsonParse(raw) {
  if (typeof raw !== "string") {
    return { ok: false, error: "non-string", value: null };
  }
  try {
    return { ok: true, error: null, value: JSON.parse(raw) };
  } catch (e) {
    return { ok: false, error: String(e), value: null };
  }
}

function normalizeSide(side) {
  // Accept "BUY"/"SELL" or numeric enums 0/1
  if (side === "BUY" || side === "SELL") return side;
  if (side === 0) return "BUY";
  if (side === 1) return "SELL";
  return "UNKNOWN";
}

function normalizeOrderType(type) {
  // Accept strings or numeric enums (LIMIT=0, MARKET=1, CANCEL=2)
  if (type === "LIMIT" || type === "MARKET" || type === "CANCEL") return type;
  if (type === 0) return "LIMIT";
  if (type === 1) return "MARKET";
  if (type === 2) return "CANCEL";
  return "UNKNOWN";
}

function normalizeStatus(status) {
    if (status === "FILLED" || status === "CANCELED") return status;
    if (status === 0) return "FILLED"
    if (status === 1) return "CANCELED"
    return "UNKNOWN";
}

function sanitizeSnapshotPacket(msg) {
  if (!isObject(msg) || msg.type !== "SNAPSHOT") return null;

  const numBids = clampInt(msg.num_bids, 0, 10);
  const numAsks = clampInt(msg.num_asks, 0, 10);

  const bidsIn = Array.isArray(msg.bids) ? msg.bids : [];
  const asksIn = Array.isArray(msg.asks) ? msg.asks : [];

  const bids = bidsIn.slice(0, numBids).map((lvl) => ({
    price: toFiniteNumber(lvl?.price, 0),
    qty: clampInt(lvl?.qty, 0, 2 ** 31 - 1),
  }));

  const asks = asksIn.slice(0, numAsks).map((lvl) => ({
    price: toFiniteNumber(lvl?.price, 0),
    qty: clampInt(lvl?.qty, 0, 2 ** 31 - 1),
  }));

  return { type: "SNAPSHOT", num_bids: bids.length, num_asks: asks.length, bids, asks };
}

function sanitizeCandlePacket(msg) {
  if (!isObject(msg) || msg.type !== "CANDLE") return null;
  const d = isObject(msg.data) ? msg.data : {};

  // Keep the same shape Chart currently expects: latestTrade.data.close, etc.
  return {
    type: "CANDLE",
    data: {
      open: toFiniteNumber(d.open, 0),
      high: toFiniteNumber(d.high, 0),
      low: toFiniteNumber(d.low, 0),
      close: toFiniteNumber(d.close, 0),
      volume: clampInt(d.volume, 0, 2 ** 31 - 1),
      time: toFiniteNumber(d.time, 0),
    },
  };
}

function sanitizeAccountInfoPacket(msg) {
  if (!isObject(msg) || msg.type !== "ACCOUNT_INFO") return null;
  return {
    type: "ACCOUNT_INFO",
    usd: toFiniteNumber(msg.usd, 0),
    equity: toFiniteNumber(msg.equity, 0),
  };
}

function sanitizeOpenOrdersPacket(msg) {
  if (!isObject(msg) || msg.type !== "OPEN_ORDERS") return null;

  const rowsIn = Array.isArray(msg.open_orders) ? msg.open_orders : [];

  const open_orders = rowsIn
    .map((row) => {
      if (!isObject(row)) return null;

      const id = toFiniteNumber(row.id, NaN);
      if (!Number.isFinite(id) || id <= 0) return null;

      const price = toFiniteNumber(row.price, NaN);
      const amount = toFiniteNumber(row.amount, NaN);
      if (!Number.isFinite(price) || price < 0) return null;
      if (!Number.isFinite(amount) || amount < 0) return null;

      // filled is currently your "filled_pct" on the wire; clamp to [0,100]
      const filledPct = clampInt(row.filled, 0, 100);

      return {
        id,
        time: row.time ?? 0, // you can format timestamp later
        type: normalizeOrderType(row.type),
        side: normalizeSide(row.side),
        price,
        amount,
        filled: filledPct,
      };
    })
    .filter(Boolean);

  return { type: "OPEN_ORDERS", open_orders };
}

function sanitizeOrdersHistoryPacket(msg) {
    if (!isObject(msg) || msg.type != "ORDERS_HISTORY") return null;

    const data = msg.order_history || {};

    return {
        id: toFiniteNumber(data.id),
        user_id: toFiniteNumber(data.user_id),
        timestamp: data.timestamp ?? 0,
        type: normalizeOrderType(data.type),
        side: normalizeSide(data.side),
        price: toFiniteNumber(data.price),
        amount: toFiniteNumber(data.amount),
        status: normalizeStatus(data.status)
    };
}

// inside ws.onmessage use this:
async function readWsDataAsText(eventData) {
  if (typeof eventData === "string") return eventData;
  if (eventData instanceof Blob) return await eventData.text();
  if (eventData instanceof ArrayBuffer) return new TextDecoder().decode(eventData);
  // last resort: string-coerce
  return String(eventData);
}

function App(props) {
    const [snapshot, setSnapshot] = useState(null);
    const [latestTrade, setLatestTrade] = useState(null);
    const [accountInfo, setAccountInfo] = useState(null);
    const [openOrders, setOpenOrders] = useState(null);
    const [ordersHistory, setOrdersHistory] = useState([]);

    useEffect(() => {
        const ws = new WebSocket('ws://localhost:8765');

        ws.onmessage = async (event) => {
            const raw = await readWsDataAsText(event.data);

            const parsed = safeJsonParse(raw);
            if (!parsed.ok) {
                console.warn("WS frame not JSON:", parsed.error, raw.slice(0, 200));
                return;
            }

            const msg = parsed.value;

            switch (msg?.type) {
                case "SNAPSHOT": {
                    const clean = sanitizeSnapshotPacket(msg);
                    if (clean) setSnapshot(clean);
                    return;
                }
                case "CANDLE": {
                    const clean = sanitizeCandlePacket(msg);
                    if (clean) setLatestTrade(clean);
                    return;
                }
                case "ACCOUNT_INFO": {
                    const clean = sanitizeAccountInfoPacket(msg);
                    if (clean) setAccountInfo(clean);
                    return;
                }
                case "OPEN_ORDERS": {
                    const clean = sanitizeOpenOrdersPacket(msg);
                    if (clean) setOpenOrders(clean.open_orders);
                    return;
                }
                case "ORDERS_HISTORY":
                    console.log(msg)
                    const clean = sanitizeOrdersHistoryPacket(msg);
                    console.log(clean)
                    if (clean) {
                        setOrdersHistory(prevHistory => [clean, ...prevHistory]);
                    }
                    return;
                default:
                    // Ignore unknown message types safely
                    return;
            }
        };

        return () => ws.close();
    }, []);

    return (
        <div>
            <div className="app-header">
                <Header />
            </div>
            {/* Renamed "footer" to "main-content" because it holds the core layout */}
            <div className="main-content">
                <div className="layout-row">
                    <div className="chart-container">
                        <Chart latestTrade={latestTrade}></Chart>
                    </div>

                    <div className="side-column">
                        <div className="panel" id='trading-panel'>
                            <TradePanel accountInfo={accountInfo} currentPrice={latestTrade?.data?.close || 0} />
                        </div>
                    </div>
                </div>

                <div className="layout-row" style={{ paddingTop: '0px' }}>
                    <div style={{ width: '100%' }}>
                        <OrdersTable activeOrders={openOrders} orderHistory={ordersHistory}/>
                    </div>
                </div>

                <div className="layout-row layout-bottom-row">
                    <div className="chart-container">
                        <DOM snapshot={snapshot} />
                    </div>
                    <div className="side-column">
                        <div className="panel">
                            <OrderBook snapshot={snapshot} />
                        </div>
                        <div className="panel progress-panel">
                            <ProgressBar snapshot={snapshot} />
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
}

export default App;