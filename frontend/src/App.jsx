import React, { useEffect, useState } from 'react';
import OrderBook from './OrderBook';
import Chart from "./Chart";
import Header from "./Header";
import ProgressBar from './ProgressBar';
import DOM from './DOM';

function App(props) {
    const [snapshot, setSnapshot] = useState(null);
    const [latestTrade, setLatestTrade] = useState(null);

    useEffect(() => {
        const ws = new WebSocket('ws://localhost:8765');

        ws.onmessage = (event) => {
            const data = JSON.parse(event.data);

            if (data.type == "SNAPSHOT") {
              setSnapshot(data);
            } else if (data.type == "CANDLE") {
              setLatestTrade(data)
            }
        };

        return () => ws.close();
    }, []);

    return (
        <div>
            <div id="header">
                <Header />
            </div>
            <div id="footer">
                <div id="main-div">
                    <div id="chart-div">
                        <Chart latestTrade={latestTrade}></Chart>
                    </div>
                    <div id="right-side">
                        <div id="snapshot-div" style={{ width: '300px' }}>
                            <OrderBook id="snapshot" snapshot={snapshot} />
                        </div>
                        <div id="bids-asks-div">
                            <ProgressBar snapshot={snapshot} />
                        </div>
                    </div>
                </div>
                <div id="dom-div">
                    <DOM snapshot={snapshot} />
                </div>
            </div>
        </div>
    );
}

export default App;