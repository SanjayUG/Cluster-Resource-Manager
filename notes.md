**Steps to Use:**

1. Open Ubuntu (WSL Terminal)

*Terminal 1:*

2. cd ClusterResourceManager
3. bash build.sh
   
   Cleans and builds the project using the Makefile. Binaries are placed in the build directory.

5. ./build/manager
6. Open multiple new terminal windows for each node agent (Ex:3 nodes)

*Terminals 2,3,4 respectively:*

7. cd ClusterResourceManager
8. ./build/node_agent node1 127.0.0.1 5000 9001
   
   ./build/node_agent node2 127.0.0.1 5000 9002
   
   ./build/node_agent node3 127.0.0.1 5000 9003
   
   (Each node will also log to its own file)

*Terminal 5:*

9. ./build/client 127.0.0.1 5000 10
    
   This will submit 10 tasks. Each task will have a random memory requirement (6-126 MB). The manager will schedule and assign tasks to nodes based on available memory.

11. To Demonstrate Health Monitoring & Failover:
    - Go to one of the Node Agent terminals (e.g., node2) and press Ctrl+C to terminate the process.

    - In the Manager Terminal, you will see a log message indicating that the connection to node2 was lost (e.g., WARN: Node node2 is DOWN). It will then automatically detect the failure, mark node2 as inactive, and reassign any incomplete tasks to the next available healthy node (e.g., node1 or node3). If no nodes are available, the manager will log an error.

12. Cleanup:
    - Press Ctrl+C in all terminals.
    - Run bash build.sh or make clean to remove builds and logs.

**Note:**
- No CMake is used; the project is built with Makefile and build.sh.
- Tasks are memory-aware and randomly sized (6-126 MB).
- Health monitoring and failover are automatic.

