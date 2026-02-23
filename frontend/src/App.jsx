import React, { useEffect, useState } from 'react';
import OrderBook from './OrderBook';

function App() {
    const [snapshot, setSnapshot] = useState(null);

    useEffect(() => {
        const ws = new WebSocket('ws://localhost:8765');

        ws.onmessage = (event) => {
            const data = JSON.parse(event.data);

            if (data.type == "SNAPSHOT") {
              setSnapshot(data);
            }
        };

        return () => ws.close();
    }, []);

    return (
        <div style={{ display: 'flex', background: '#131722', color: 'white' }}>
            <div style={{ width: '300px' }}>
                <OrderBook snapshot={snapshot} />
            </div>
        </div>
    );
}

export default App;