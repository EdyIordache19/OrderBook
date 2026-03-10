import React, { useEffect, useState } from 'react';
import OrderBook from './OrderBook';
import Chart from "./Chart";
import Header from "./Header";
import ProgressBar from './ProgressBar';
import DOM from './DOM';
import TradePanel from './TradePanel';

function App(props) {
    const [snapshot, setSnapshot] = useState(null);
    const [latestTrade, setLatestTrade] = useState(null);
    const [accountInfo, setAccountInfo] = useState(null);

    useEffect(() => {
        const ws = new WebSocket('ws://localhost:8765');

        ws.onmessage = (event) => {
            const data = JSON.parse(event.data);

            if (data.type == "SNAPSHOT") {
                setSnapshot(data);
            } else if (data.type == "CANDLE") {
                setLatestTrade(data);
            } else if (data.type == "ACCOUNT_INFO") {
                setAccountInfo(data);
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
                            <TradePanel accountInfo={accountInfo} />
                        </div>
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