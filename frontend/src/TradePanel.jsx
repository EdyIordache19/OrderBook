import React, { useState, useEffect } from 'react';

const TradePanel = () => {
    const [price, setPrice] = useState('');
    const [quantity, setQuantity] = useState('');
    const [websocket, setWebsocket] = useState('');

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
        if (!price) {
            console.log("Please enter price");
            alert("Please enter price");
        }

        console.log(`Executing ${type} order | QTY: ${quantity} | PRICE: ${price}`);

        const order = {
            action: "PLACE_ORDER",
            price: parseFloat(price),
            quantity: parseInt(quantity),
            side: type == "BUY" ? 0 : 1,
            tif: 0,
            type: 0
        };

        websocket.send(JSON.stringify(order));
    };

    return (
        <div id="trading-panel">
            {/* Top Buttons Row */}
            <div className="buttons-div">
                <button id="sell-button" onClick={() => handleTrade('SELL')}>SELL</button>
                <button id="buy-button" onClick={() => handleTrade('BUY')}>BUY</button>
            </div>

            {/* Input Fields Container */}
            <div className="input-group">
                <div className="input-field">
                    <label>Price</label>
                    <input
                        type="number"
                        value={price}
                        onChange={(e) => setPrice(e.target.value)}
                        placeholder="0.00"
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
        </div>
    );
};

export default TradePanel;