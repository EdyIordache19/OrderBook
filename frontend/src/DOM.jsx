import React, { useEffect, useRef } from 'react'
import { AreaSeries, createChartEx, LineType } from 'lightweight-charts';

// const data = [];
// for (let i = 0; i < 5000; i++) {
//     data.push({
//         time: i * 0.25,
//         value: Math.sin(i / 100) + i / 500,
//     });
// }

function markWithGreaterWeight(a, b) {
	return a.weight > b.weight ? a : b;
}

class HorzScaleBehaviorPrice {
	options() {}
	setOptions() {}
	preprocessData() {}
	updateFormatter() {}
	createConverterToInternalObj() {
		return price => price;
	}

	key(item) {
		return item;
	}

	cacheKey(item) {
		return item;
	}

	convertHorzItemToInternal(item) {
		return item;
	}

	formatHorzItem(item) {
		const tp = item;
		return tp;
	}

	formatTickmark(tickMark) {
		return tickMark.time;
	}

	maxTickMarkWeight(tickMarks) {
		return tickMarks.reduce(markWithGreaterWeight, tickMarks[0]).weight;
	}

	fillWeightsForPoints(sortedTimePoints, startIndex) {
		const priceWeight = price => {
			if (price === Math.ceil(price / 100) * 100) {
				return 8;
			}
			if (price === Math.ceil(price / 50) * 50) {
				return 7;
			}
			if (price === Math.ceil(price / 25) * 25) {
				return 6;
			}
			if (price === Math.ceil(price / 10) * 10) {
				return 5;
			}
			if (price === Math.ceil(price / 5) * 5) {
				return 4;
			}
			if (price === Math.ceil(price)) {
				return 3;
			}
			if (price * 2 === Math.ceil(price * 2)) {
				return 1;
			}
			return 0;
		};
		for (let index = startIndex; index < sortedTimePoints.length; ++index) {
			sortedTimePoints[index].timeWeight = priceWeight(
				sortedTimePoints[index].time
			);
		}
	}
}

export const DOM = ({ snapshot }) => {
    if (!snapshot) return <div>Loading...</div>;

    const chartContainerRef = useRef();
    const askSeriesRef = useRef();
    const bidSeriesRef = useRef();
    const chartRef = useRef();

    const asks = snapshot.asks;
    const bids = snapshot.bids;

    useEffect(() => {
        const horzItemBehavior = new HorzScaleBehaviorPrice();
        chartRef.current = createChartEx(chartContainerRef.current, horzItemBehavior, {
            width: 1400, height: 800,
            layout: {
                background: { color: '#131722' },
                textColor: '#d1d4dc'
            },
            grid: { vertLines: { color: '#2b2b43' }, horzLines: { color: '#2b2b43' } },
            timeScale: {
                timeVisible: true,
                secondsVisible: true,
            },
        });

        askSeriesRef.current = chartRef.current.addSeries(AreaSeries, {
            priceFormat: {
                type: 'volume',
            },
            lineColor: '#F6465D',
            topColor: '#F6465D',
            bottomColor: '#F6465D',
            priceScaleId: 'right',
            lineType: LineType.WithSteps,
        });

        bidSeriesRef.current = chartRef.current.addSeries(AreaSeries, {
            priceFormat: {
                type: 'volume',
            },
            lineColor: '#0ECB81',
            topColor: '#0ECB81',
            bottomColor: '#0ECB81',
            priceScaleId: 'right',
            lineType: LineType.WithSteps,
        });

        return () => chartRef.current.remove();
    }, []);

    useEffect(() => {
        const bidData = [];
        let bidQty = 0;
        bids.map(bid => {
            bidQty += bid.qty;
            bidData.push({
                time: bid.price,
                value: bidQty,
            })
        });

        bidData.reverse();

        const askData = [];
        let askQty = 0;
        asks.map(ask => {
            askQty += ask.qty;
            askData.push({
                time: ask.price,
                value: askQty,
            })
        });

        if (bidSeriesRef.current) {
            bidSeriesRef.current.setData(bidData);
        }

        if (askSeriesRef.current) {
            askSeriesRef.current.setData(askData);
        }

        if (bids.length > 0 && asks.length > 0) {
            let maxBid = bids[0].price;
            let minAsk = asks[0].price;

            if (maxBid && minAsk) {
                let midPrice = (maxBid + minAsk) / 2;

                if (chartRef.current && !isNaN(midPrice)) {
                    chartRef.current.timeScale().setVisibleRange({
                        from: midPrice - 100,
                        to: midPrice + 100
                    });
                }
            }
        }

    }, [snapshot])

    return <div ref={chartContainerRef} />;
}

export default DOM;