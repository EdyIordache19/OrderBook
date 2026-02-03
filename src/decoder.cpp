#include "decoder.hpp"

bool Decoder::decode(const char* buffer, size_t len, Order& order, uint64_t maxPrice) {
    if (len < sizeof(WireMessage)) {
        return false;
    }

    const WireMessage *message = reinterpret_cast<const WireMessage *>(buffer);

    // Copy to order
    order.id = message->id;
    order.price = message->price;
    order.quantity = message->quantity;

    if (message->side == 'B') order.side = Side::BUY;
    else order.side = Side::SELL;

    if (message->type == 0) order.type = OrderType::LIMIT;
    else if (message->type == 1) {
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
    }

    order.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

    return true;
}
