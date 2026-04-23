import React, { useState, useEffect } from 'react';

const OrdersTable = ({ activeOrders, orderHistory }) => {
    const [activeTab, setActiveTab] = useState('OPEN');
        const [websocket, setWebsocket] = useState('');

    const openOrders = Array.isArray(activeOrders) ? activeOrders : [];

    // Toggle which array we render based on the active tab
    const history = Array.isArray(orderHistory) ? orderHistory : orderHistory;
    const displayData = activeTab === 'OPEN' ? openOrders : history;

    useEffect(() => {
        const websocket = new WebSocket("ws://localhost:8765");

        setWebsocket(websocket);
        return () => websocket.close();
    }, []);

    const handleCancel = (orderId, orderPrice, orderSide) => {
        console.log(`Canceling order ${orderId}...`);
        // We will wire this to your WebSocket later
        const order = {
            action: "CANCEL_ORDER",
            id: orderId,
            price: orderPrice,
            side: orderSide === "BUY" ? 0 : 1
        }

        websocket.send(JSON.stringify(order));

    };

    return (
        <div className="panel orders-panel">

            {/* --- Tabs --- */}
            <div className="orders-tabs">
                <button
                    className={`order-tab-btn ${activeTab === 'OPEN' ? 'active' : ''}`}
                    onClick={() => setActiveTab('OPEN')}
                >
                    Open Orders ({openOrders.length})
                </button>
                <button
                    className={`order-tab-btn ${activeTab === 'HISTORY' ? 'active' : ''}`}
                    onClick={() => setActiveTab('HISTORY')}
                >
                    Order History
                </button>
            </div>

            {/* --- Table --- */}
            <div className="table-container">
                <table className="orders-table">
                    <thead>
                        <tr>
                            <th>Time</th>
                            <th>Type</th>
                            <th>Side</th>
                            <th>Price</th>
                            <th>Amount</th>
                            {activeTab === 'OPEN' ? <th>Filled %</th> : <th>Status</th>}
                            {activeTab === 'OPEN' && <th>Action</th>}
                        </tr>
                    </thead>
                    <tbody>
                        {(!Array.isArray(displayData) || displayData.length === 0) ? (
                            <tr>
                                <td colSpan={activeTab === 'OPEN' ? 7 : 6} className="empty-state">
                                    No {activeTab.toLowerCase()} orders
                                </td>
                            </tr>
                        ) : (
                            displayData.map((order) => (
                                <tr key={order.id}>
                                    <td className="time-col">{order.time}</td>
                                    <td>{order.type}</td>
                                    <td className={order.side === 'BUY' ? 'text-buy' : 'text-sell'}>
                                        {order.side}
                                    </td>
                                    <td className="num-col">
                                        {order.type === "MARKET" ? <span>-</span> : '$' + order.price}

                                    </td>
                                    <td className="num-col">{order.amount}</td>

                                    {/* Dynamic column based on tab */}
                                    {activeTab === 'OPEN' ? (
                                        <td className="num-col">
                                            {order.filled}%
                                        </td>
                                    ) : (
                                        <td className={order.status === 'FILLED' ? 'text-buy' : 'text-gray'}>
                                            {order.status}
                                        </td>
                                    )}

                                    {/* Action Column */}
                                    <td className="action-col">
                                        {activeTab === 'OPEN' && (
                                            <button
                                                className="cancel-btn"
                                                onClick={() => handleCancel(order.id, order.price, order.side)}
                                            >
                                                Cancel
                                            </button>
                                        )}
                                    </td>
                                </tr>
                            ))
                        )}
                    </tbody>
                </table>
            </div>
        </div>
    );
};

export default OrdersTable;