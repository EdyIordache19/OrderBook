import React from 'react';

const OrderBook = ({ snapshot }) => {
    if (!snapshot) return <div>Loading...</div>;

    return (
        <div className="orderbook">
            <div className="asks">
                {snapshot.asks.slice().reverse().map((ask, i) => (
                    <div key={i} style={{ display: 'flex', color: 'red' }}>
                        <span>PRICE: {ask.price}</span>
                        &nbsp;&nbsp;&nbsp;
                        <span>QTY: {ask.qty}</span>
                    </div>
                ))}
            </div>

            <div className="spread">
            </div>

            <div className="bids">
                {snapshot.bids.map((bid, i) => (
                    <div key={i} style={{ display: 'flex', color: 'green' }}>
                        <span>PRICE: {bid.price}</span>
                        &nbsp;&nbsp;&nbsp;
                        <span>QTY: {bid.qty}</span>
                    </div>
                ))}
            </div>
        </div>
    );
};

export default OrderBook;