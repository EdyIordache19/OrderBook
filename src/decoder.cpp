#include "decoder.hpp"

#include <chrono>

bool Decoder::decode(const char* buffer, size_t len, Order& order, uint64_t maxPrice) {
    // Buffer should contain at least sizeof(WireMessage) bytes
    if (len < sizeof(WireMessage)) {
        return false;
    }

    const WireMessage *message = reinterpret_cast<const WireMessage *>(buffer);

    // Copy to order
    order.id = message->id;
    order.user_id = message->user_id;
    order.price = message->price;
    order.quantity = message->quantity;

    // Side accepts either 'B' or 0 as BUY, else SELL
    if (message->side == 'B' || message->side == 0) order.side = Side::BUY;
    else order.side = Side::SELL;

    if (message->type == 0) order.type = OrderType::LIMIT;
    else if (message->type == 1) {
        // Normalize MARKET into aggressive LIMIT so engine only needs one matching path
        //  - BUY => maxPrice - 1
        //  - SELL => 0
        order.type = OrderType::MARKET;
        if (order.side == Side::BUY) {
            order.price = maxPrice - 1;
        } else {
            order.price = 0;
        }
    } else if (message->type == 2) {
        order.type = OrderType::CANCEL;
    } else {
        order.type = OrderType::KILL;
    }

    switch (message->tif) {
        case 0:
            order.tif = TimeInForce::GTC;
            break;
        case 1:
            order.tif = TimeInForce::IOC;
            break;
        case 2:
            order.tif = TimeInForce::FOK;
            break;
        default:
            return false;
    }

    // Monotonic engine timestamp for metrics
    order.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

    return true;
}
