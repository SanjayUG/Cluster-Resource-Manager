This project implements a **Cluster Resource Manager** in C++ that manages task allocation across a distributed set of nodes. It simulates a simplified real-world cluster system, including task scheduling, health monitoring, load handling, and failover recovery.

---

## 🧠 Project Objectives

- **Efficient Load Balancing**
- **Task Allocation**
- **Failover & Recovery**
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
  Accepts node and client connections, tracks node status, assigns tasks, handles failures.

- **Node Agent (node_agent.cpp)**  
  Connects to the manager, registers itself, accepts and executes tasks, sends heartbeat pings.

- **Client (client.cpp)**  
  Sends a batch of tasks to the manager for processing.

---

## 📋 Custom Algorithms Used

> This project uses a **Greedy + FCFS** scheduling strategy with reactive failover and thread-based concurrency.

- **FCFS Task Assignment:** Tasks assigned in order of arrival.
- **Greedy Dispatch:** Tasks are assigned immediately to the first available node.
- **Failover:** When a node crashes or disconnects, its tasks are reassigned.
- **Multithreading:** Each connection is handled in a separate thread.

---
