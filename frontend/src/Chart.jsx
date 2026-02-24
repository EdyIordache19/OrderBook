import { createChart, CandlestickSeries } from 'lightweight-charts';
import React, { useEffect, useRef } from 'react';

export const Chart = ({ latestTrade }) => {
    const chartContainerRef = useRef();
    const candlestickSeriesRef = useRef();
    const currentBarRef = useRef(null);

    useEffect(() => {
        const chart = createChart(chartContainerRef.current, {
            width: 1500, height: 900,
            layout: {
                background: { color: '#131722' },
                textColor: '#d1d4dc'
            },
            grid: { vertLines: { color: '#2b2b43' }, horzLines: { color: '#2b2b43' } },
        });

        candlestickSeriesRef.current = chart.addSeries(CandlestickSeries);

        return () => chart.remove();
    }, []);

    useEffect(() => {
        if (!latestTrade || !candlestickSeriesRef.current) return;

        const tradeTime = Math.floor(Date.now() / 1000);
        const price = Number(latestTrade.price);

        if (!currentBarRef.current || currentBarRef.current.time !== tradeTime) {
            // If there is a previous candle use its Close as new Open
            const newOpen = currentBarRef.current ? currentBarRef.current.close : price;

            // Create the new candle
            currentBarRef.current = {
                time: tradeTime,
                open: newOpen,
                high: Math.max(newOpen, price),
                low: Math.min(newOpen, price),
                close: price,
            };
        } else {
            // Update the existing candle
            currentBarRef.current.high = Math.max(currentBarRef.current.high, price);
            currentBarRef.current.low = Math.min(currentBarRef.current.low, price);
            currentBarRef.current.close = price;
        }

        candlestickSeriesRef.current.update(currentBarRef.current);
    }, [latestTrade]);

    return <div ref={chartContainerRef} />;
};

export default Chart;