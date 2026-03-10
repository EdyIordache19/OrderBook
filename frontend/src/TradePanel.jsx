import React, { useState, useEffect } from 'react';

const TradePanel = ({ accountInfo, currentPrice }) => {
    const [price, setPrice] = useState(0);
    const [quantity, setQuantity] = useState('');
    const [websocket, setWebsocket] = useState('');

    const [orderType, setOrderType] = useState('LIMIT');

    const usdBalance = accountInfo?.usd || 0;
    const equityBalance = accountInfo?.equity || 0;

    const safeMarketPrice = currentPrice || 0;
    const totalAccountValue = usdBalance + (equityBalance * safeMarketPrice);
    const totalProfit = totalAccountValue - 1000000;

    useEffect(() => {
        const websocket = new WebSocket("ws://localhost:8765");

        setWebsocket(websocket);
        return () => websocket.close();
    }, []);

    const handleTrade = (type) => {
        if (!quantity) {
            console.log("Please enter quantity");
            alert("Please enter quantity");
        }
        if (orderType === "LIMIT" && !price) {
            console.log("Please enter price");
            alert("Please enter price");
        }

        console.log(`Executing ${type} order | QTY: ${quantity} | PRICE: ${price}`);

        let orderPrice = parseInt(price);
        if (orderType === "MARKET") {
            orderPrice = type === "BUY" ? 9999 : 0;
        }

        const order = {
            action: "PLACE_ORDER",
            price: orderPrice,
            quantity: parseInt(quantity),
            side: type == "BUY" ? 0 : 1,
            tif: 0,
            type: orderType === "LIMIT" ? 0 : 1
        };

        websocket.send(JSON.stringify(order));
    };

    return (
        <div id="trading-panel">
            <div className="order-type-tabs">
                <button
                    className={`tab-btn ${orderType === 'LIMIT' ? 'active' : ''}`}
                    onClick={() => setOrderType('LIMIT')}
                >
                    Limit
                </button>
                <button
                    className={`tab-btn ${orderType === 'MARKET' ? 'active' : ''}`}
                    onClick={() => setOrderType('MARKET')}
                >
                    Market
                </button>
            </div>

            {/* Input Fields Container */}
            <div className="input-group">
                <div className="input-field">
                    <label>Price</label>
                    <input
                        type={orderType === 'MARKET' ? "text" : "number"}
                        value={orderType === 'MARKET' ? 'Market Price' : price}
                        onChange={(e) => setPrice(e.target.value)}
                        placeholder="0.00"
                        disabled={orderType === 'MARKET'}
                        className={orderType === 'MARKET' ? "disabled-input" : ""}
                    />
                </div>

                <div className="input-field">
                    <label>Quantity</label>
                    <input
                        type="number"
                        value={quantity}
                        onChange={(e) => setQuantity(e.target.value)}
                        placeholder="0"
                    />
                </div>
            </div>

            {/* Buttons Row */}
            <div className="buttons-div">
                <button id="sell-button" onClick={() => handleTrade('SELL')}>SELL</button>
                <button id="buy-button" onClick={() => handleTrade('BUY')}>BUY</button>
            </div>

            <div className="ledger-section">
                <div className="ledger-header">User Ledger</div>

                <div className="ledger-row">
                    <span className="ledger-label">USD Balance</span>
                    <span className="ledger-value usd-value">
                        ${usdBalance.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
                    </span>
                </div>

                <div className="ledger-row">
                    <span className="ledger-label">Equity (Shares)</span>
                    <span className="ledger-value equity-value">
                        {equityBalance.toLocaleString()}
                    </span>
                </div>
            </div>

            <div className="ledger-row">
                <span className="ledger-label" style={{ color: '#d1d4dc', fontWeight: 'bold' }}>Est. Total Value</span>

                {/* Dynamic Color Logic: Green if > $1M, Red if < $1M, White if exactly $1M */}
                <span className="ledger-value" style={{
                    fontWeight: 'bold',
                    color: totalAccountValue > 1000000 ? '#0ECB81' : totalAccountValue < 1000000 ? '#F6465D' : '#EAECEF'
                }}>
                    ${totalAccountValue.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
                </span>
            </div>

            <div className="ledger-row">
                <span className="ledger-label" style={{ color: '#d1d4dc', fontWeight: 'bold' }}>Est. Total P&L</span>

                {/* Dynamic Color Logic: Green if > $0, Red if < $0, White if exactly $0 */}
                <span className="ledger-value" style={{
                    fontWeight: 'bold',
                    color: totalProfit > 0 ? '#0ECB81' : totalProfit < 0 ? '#F6465D' : '#EAECEF'
                }}>
                    ${totalProfit.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
                </span>
            </div>
        </div>
    );
};

export default TradePanel;