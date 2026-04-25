#include "engine.hpp"
#include "main.hpp"

#include <atomic>
#include <algorithm>

Engine::Engine(RingBuffer<Order>& _ordersBuffer, std::atomic<bool>& _running, OrderBook& _orderBook, uint64_t numOrders,
    std::atomic<int64_t>& _usd_balance, std::atomic<int64_t>& _equity_balance)
    : orderBook(_orderBook),
      ordersBuffer(_ordersBuffer),
      running(_running),
      usd_balance(_usd_balance),
      equity_balance(_equity_balance)
{
    latencies.reserve(numOrders);
}

void Engine::run() {
    pin_thread_to_core(1);

    // Trades vector for multiple trades happening for the same order
    std::vector<Trade> trades;
    trades.reserve(256);
    // Best-effort drain of ordersBuffer
    while (running.load(std::memory_order_acquire) || !ordersBuffer.is_empty()) {
        // Pop orders from SPSC ring buffer, from gateway to engine
        Order order;
        if (!ordersBuffer.pop(order)) {
            // Treat KILL and CANCEL orders
            if (order.type == OrderType::KILL) {
                break;
            } else if (order.type == OrderType::CANCEL) {
                // Add CANCEL history node to history buffer if from user
                if (order.user_id == 1) {
                    Order* order_to_be_removed = orderBook.getOrderLookup()[order.id];
                    OrderHistory orderHistory(order_to_be_removed->id,
                        order_to_be_removed->user_id,
                        order_to_be_removed->timestamp,
                        order_to_be_removed->type,
                        order_to_be_removed->side,
                        order_to_be_removed->price,
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

            trades.clear();

            if (order.tif == TimeInForce::IOC) {
                // Just process (match order with book) and dismiss the remainder
                orderBook.processOrder(order, trades);
            } else if (order.tif == TimeInForce::FOK) {
                // Check if it can be filled entirely and process order
                if (orderBook.canFill(order)) {
                    orderBook.processOrder(order, trades);
                }
            } else {
                // Process order and add remainder to book
                uint32_t remaining_qty = orderBook.processOrder(order, trades);

                order.quantity = remaining_qty;
                if (remaining_qty > 0 && order.type != OrderType::MARKET) {
                    orderBook.addOrder(order);
                }
            }

            for (Trade &trade : trades) {
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

            uint64_t now = std::chrono::steady_clock::now().time_since_epoch().count();
            latencies.push_back(now - order.latency_timestamp);
        } else {
            continue;
        }
    }
}

void Engine::stop() {
    running = false;
    if (engineThread.joinable()) {
        engineThread.join();
    }
}

void Engine::printStats() {
    std::sort(latencies.begin(), latencies.end());

    double median = latencies[latencies.size() * 0.5];
    double p99 = latencies[latencies.size() * 0.99];
    double p99_9 = latencies[latencies.size() * 0.999];

    std::cout << "Median Latency:  " << median << " ns\n";
    std::cout << "99\% Latency:     " << p99 << " ns\n";
    std::cout << "99.9\% Latency:   " << p99_9 << " ns\n";

    std::cout << "USD BALANCE: " << usd_balance << '\n';
    std::cout << "EQUITY BALANCE: " << equity_balance << '\n';
}