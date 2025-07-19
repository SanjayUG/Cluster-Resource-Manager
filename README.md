## CLUSTER RESOURCE MANAGER

This project implements a **Cluster Resource Manager** in C++ that manages task allocation across a distributed set of nodes. It simulates a simplified real-world cluster system, including dynamic memory-aware task scheduling, load handling, health monitoring, and failover recovery.

---

## 🧠 Project Objectives

- **Efficient Load Balancing**
- **Dynamic Memory-Aware Task Allocation** (Each task specifies its memory requirement; manager schedules tasks dynamically based on node memory)
- **Health Monitoring** (Periodic node status checks)
- **Failover & Recovery** (Automatic task reallocation)
- **Performance Logging**
- Demonstrates core **Linux OS concepts** like:
  - Memory Management
  - Virtual Memory
  - Processes
  - Scheduling
  - Multi-threading
  - Inter-process Communication
  - Socket Programming
  - File System

---

## ⚙️ Architecture

- **Manager (manager.cpp)**  
  Accepts node and client connections, tracks node status, assigns tasks (dynamic memory-aware), monitors node health, handles failures.

- **Node Agent (node_agent.cpp)**  
  Connects to the manager, registers itself (with available memory), accepts and executes tasks, sends heartbeat pings.

- **Client (client.cpp)**  
  Sends a batch of tasks to the manager for processing. Each task has a random memory requirement (6-126 MB).

---

## 📋 Custom Algorithms Used

> This project uses a **Dynamic Memory-Aware Greedy + FCFS** scheduling strategy with health monitoring, reactive failover, and thread-based concurrency.

- **Dynamic Memory-Aware Scheduling:** Each task specifies its memory requirement; nodes are only assigned tasks if they have enough memory. The manager dynamically adapts as tasks complete and memory is freed.
- **FCFS Task Assignment:** Tasks assigned in order of arrival.
- **Greedy Dispatch:** Tasks are assigned immediately to the first node with enough available memory.
- **Health Monitoring:** Manager checks node status every 10 seconds and reallocates tasks from failed nodes.
- **Failover:** When a node crashes or disconnects, its tasks are reassigned.
- **Multithreading:** Each connection is handled in a separate thread.

---

## 🛠️ Build & Run

### Prerequisites
- g++ (C++17)
- make
- bash (for build.sh)

### Build
```sh
bash build.sh
```

### Run
1. Start the manager:
   ```sh
   ./build/manager
   ```
2. Start one or more node agents (in separate terminals):
   ```sh
   ./build/node_agent node1 127.0.0.1 5000 9001
   ./build/node_agent node2 127.0.0.1 5000 9002
   ...
   ```
3. Submit tasks from the client:
   ```sh
   ./build/client 127.0.0.1 5000 10
   ```

---

## Notes
- No CMake is used; the project is built with Makefile and build.sh.
- Tasks are memory-aware and randomly sized (6-126 MB).
- Health monitoring and failover are automatic.
- **DAG scheduling is not implemented; all tasks are independent.**
