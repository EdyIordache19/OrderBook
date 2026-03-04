#!/bin/bash

# Prompt for the initial number of orders
echo "How many orders do you want to simulate?"
read num_orders

# 1. Exit immediately if the build fails
set -e

echo "Building C++ project..."
cd build
make -j$(nproc)
cd ..

# Disable exit-on-error so our cleanup trap works properly later
set +e

# 2. Process Tree Killer
# This function recursively finds all child processes of a given PID and kills them
kill_tree() {
    local _pid=$1
    # Find all children of this process
    for _child in $(pgrep -P $_pid 2>/dev/null); do
        kill_tree $_child
    done
    # Kill the parent process forcefully
    kill -9 $_pid 2>/dev/null
}

# 3. Cleanup Trap
cleanup() {
    echo -e "\nShutting down all processes..."

    # Unregister the trap to prevent infinite recursion
    trap - SIGINT SIGTERM EXIT

    # Kill all background jobs and their children
    for pid in $(jobs -p); do
        kill_tree $pid
    done

    rm -f .last_time.txt .last_price.txt 2>/dev/null

    exit 0
}

# Bind the safe cleanup function to the signals
trap cleanup SIGINT SIGTERM EXIT

echo "Starting Python WebSocket publisher..."
cd apps
python3 trades_viewer.py &
cd ..

echo "Starting React frontend..."
cd frontend
npm run dev &
cd ..

# Wait for the Python and React servers to bind to their ports
echo "Waiting for servers to initialize..."
sleep 1

echo "Press ENTER when finished opening the website!"
read

# Navigate to the build directory and stay there for the engine/client executions
cd build

# Function to run the C++ components
run_simulation() {
    local orders=$1
    echo -e "\nStarting C++ Orderbook matching engine..."
    ./orderbook -o ../out.txt -n $orders &

    # Capture the Process ID of the orderbook so we know when it dies
    ORDERBOOK_PID=$!

    # Wait a fraction of a second for the orderbook to bind its UDP sockets
    sleep 1

    echo "Starting C++ Client to pump $orders orders with..."
    ./client -n $orders

    echo "Client finished dispatching orders."

    # Wait for the orderbook process to cleanly finish processing and exit
    wait $ORDERBOOK_PID 2>/dev/null
}

# Run the simulation for the first time
run_simulation $num_orders

# Interactive Loop
while true
do
    echo -e "\n----------------------------------------"
    echo "What do you want to do now?"
    echo "1. Run again ($num_orders orders)"
    echo "2. Run again (Different number of orders)"
    echo "3. Exit"
    read -p "Enter choice [1-3]: " choice

    if [ "$choice" -eq 1 ]; then
        run_simulation $num_orders

    elif [ "$choice" -eq 2 ]; then
        echo "How many orders do you want to simulate?"
        read num_orders
        run_simulation $num_orders

    elif [ "$choice" -eq 3 ]; then
        echo "Exiting..."
        cleanup

    else
        echo "Invalid choice. Please enter 1, 2, or 3."
    fi
done