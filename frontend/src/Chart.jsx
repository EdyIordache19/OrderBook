import { createChart, CandlestickSeries } from 'lightweight-charts';
import React, { useEffect, useRef } from 'react';

export const Chart = ({ latestTrade }) => {
    const chartContainerRef = useRef();
    const candlestickSeriesRef = useRef();
    const currentBarRef = useRef(null);

    useEffect(() => {
        const chart = createChart(chartContainerRef.current, {
            width: 1400, height: 800,
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

        const tradeTime = Math.floor(Date.now() / 100);
        const data = latestTrade.data;

        if (!currentBarRef.current || currentBarRef.current.time != tradeTime) {
            const openPrice = currentBarRef.current ? currentBarRef.current.close : data.open;
            currentBarRef.current = {
                time: tradeTime,
                open: openPrice,
                high: Math.max(openPrice, data.high),
                low: Math.min(openPrice, data.low),
                close: data.close
            }
        } else {
            currentBarRef.current.high = Math.max(currentBarRef.current.high, data.high);
            currentBarRef.current.low = Math.min(currentBarRef.current.low, data.low);
            currentBarRef.current.close = data.close;
        }

        candlestickSeriesRef.current.update(currentBarRef.current);
    }, [latestTrade]);

    return <div ref={chartContainerRef} />;
};

export default Chart;