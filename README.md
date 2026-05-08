# Orderbook Project

Welcome to the Orderbook project! This repository contains a system that processes orders and a frontend for visualizing them.

## Overview

The system is designed to handle trading operations efficiently. It involves a backend component built in C++ and a frontend built using JavaScript/React. The exact messaging protocols, data flows, and state management mechanisms are abstracted to adapt to various use cases.

## Build Instructions

### Backend

To build the backend components, you will need CMake and a modern C++ compiler.

```bash
mkdir build
cd build
cmake ..
make
```

### Frontend

The frontend requires Node.js.

```bash
cd frontend
npm install
npm run dev
```

## Running the Application

After building, you can start the components as needed. Further configuration might be necessary depending on the runtime environment and network topology.
