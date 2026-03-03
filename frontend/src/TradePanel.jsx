import React, { useState } from 'react';

const TradePanel = () => {
    // State to hold the user's input values
    const [price, setPrice] = useState('');
    const [quantity, setQuantity] = useState('');

    const handleTrade = (type) => {
        console.log(`Executing ${type} order | QTY: ${quantity} | PRICE: ${price}`);
        // Add your WebSocket or execution logic here
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