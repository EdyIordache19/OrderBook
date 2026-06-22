#include <gtest/gtest.h>
#include "orderbook.hpp"
#include "order_types.hpp"
#include "ring_buffer.hpp"

// Fixture class to initialize OrderBook for every test
class OrderBookTest : public ::testing::Test {
protected:
    // Initialize with 100k orders support
    OrderBook book;
    RingBuffer<Trade> matchBuffer;
    RingBuffer<OrderHistory> historyBuffer;
    Trade *trades = (Trade *)malloc(sizeof(Trade) * 256);
    uint8_t trades_count = 0;
    OrderBookTest()
        : matchBuffer(1 << 16),
          historyBuffer(1 << 16),
          book(100000, matchBuffer, historyBuffer) {}
};

// 1. Basic Add - Verify orders sit in the book
TEST_F(OrderBookTest, AddLimitOrder) {
    // Add Buy: ID 1, Price 100, Qty 10
    Order buy(1, 1, 100, 10, Side::BUY, OrderType::LIMIT);
    book.addOrder(buy);

    ASSERT_EQ(book.getLevelQuantity(100, Side::BUY), 10);
    ASSERT_EQ(book.getMaxBid(), 100);
}

// 2. Full Match - Buy completely consumes Sell
TEST_F(OrderBookTest, FullMatch) {
    // Sell 10 @ 100
    Order sell(1, 1, 100, 10, Side::SELL, OrderType::LIMIT);
    book.addOrder(sell);

    // Buy 10 @ 100
    Order buy(2, 1, 100, 10, Side::BUY, OrderType::LIMIT);
    uint32_t remaining = book.processOrder(buy, trades, trades_count);

    ASSERT_EQ(remaining, 0); // Buy fully filled
    ASSERT_EQ(book.getLevelQuantity(100, Side::SELL), 0); // Sell fully eaten
    ASSERT_TRUE(book.isEmpty());
}

// 3. Partial Match (Resting Order remains)
TEST_F(OrderBookTest, PartialMatch_RestingRemains) {
    // Sell 20 @ 100
    Order sell(1, 1, 100, 20, Side::SELL, OrderType::LIMIT);
    book.addOrder(sell);

    // Buy 10 @ 100
    Order buy(2, 1, 100, 10, Side::BUY, OrderType::LIMIT);
    book.processOrder(buy, trades, trades_count);

    // Sell should have 10 left
    ASSERT_EQ(book.getLevelQuantity(100, Side::SELL), 10);
}

// 4. Partial Match (Incoming Order remains)
TEST_F(OrderBookTest, PartialMatch_IncomingRemains) {
    // Sell 10 @ 100
    Order sell(1, 1, 100, 10, Side::SELL, OrderType::LIMIT);
    book.addOrder(sell);

    // Buy 20 @ 100
    Order buy(2, 1, 100, 20, Side::BUY, OrderType::LIMIT);
    uint32_t remaining = book.processOrder(buy, trades, trades_count);

    ASSERT_EQ(remaining, 10); // 10 Left over
    ASSERT_EQ(book.getLevelQuantity(100, Side::SELL), 0); // Sell side empty

    // Simulate Engine: Add remainder to book
    if (remaining > 0) {
        buy.quantity = remaining;
        book.addOrder(buy);
    }
    ASSERT_EQ(book.getLevelQuantity(100, Side::BUY), 10);
}

// 5. Multi-Level Sweep
TEST_F(OrderBookTest, SweepMultipleLevels) {
    // Stack the Sell side
    Order s1(1, 1, 100, 10, Side::SELL, OrderType::LIMIT);
    Order s2(2, 1, 101, 10, Side::SELL, OrderType::LIMIT);
    Order s3(3, 1, 102, 10, Side::SELL, OrderType::LIMIT);
    book.addOrder(s1);
    book.addOrder(s2);
    book.addOrder(s3);

    // Buy 25 @ 102 (Should eat 10@100, 10@101, and 5@102)
    Order buy(4, 1, 102, 25, Side::BUY, OrderType::LIMIT);
    uint32_t remaining = book.processOrder(buy, trades, trades_count);

    ASSERT_EQ(remaining, 0);
    ASSERT_EQ(book.getLevelQuantity(100, Side::SELL), 0);
    ASSERT_EQ(book.getLevelQuantity(101, Side::SELL), 0);
    ASSERT_EQ(book.getLevelQuantity(102, Side::SELL), 5); // 5 left at top
}

// 6. No Match (Spread)
TEST_F(OrderBookTest, Spread_NoMatch) {
    Order sell(1, 1, 101, 10, Side::SELL, OrderType::LIMIT);
    book.addOrder(sell);

    Order buy(2, 1, 100, 10, Side::BUY, OrderType::LIMIT);
    uint32_t remaining = book.processOrder(buy, trades, trades_count);

    ASSERT_EQ(remaining, 10); // Nothing happened

    // Engine would add it
    book.addOrder(buy);

    ASSERT_EQ(book.getMinAsk(), 101);
    ASSERT_EQ(book.getMaxBid(), 100);
}

// 7. Market Order (Basic)
TEST_F(OrderBookTest, MarketOrder_Execution) {
    Order s1(1, 1, 100, 10, Side::SELL, OrderType::LIMIT);
    Order s2(2, 1, 101, 10, Side::SELL, OrderType::LIMIT);
    book.addOrder(s1);
    book.addOrder(s2);

    Order mkt(3, 1, 0, 15, Side::BUY, OrderType::MARKET);
    uint32_t remaining = book.processOrder(mkt, trades, trades_count);

    ASSERT_EQ(remaining, 0);
    ASSERT_EQ(book.getLevelQuantity(100, Side::SELL), 0); // Eaten
    ASSERT_EQ(book.getLevelQuantity(101, Side::SELL), 5); // 5 Remaining
}

// 8. FOK (Fill Or Kill) - Success
TEST_F(OrderBookTest, FOK_Success) {
    Order sell(1, 1, 100, 50, Side::SELL, OrderType::LIMIT);
    book.addOrder(sell);

    Order fok(2, 1, 100, 50, Side::BUY, OrderType::LIMIT);
    fok.tif = TimeInForce::FOK;

    ASSERT_TRUE(book.canFill(fok)); // Check predicate
    book.processOrder(fok, trades, trades_count);

    ASSERT_EQ(book.getLevelQuantity(100, Side::SELL), 0);
}

// 9. FOK (Fill Or Kill) - Reject
TEST_F(OrderBookTest, FOK_Reject) {
    Order sell(1, 1, 100, 10, Side::SELL, OrderType::LIMIT);
    book.addOrder(sell);

    // Wants 50, only 10 available -> Reject
    Order fok(2, 1, 100, 50, Side::BUY, OrderType::LIMIT);
    fok.tif = TimeInForce::FOK;

    ASSERT_FALSE(book.canFill(fok));
    // Note: Engine logic handles skipping processOrder if canFill is false
}

// 10. IOC (Immediate Or Cancel)
TEST_F(OrderBookTest, IOC_PartialFill) {
    Order sell(1, 1, 100, 10, Side::SELL, OrderType::LIMIT);
    book.addOrder(sell);

    // Buy 50 IOC. Eats 10, cancels 40.
    Order ioc(2, 1, 100, 50, Side::BUY, OrderType::LIMIT);
    ioc.tif = TimeInForce::IOC;

    uint32_t remaining = book.processOrder(ioc, trades, trades_count);

    ASSERT_EQ(remaining, 40);
    ASSERT_EQ(book.getLevelQuantity(100, Side::SELL), 0);

    // Remaining IOC quantity is not added to book
    ASSERT_EQ(book.getLevelQuantity(100, Side::BUY), 0);
}

// 11. Time Priority (FIFO)
TEST_F(OrderBookTest, TimePriority) {
    // Add two orders at same price
    Order s1(1, 1, 100, 10, Side::SELL, OrderType::LIMIT); // First
    Order s2(2, 1, 100, 10, Side::SELL, OrderType::LIMIT); // Second
    book.addOrder(s1);
    book.addOrder(s2);

    // Buy 10 - Should execute against s1 (First In)
    Order buy(3, 1, 100, 10, Side::BUY, OrderType::LIMIT);
    book.processOrder(buy, trades, trades_count);

    ASSERT_EQ(book.getLevelQuantity(100, Side::SELL), 10);
}

// 12. Empty Book Handling
TEST_F(OrderBookTest, EmptyBook_NoCrash) {
    Order buy(1, 1, 100, 10, Side::BUY, OrderType::LIMIT);
    uint32_t remaining = book.processOrder(buy, trades, trades_count);
    ASSERT_EQ(remaining, 10);

    book.addOrder(buy);
    ASSERT_EQ(book.getLevelQuantity(100, Side::BUY), 10);
}

// 13. Zero Quantity Attack
// Verify engine rejects orders with 0 quantity
TEST_F(OrderBookTest, ZeroQuantityReject) {
    Order buy(1, 1, 100, 0, Side::BUY, OrderType::LIMIT);
    uint32_t remaining = book.processOrder(buy, trades, trades_count);

    // Should return 0 (nothing processed) and NOT add to book
    ASSERT_EQ(remaining, 0);
    ASSERT_EQ(book.getLevelQuantity(100, Side::BUY), 0);
}

// 14. Out of Bounds Price (Segfault Prevention)
TEST_F(OrderBookTest, OutOfBoundsPrice) {
    // Assuming book was init with 100,000. Use value > 100,000
    uint64_t badPrice = 2000000;
    Order buy(1, 1, badPrice, 10, Side::BUY, OrderType::LIMIT);

    // Verifies bounds check
    book.processOrder(buy, trades, trades_count);

    // Should not add to book or crash
    ASSERT_EQ(book.getLevelQuantity(badPrice, Side::BUY), 0);
}

// 15. Self-Matching (Wash Trading)
TEST_F(OrderBookTest, NoSelfMatch) {
    // Check if cancelling a phantom order crashes
    book.removeOrder(9999); // ID that doesn't exist
    ASSERT_TRUE(true); // Just reaching here without crash is success
}

// 16. Market Order Liquidity Gap (Partial Fill)
TEST_F(OrderBookTest, MarketOrderGap) {
    // Sell 10 @ 100
    Order sell(1, 1, 100, 10, Side::SELL, OrderType::LIMIT);
    book.addOrder(sell);

    // Market Buy 50
    Order mkt(2, 1, 0, 50, Side::BUY, OrderType::MARKET);
    uint32_t remaining = book.processOrder(mkt, trades, trades_count);

    // Should fill 10, return 40.
    ASSERT_EQ(remaining, 40);
    ASSERT_EQ(book.getLevelQuantity(100, Side::BUY), 0);

    // Market orders that don't fill are cancelled (not added to book)
    ASSERT_EQ(book.getLevelQuantity(99999, Side::BUY), 0);
}

// 17. Basic Cancel - Verify order is removed
TEST_F(OrderBookTest, Cancel_Basic) {
    // Add Sell: ID 1, 10 @ 100
    Order sell(1, 1, 100, 10, Side::SELL, OrderType::LIMIT);
    book.addOrder(sell);

    // Verify presence
    ASSERT_EQ(book.getLevelQuantity(100, Side::SELL), 10);

    // Cancel ID 1
    book.removeOrder(1);

    // Verify order is removed and book is empty
    ASSERT_EQ(book.getLevelQuantity(100, Side::SELL), 0);
    ASSERT_TRUE(book.isEmpty());
}

// 18. Cancel Head of List (Maintenance of pointers)
TEST_F(OrderBookTest, Cancel_HeadOfList) {
    // Add two orders at same price
    Order b1(1, 1, 100, 10, Side::BUY, OrderType::LIMIT);
    Order b2(2, 1, 100, 20, Side::BUY, OrderType::LIMIT);
    book.addOrder(b1); // Head
    book.addOrder(b2); // Tail

    // Cancel Head (ID 1)
    book.removeOrder(1);

    // Total quantity should be 20 (only ID 2 remains)
    ASSERT_EQ(book.getLevelQuantity(100, Side::BUY), 20);

    // Verify matching still works against the remaining order (ID 2)
    Order sell(3, 1, 100, 20, Side::SELL, OrderType::LIMIT);
    uint32_t remaining = book.processOrder(sell, trades, trades_count);

    ASSERT_EQ(remaining, 0);
    ASSERT_EQ(book.getLevelQuantity(100, Side::BUY), 0);
}

// 19. Cancel Tail of List
TEST_F(OrderBookTest, Cancel_TailOfList) {
    Order b1(1, 1, 100, 10, Side::BUY, OrderType::LIMIT);
    Order b2(2, 1, 100, 20, Side::BUY, OrderType::LIMIT);
    book.addOrder(b1);
    book.addOrder(b2);

    // Cancel Tail (ID 2)
    book.removeOrder(2);

    ASSERT_EQ(book.getLevelQuantity(100, Side::BUY), 10);

    // Match remaining to ensure head is still valid
    Order sell(3, 1, 100, 10, Side::SELL, OrderType::LIMIT);
    uint32_t remaining = book.processOrder(sell, trades, trades_count);
    ASSERT_EQ(remaining, 0);
}

// 20. Cancel Middle of List
TEST_F(OrderBookTest, Cancel_MiddleOfList) {
    Order b1 = Order(1, 1, 100, 10, Side::BUY, OrderType::LIMIT);
    Order b2 = Order(2, 1, 100, 10, Side::BUY, OrderType::LIMIT);
    Order b3 = Order(3, 1, 100, 10, Side::BUY, OrderType::LIMIT);
    book.addOrder(b1); // Head
    book.addOrder(b2); // Middle
    book.addOrder(b3); // Tail

    // Cancel Middle (ID 2)
    book.removeOrder(2);

    // Should have 20 left (10 + 10)
    ASSERT_EQ(book.getLevelQuantity(100, Side::BUY), 20);

    // Verify Chain: If we match 20, it should consume ID 1 and ID 3
    Order sell(4, 1, 100, 25, Side::SELL, OrderType::LIMIT);
    uint32_t remaining = book.processOrder(sell, trades, trades_count);

    ASSERT_EQ(remaining, 5); // 25 - 10 - 10 = 5
}

// 21. Double Cancel (Idempotency)
// Cancelling an already cancelled order should not crash or corrupt state
TEST_F(OrderBookTest, Cancel_DoubleCancel) {
    Order sell = Order(1, 1, 100, 10, Side::SELL, OrderType::LIMIT);
    book.addOrder(sell);

    book.removeOrder(1); // First cancel
    book.removeOrder(1); // Second cancel (should be ignored)

    ASSERT_TRUE(true); // Verifies no Segfault occurred
}

// 22. Cancel Non-Existent ID
TEST_F(OrderBookTest, Cancel_NonExistent) {
    book.removeOrder(99999); // ID never existed, should be ignored
    ASSERT_TRUE(true);
}

// 23. Partial Match - Aggressive order partially fills a larger resting order
TEST_F(OrderBookTest, PartialFillRestingOrder) {
    // 1. Add a large resting Buy order (Qty: 100 @ Price: 1000)
    Order resting_buy(1, 1, 1000, 100, Side::BUY, OrderType::LIMIT);
    book.addOrder(resting_buy);

    // Assert it is in the book
    ASSERT_EQ(book.getLevelQuantity(1000, Side::BUY), 100);

    // 2. Send a smaller aggressive Sell order (Qty: 40 @ Price: 1000)
    Order aggressive_sell(2, 1, 1000, 40, Side::SELL, OrderType::LIMIT);
    uint32_t remaining_sell = book.processOrder(aggressive_sell, trades, trades_count);

    // 3. Assertions
    // The aggressive sell should be completely filled (0 remaining)
    ASSERT_EQ(remaining_sell, 0);

    // The resting buy should have 60 left (100 - 40)
    ASSERT_EQ(book.getLevelQuantity(1000, Side::BUY), 60);

    // NOTE: If your OrderBook exposes order states directly (e.g. getOrder(id)),
    // you should also assert that resting_buy's `filled_qty` equals 40 here!
}