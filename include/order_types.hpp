#pragma once

#include <cstdint>

enum OrderType : uint8_t {
    LIMIT = 0,
    MARKET = 1,
    CANCEL = 2,
    KILL = 99
};

enum TimeInForce : uint8_t {
    GTC = 0, // Good 'till Cancel
    IOC = 1, // Immediate or Cancel
    FOK = 2 // Fill or Kill
};

enum Side : uint8_t {
    BUY = 0,
    SELL = 1
};

enum MsgType : uint8_t {
    MSG_CANDLE = 1,
    MSG_BOOK_SNAPSHOT = 2,
    MSG_TRADE = 3,
    MSG_ACCOUNT_INFO = 4
};

struct __attribute__((packed)) PriceLevel {
    uint64_t price;
    uint32_t quantity;

    PriceLevel(uint64_t _price, uint32_t _quantity) :
        price(_price),
        quantity(_quantity)
        { }

    PriceLevel() { }
};

struct __attribute__((packed)) BookSnapshot {
    uint8_t num_bids;
    uint8_t num_asks;
    PriceLevel bids[10];
    PriceLevel asks[10];
};

struct __attribute__((packed)) Candle {
    uint64_t open = 0;
    uint64_t high = 0;
    uint64_t low = -1;
    uint64_t close = 0;
    uint32_t volume = 0;
    bool is_active = false;
    uint64_t simulated_time;
};

struct __attribute__((packed)) Trade {
    uint64_t maker_id;
    uint64_t taker_id;
    uint64_t maker_user_id;
    uint64_t taker_user_id;

    uint64_t price;
    uint32_t quantity;
    uint64_t timestamp;

    Trade() { }

    Trade(uint64_t _maker_id, uint64_t _taker_id, uint64_t _maker_user_id, uint64_t _taker_user_id, uint64_t _price, uint32_t _quantity, uint64_t _timestamp) :
        maker_id(_maker_id),
        taker_id(_taker_id),
        maker_user_id(_maker_user_id),
        taker_user_id(_taker_user_id),
        price(_price),
        quantity(_quantity),
        timestamp(_timestamp)
        { }
};

struct alignas(64) Order {
    uint64_t id;
    uint32_t user_id;
    uint64_t price;
    uint32_t quantity;
    uint64_t timestamp;

    Side side;
    OrderType type;
    TimeInForce tif;

    Order *next = nullptr;
    Order *prev = nullptr;

    Order() { }

    Order(uint64_t _id, uint32_t _user_id, uint64_t _price, uint32_t _quantity, Side _side, OrderType _type) :
        id(_id),
        user_id(_user_id),
        price(_price),
        quantity(_quantity),
        side(_side),
        type(_type)
            { }
};

struct Level {
    // First order (highest priority)
    Order *head = nullptr;

    // Last order (lowest priority)
    Order *tail = nullptr;
};

struct __attribute__((packed)) AccountInfo {
    int64_t usd;
    int64_t equity;
};

struct __attribute__((packed)) MsgHdr {
    MsgType type;
    uint16_t size;
};

struct __attribute__((packed)) CandlePacket {
    MsgHdr header;
    Candle payload;
};

struct __attribute__((packed)) SnapshotPacket {
    MsgHdr header;
    BookSnapshot payload;
};

struct __attribute__((packed)) TradePacket {
    MsgHdr header;
    Trade payload;
};

struct __attribute__((packed)) AccountPacket {
    MsgHdr header;
    AccountInfo payload;
};
