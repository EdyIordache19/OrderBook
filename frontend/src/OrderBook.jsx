import React from 'react';

const OrderBook = ({ snapshot }) => {
    if (!snapshot) return <div>Loading...</div>;

    return (
        <table className="orderbook" style={{ borderCollapse: 'collapse', width: '200px' }}>
            <thead>
                <tr>
                    <th>QTY</th>
                    <th>PRICE</th>
                </tr>
            </thead>
            <tbody id='snapshot-body'>
                {/* Asks (Red) */}
                {snapshot.asks.slice().reverse().map((ask, i) => (
                    <tr key={`ask-${i}`} style={{ color: 'red' }}>
                        <td>{ask.qty}</td>
                        <td>{ask.price}</td>
                    </tr>
                ))}

                {/* Spread (Optional empty row separator) */}
                <tr className="spread">
                    <td colSpan="2" style={{ height: '10px' }}></td>
                </tr>

                {/* Bids (Green) */}
                {snapshot.bids.map((bid, i) => (
                    <tr key={`bid-${i}`} style={{ color: 'green' }}>
                        <td>{bid.qty}</td>
                        <td>{bid.price}</td>
                    </tr>
                ))}
            </tbody>
        </table>
    );
};

export default OrderBook;