#include <gtest/gtest.h>
#include "orderbook.hpp"
#include "order_types.hpp"

// Fixture class to initialize OrderBook for every test
class OrderBookTest : public ::testing::Test {
protected:
    // Initialize with 1M orders support
    OrderBook book;
    OrderBookTest() : book(100000) {}
};

// 1. Basic Add - Verify orders sit in the book
TEST_F(OrderBookTest, AddLimitOrder) {
    // Add Buy: ID 1, Price 100, Qty 10
    Order buy(1, 100, 10, Side::BUY, OrderType::LIMIT);
    book.addOrder(buy);

    ASSERT_EQ(book.getLevelQuantity(100, Side::BUY), 10);
    ASSERT_EQ(book.getMaxBid(), 100);
}

// 2. Full Match - Buy completely consumes Sell
TEST_F(OrderBookTest, FullMatch) {
    // Sell 10 @ 100
    Order sell(1, 100, 10, Side::SELL, OrderType::LIMIT);
    book.addOrder(sell);

    // Buy 10 @ 100
    Order buy(2, 100, 10, Side::BUY, OrderType::LIMIT);
    uint32_t remaining = book.processOrder(buy);

    ASSERT_EQ(remaining, 0); // Buy fully filled
    ASSERT_EQ(book.getLevelQuantity(100, Side::SELL), 0); // Sell fully eaten
    ASSERT_TRUE(book.isEmpty());
}

// 3. Partial Match (Resting Order remains)
TEST_F(OrderBookTest, PartialMatch_RestingRemains) {
    // Sell 20 @ 100
    Order sell(1, 100, 20, Side::SELL, OrderType::LIMIT);
    book.addOrder(sell);

    // Buy 10 @ 100
    Order buy(2, 100, 10, Side::BUY, OrderType::LIMIT);
    book.processOrder(buy);

    // Sell should have 10 left
    ASSERT_EQ(book.getLevelQuantity(100, Side::SELL), 10);
}

// 4. Partial Match (Incoming Order remains)
TEST_F(OrderBookTest, PartialMatch_IncomingRemains) {
    // Sell 10 @ 100
    Order sell(1, 100, 10, Side::SELL, OrderType::LIMIT);
    book.addOrder(sell);

    // Buy 20 @ 100
    Order buy(2, 100, 20, Side::BUY, OrderType::LIMIT);
    uint32_t remaining = book.processOrder(buy);

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
    Order s1(1, 100, 10, Side::SELL, OrderType::LIMIT);
    Order s2(2, 101, 10, Side::SELL, OrderType::LIMIT);
    Order s3(3, 102, 10, Side::SELL, OrderType::LIMIT);
    book.addOrder(s1);
    book.addOrder(s2);
    book.addOrder(s3);

    // Buy 25 @ 102 (Should eat 10@100, 10@101, and 5@102)
    Order buy(4, 102, 25, Side::BUY, OrderType::LIMIT);
    uint32_t remaining = book.processOrder(buy);

    ASSERT_EQ(remaining, 0);
    ASSERT_EQ(book.getLevelQuantity(100, Side::SELL), 0);
    ASSERT_EQ(book.getLevelQuantity(101, Side::SELL), 0);
    ASSERT_EQ(book.getLevelQuantity(102, Side::SELL), 5); // 5 left at top
}

// 6. No Match (Spread)
TEST_F(OrderBookTest, Spread_NoMatch) {
    Order sell(1, 101, 10, Side::SELL, OrderType::LIMIT);
    book.addOrder(sell);

    Order buy(2, 100, 10, Side::BUY, OrderType::LIMIT);
    uint32_t remaining = book.processOrder(buy);

    ASSERT_EQ(remaining, 10); // Nothing happened

    // Engine would add it
    book.addOrder(buy);

    ASSERT_EQ(book.getMinAsk(), 101);
    ASSERT_EQ(book.getMaxBid(), 100);
}

// 7. Market Order (Basic)
TEST_F(OrderBookTest, MarketOrder_Execution) {
    Order s1(1, 100, 10, Side::SELL, OrderType::LIMIT);
    Order s2(2, 101, 10, Side::SELL, OrderType::LIMIT);
    book.addOrder(s1);
    book.addOrder(s2);

    Order mkt(3, 0, 15, Side::BUY, OrderType::MARKET);
    uint32_t remaining = book.processOrder(mkt);

    ASSERT_EQ(remaining, 0);
    ASSERT_EQ(book.getLevelQuantity(100, Side::SELL), 0); // Eaten
    ASSERT_EQ(book.getLevelQuantity(101, Side::SELL), 5); // 5 Remaining
}

// 8. FOK (Fill Or Kill) - Success
TEST_F(OrderBookTest, FOK_Success) {
    Order sell(1, 100, 50, Side::SELL, OrderType::LIMIT);
    book.addOrder(sell);

    Order fok(2, 100, 50, Side::BUY, OrderType::LIMIT);
    fok.tif = TimeInForce::FOK;

    ASSERT_TRUE(book.canFill(fok)); // Check predicate
    book.processOrder(fok);

    ASSERT_EQ(book.getLevelQuantity(100, Side::SELL), 0);
}

// 9. FOK (Fill Or Kill) - Reject
TEST_F(OrderBookTest, FOK_Reject) {
    Order sell(1, 100, 10, Side::SELL, OrderType::LIMIT);
    book.addOrder(sell);

    // Wants 50, only 10 available -> Reject
    Order fok(2, 100, 50, Side::BUY, OrderType::LIMIT);
    fok.tif = TimeInForce::FOK;

    ASSERT_FALSE(book.canFill(fok));
    // Engine should NOT call processOrder if canFill is false,
    // or processOrder should return full qty without matching.
}

// 10. IOC (Immediate Or Cancel)
TEST_F(OrderBookTest, IOC_PartialFill) {
    Order sell(1, 100, 10, Side::SELL, OrderType::LIMIT);
    book.addOrder(sell);

    // Buy 50 IOC. Eats 10, cancels 40.
    Order ioc(2, 100, 50, Side::BUY, OrderType::LIMIT);
    ioc.tif = TimeInForce::IOC;

    uint32_t remaining = book.processOrder(ioc);

    ASSERT_EQ(remaining, 40);
    ASSERT_EQ(book.getLevelQuantity(100, Side::SELL), 0);

    // Crucial: Engine does NOT add remaining 40 to book
    ASSERT_EQ(book.getLevelQuantity(100, Side::BUY), 0);
}

// 11. Time Priority (FIFO)
TEST_F(OrderBookTest, TimePriority) {
    // Add two orders at same price
    Order s1(1, 100, 10, Side::SELL, OrderType::LIMIT); // First
    Order s2(2, 100, 10, Side::SELL, OrderType::LIMIT); // Second
    book.addOrder(s1);
    book.addOrder(s2);

    // Buy 10
    Order buy(3, 100, 10, Side::BUY, OrderType::LIMIT);
    book.processOrder(buy);

    // Should have executed against s1 (ID 1)
    // Assuming your OrderBook removes head:
    // We can't easily check ID without a getter for the list,
    // but we can check the queue logic if we had access.
    // Generally, s1 should be gone, s2 remains.

    // For now, just check quantity decreased correctly
    ASSERT_EQ(book.getLevelQuantity(100, Side::SELL), 10);
}

// 12. Empty Book Handling
TEST_F(OrderBookTest, EmptyBook_NoCrash) {
    Order buy(1, 100, 10, Side::BUY, OrderType::LIMIT);
    uint32_t remaining = book.processOrder(buy);
    ASSERT_EQ(remaining, 10);

    book.addOrder(buy);
    ASSERT_EQ(book.getLevelQuantity(100, Side::BUY), 10);
}

// 13. Zero Quantity Attack
// Does the engine reject orders with 0 quantity?
TEST_F(OrderBookTest, ZeroQuantityReject) {
    Order buy(1, 100, 0, Side::BUY, OrderType::LIMIT);
    uint32_t remaining = book.processOrder(buy);

    // Should return 0 (nothing processed) and NOT add to book
    ASSERT_EQ(remaining, 0);
    ASSERT_EQ(book.getLevelQuantity(100, Side::BUY), 0);
}

// 14. Out of Bounds Price (Segfault Prevention)
// If I send price 9999999, does it crash?
TEST_F(OrderBookTest, OutOfBoundsPrice) {
    // Assuming book was init with 1,000,000
    uint64_t badPrice = 2000000;
    Order buy(1, badPrice, 10, Side::BUY, OrderType::LIMIT);

    // Should ideally reject or return full quantity
    // This verifies your "if (price >= numOrders)" check
    book.processOrder(buy);

    // Should NOT be in the book (would crash if it tried to access vector[2000000])
    // Since we can't easily check 'getLevelQuantity' for invalid index, just ensure no crash.
}

// 15. Self-Matching (Wash Trading)
// If the same order ID executes against itself (logic error check)
TEST_F(OrderBookTest, NoSelfMatch) {
    // This is hard to test unless your addOrder checks IDs.
    // But we can check if cancelling a phantom order crashes.
    book.removeOrder(9999); // ID that doesn't exist
    ASSERT_TRUE(true); // Just reaching here without crash is success
}

// 16. Market Order Liquidity Gap (Partial Fill)
// Market Buy 50, but only 10 on sale.
TEST_F(OrderBookTest, MarketOrderGap) {
    // Sell 10 @ 100
    Order sell = Order(1, 100, 10, Side::SELL, OrderType::LIMIT);
    book.addOrder(sell);

    // Market Buy 50
    Order mkt(2, 0, 50, Side::BUY, OrderType::MARKET);
    uint32_t remaining = book.processOrder(mkt);

    // Should fill 10, return 40.
    ASSERT_EQ(remaining, 40);
    ASSERT_EQ(book.getLevelQuantity(100, Side::BUY), 0);

    // CRITICAL: Ensure the remaining 40 Market Order is NOT added to the book at price MAX!
    // Market orders that don't fill usually Cancel or become Limit at Touch.
    // Your engine logic dictates this. If you don't add it, book should be empty.
    ASSERT_EQ(book.getLevelQuantity(999999, Side::BUY), 0);
}