#include "engine.hpp"
#include "main.hpp"

Engine::Engine(RingBuffer<Order>& _ordersBuffer, std::atomic<bool>& _running, OrderBook& _orderBook, uint64_t numOrders,
    std::atomic<int64_t>& _usd_balance, std::atomic<int64_t>& _equity_balance)
    : ordersBuffer(_ordersBuffer),
      running(_running),
      orderBook(_orderBook),
      usd_balance(_usd_balance),
      equity_balance(_equity_balance)
{
    latencies.reserve(numOrders);
}

void Engine::run() {
    pin_thread_to_core(1);

    std::vector<Trade> trades;
    trades.reserve(256);
    while (running.load(std::memory_order_acquire) || !ordersBuffer.is_empty()) {
        Order order;
        if (!ordersBuffer.pop(order)) {
            if (order.type == OrderType::KILL) {
                orderBook.addOrder(order);
                break;
            } else if (order.type == OrderType::CANCEL) {
                orderBook.removeOrder(order.id);
                continue;
            }

            if (order.user_id == 1) {
                if (order.side == Side::BUY) {
                    uint64_t cost = order.price * order.quantity;
                    if (usd_balance < cost) continue;
                } else {
                    if (equity_balance < order.quantity) continue;
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
                uint64_t trade_value = trade.price * trade.quantity;

                if (trade.maker_user_id == 1) {
                    if (order.side == Side::BUY) {
                        usd_balance -= trade_value;
                        equity_balance += trade.quantity;
                    } else {
                        usd_balance += trade_value;
                        equity_balance -= trade.quantity;
                    }
                }

                if (trade.taker_user_id == 1) {
                    if (order.side == Side::BUY) {
                        usd_balance += trade_value;
                        equity_balance -= trade.quantity;
                    } else {
                        usd_balance -= trade_value;
                        equity_balance += trade.quantity;
                    }
                }

                orderBook.matchBuffer.push(trade);
            }

            uint64_t now = std::chrono::steady_clock::now().time_since_epoch().count();
            latencies.push_back(now - order.timestamp);
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
    std::cout << "99% Latency:     " << p99 << " ns\n";
    std::cout << "99.9% Latency:   " << p99_9 << " ns\n";

    std::cout << "USD BALANCE: " << usd_balance << '\n';
    std::cout << "EQUITY BALANCE: " << equity_balance << '\n';
}