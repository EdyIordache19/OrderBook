/**
 * @file order_types.hpp
 * @brief Header file for different structures, organizing orders and trades
 * Price/Qty are represented as integers, instead of doubles, for determinism and speed
 */

#pragma once

#include <cstdint>

/**
 * @brief Enums for different attributes
 * Numeric values are for network protocol stability, don't change without updating all consumers
 */
enum OrderType : uint8_t {
    LIMIT = 0,
    MARKET = 1,
    CANCEL = 2,

    // Kill order type sent from client to kill the engine
    KILL = 99
};

enum TimeInForce : uint8_t {
    // Resting order
    GTC = 0, // Good 'till Cancel

    // The remainder of the order not filled gets canceled
    IOC = 1, // Immediate or Cancel

    // Cancel if not filled fully
    FOK = 2 // Fill or Kill
};

enum Side : uint8_t {
    BUY = 0,
    SELL = 1
};

// Used for defining type of messages sent through network, in MsgHdr
enum MsgType : uint8_t {
    MSG_CANDLE = 1,
    MSG_BOOK_SNAPSHOT = 2,
    MSG_TRADE = 3,
    MSG_ACCOUNT_INFO = 4,
    MSG_OPEN_ORDERS = 5
};

/**
 * @brief In memory for engine logic
 */

struct alignas(64) Order {
    uint64_t id;
    uint32_t user_id;
    uint64_t price;
    uint32_t quantity;
    uint32_t initial_quantity = 0;
    uint64_t timestamp;

    Side side;
    OrderType type;
    TimeInForce tif;

    // next/prev only valid when resting on the orderbook
    Order *next = nullptr;
    Order *prev = nullptr;

    Order() { }

    Order(uint64_t _id, uint32_t _user_id, uint64_t _price, uint32_t _quantity, Side _side, OrderType _type) :
        id(_id),
        user_id(_user_id),
        price(_price),
        quantity(_quantity),
        initial_quantity(_quantity),
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



/**
 * @brief Wire format / Network payload
 * __attribute__((packed)) chosen for stability on the wire
 */
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

    // 10 Price levels for UI
    PriceLevel bids[10];
    PriceLevel asks[10];
};

struct __attribute__((packed)) Candle {
    uint64_t open = 0;
    uint64_t high = 0;

    // low initialized to UINT64_MAX, as first trade always sets it
    uint64_t low = -1;
    uint64_t close = 0;
    uint32_t volume = 0;

    // is_active field for keeping track if current candle is open or not
    bool is_active = false;
    uint64_t simulated_time;
};

struct __attribute__((packed)) Trade {
    // The ids of the maker/taker orders
    uint64_t maker_id;
    uint64_t taker_id;

    // The ids of the maker/taker users making the orders
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

struct __attribute__((packed)) AccountInfo {
    int64_t usd;
    int64_t equity;
};

struct __attribute__((packed)) OpenOrderRow {
    uint64_t id;
    uint64_t timestamp;
    OrderType type;
    Side side;
    uint64_t price;
    uint32_t quantity;
    uint8_t filled_pct;
};

struct __attribute__((packed)) OpenOrders {
    uint8_t count;
    OpenOrderRow open_orders[10];
};

struct __attribute__((packed)) MsgHdr {
    MsgType type;

    // Size of payload of packet
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

struct __attribute__((packed)) OpenOrdersPacket {
    MsgHdr header;
    OpenOrders payload;
};

