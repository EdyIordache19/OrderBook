import { createChart, CandlestickSeries, HistogramSeries } from 'lightweight-charts';
import React, { useEffect, useRef } from 'react';

export const Chart = ({ latestTrade }) => {
    const chartContainerRef = useRef();
    const candlestickSeriesRef = useRef();
    const currentBarRef = useRef(null);
    const volumeSeriesRef = useRef();

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

        candlestickSeriesRef.current.priceScale().applyOptions({
            scaleMargins: {
                top: 0.1,
                bottom: 0.3,
            },
        });

        volumeSeriesRef.current = chart.addSeries(HistogramSeries, {
            priceFormat: {
                type: 'volume',
            },
            priceScaleId: '',
        });

        volumeSeriesRef.current.priceScale().applyOptions({
            scaleMargins: {
                top: 0.8,
                bottom: 0.0,
            },
        });

        return () => chart.remove();
    }, []);

    useEffect(() => {
        if (!latestTrade || !candlestickSeriesRef.current) return;

        const data = latestTrade.data;
        const tradeTime = data.time;
        const isUp = data.close >= data.open;

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

        volumeSeriesRef.current.update({
            time: tradeTime,
            value: data.volume,
            color: isUp ? 'rgba(38, 166, 154, 0.5)' : 'rgba(239, 83, 80, 0.5)'
        });
    }, [latestTrade]);

    return <div ref={chartContainerRef} />;
};

export default Chart;