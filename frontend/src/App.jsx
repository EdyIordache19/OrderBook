import React, { useEffect, useState } from 'react';
import OrderBook from './OrderBook';
import Chart from "./Chart"
import Header from "./Header"

const initialData = [
    { time: '2018-12-22', value: 32.51 },
    { time: '2018-12-23', value: 31.11 },
    { time: '2018-12-24', value: 27.02 },
    { time: '2018-12-25', value: 27.32 },
    { time: '2018-12-26', value: 25.17 },
    { time: '2018-12-27', value: 28.89 },
    { time: '2018-12-28', value: 25.46 },
    { time: '2018-12-29', value: 23.92 },
    { time: '2018-12-30', value: 22.68 },
    { time: '2018-12-31', value: 22.67 },
];

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
                <div id="chart-div">
                    <Chart latestTrade={latestTrade}></Chart>
                </div>
                <br></br>
                <div id="snapshot-div" style={{ width: '300px' }}>
                    <OrderBook id="snapshot" snapshot={snapshot} />
                </div>
            </div>
        </div>
    );
}

export default App;