#include "engine.hpp"
#include "main.hpp"

#include <atomic>
#include <algorithm>
#include <cstdint>
#include <x86intrin.h>
#include <emmintrin.h>
#include <string.h>

Engine::Engine(RingBuffer<Order>& _ordersBuffer, std::atomic<bool>& _running, OrderBook& _orderBook, uint64_t numOrders,
    std::atomic<int64_t>& _usd_balance, std::atomic<int64_t>& _equity_balance, double _tsc_ticks_per_ns)
    : orderBook(_orderBook),
      ordersBuffer(_ordersBuffer),
      running(_running),
      usd_balance(_usd_balance),
      equity_balance(_equity_balance),
      tsc_ticks_per_ns(_tsc_ticks_per_ns)
{
    core_to_core_latencies.resize(numOrders);
    engine_latencies.resize(numOrders);
}

void Engine::run() {
    pin_thread_to_core(1);

    // Trades array for multiple trades happening for the same order
    Trade trades[256] = {Trade()};
    uint8_t trades_count = 0;

    size_t latency_idx = 0;
    // Best-effort drain of ordersBuffer
    while (running.load(std::memory_order_relaxed) || !ordersBuffer.is_empty()) {
        // Pop orders from SPSC ring buffer, from gateway to engine
        Order order;
        if (!ordersBuffer.pop(order)) {
            uint64_t engine_start_ts = __rdtsc();

            // Treat KILL and CANCEL orders
            if (order.type == OrderType::KILL) {
                break;
            } else if (order.type == OrderType::CANCEL) {
                // Add CANCEL history node to history buffer if from user
                if (order.user_id == 1) {
                    Order* order_to_be_removed = orderBook.getOrderLookup()[order.id];
                    OrderHistory orderHistory(order_to_be_removed->id,
                        order_to_be_removed->user_id,
                        order_to_be_removed->type,
                        order_to_be_removed->side,
                        order_to_be_removed->price,
                        0, 0,
                        order_to_be_removed->initial_quantity,
                        HistoryStatus::CANCELED);

                    orderBook.historyBuffer.push(orderHistory);
                }

                orderBook.removeOrder(order.id);
                continue;
            }

            // If order was submitted by the user, and can't be processed
            // (not enough balance / equity balance)
            if (order.user_id == 1) {
                if (order.side == Side::BUY) {
                    int64_t cost = order.price * order.quantity;
                    if (usd_balance.load(std::memory_order_relaxed) < cost) continue;
                } else {
                    if (equity_balance.load(std::memory_order_relaxed) < order.quantity) continue;
                }
            }

            trades_count = 0;

            if (order.tif == TimeInForce::IOC) {
                // Just process (match order with book) and dismiss the remainder
                orderBook.processOrder(order, trades, trades_count);
            } else if (order.tif == TimeInForce::FOK) {
                // Check if it can be filled entirely and process order
                if (orderBook.canFill(order)) {
                    orderBook.processOrder(order, trades, trades_count);
                }
            } else {
                // Process order and add remainder to book
                uint32_t remaining_qty = orderBook.processOrder(order, trades ,trades_count);

                order.quantity = remaining_qty;
                if (remaining_qty > 0 && order.type != OrderType::MARKET) {
                    orderBook.addOrder(order);
                }
            }

            for (uint8_t i = 0; i < trades_count; i++) {
                Trade trade = trades[i];
                // Calculate balance for trades done by user
                uint64_t trade_value = trade.price * trade.quantity;
                if (trade.maker_user_id == 1) {
                    if (order.side == Side::BUY) {
                        usd_balance.fetch_sub(trade_value, std::memory_order_relaxed);
                        equity_balance.fetch_add(trade.quantity, std::memory_order_relaxed);
                    } else {
                        usd_balance.fetch_add(trade_value, std::memory_order_relaxed);
                        equity_balance.fetch_sub(trade.quantity, std::memory_order_relaxed);
                    }
                }

                if (trade.taker_user_id == 1) {
                    if (order.side == Side::BUY) {
                        usd_balance.fetch_add(trade_value, std::memory_order_relaxed);
                        equity_balance.fetch_sub(trade.quantity, std::memory_order_relaxed);
                    } else {
                        usd_balance.fetch_sub(trade_value, std::memory_order_relaxed);
                        equity_balance.fetch_add(trade.quantity, std::memory_order_relaxed);
                    }
                }

                // Push every trade to matchBuffer of orderBook
                orderBook.matchBuffer.push(trade);
            }

            unsigned int ui;
            uint64_t end_ts = __rdtscp(&ui);

            uint64_t cycles_num = end_ts - order.core_to_core_ts;
            core_to_core_latencies[latency_idx] = cycles_num / tsc_ticks_per_ns;

            cycles_num = end_ts - engine_start_ts;
            engine_latencies[latency_idx] = cycles_num / tsc_ticks_per_ns;

            latency_idx++;
        } else {
            continue;
        }
    }

    core_to_core_latencies.resize(latency_idx);
    engine_latencies.resize(latency_idx);
}

void Engine::stop() {
    running = false;
    if (engineThread.joinable()) {
        engineThread.join();
    }
}

void Engine::printStats() {
    std::sort(core_to_core_latencies.begin(), core_to_core_latencies.end());
    std::sort(engine_latencies.begin(), engine_latencies.end());

    double median = core_to_core_latencies[core_to_core_latencies.size() * 0.5];
    double p99 = core_to_core_latencies[core_to_core_latencies.size() * 0.99];
    double p99_9 = core_to_core_latencies[core_to_core_latencies.size() * 0.999];

    std::cout << "\n---------PRINTING LATENCY STATS-----------\n\n";
    std::cout << "CORE TO CORE LATENCY STATS:\n";
    std::cout << "Median Latency:  " << median << " ns\n";
    std::cout << "99\% Latency:     " << p99 << " ns\n";
    std::cout << "99.9\% Latency:   " << p99_9 << " ns\n\n";

    median = engine_latencies[engine_latencies.size() * 0.5];
    p99 = engine_latencies[engine_latencies.size() * 0.99];
    p99_9 = engine_latencies[engine_latencies.size() * 0.999];

    std::cout << "ENGINE LATENCY STATS:\n";
    std::cout << "Median Latency:  " << median << " ns\n";
    std::cout << "99\% Latency:     " << p99 << " ns\n";
    std::cout << "99.9\% Latency:   " << p99_9 << " ns\n\n";

    std::cout << "USD BALANCE: " << usd_balance << '\n';
    std::cout << "EQUITY BALANCE: " << equity_balance << '\n';
}