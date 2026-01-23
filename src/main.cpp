#include "main.hpp"
#include "orderbook.hpp"
#include "orders_generator.hpp"
#include "ring_buffer.hpp"
#include "gateway.hpp"
#include "engine.hpp"

void pin_thread_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "You need to parse 2 files\n";
        return 1;
    }

    OrderBook orderBook;
    RingBuffer buffer(BUFFER_SIZE);

    std::atomic<bool> running = true;

    Engine engine(buffer, running, orderBook);
    engine.start();

    Gateway gateway;
    gateway.run(buffer, running);

    engine.stop();
    engine.printStats();

    // Print current orders
    orderBook.printOrders(argv[1]);

    return 0;
}