// ===== manager.cpp =====
#include <arpa/inet.h>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <sstream>
#include <streambuf>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <queue>
#include <vector>
#include <atomic>
#include <ctime>
#include <set>

std::mutex node_mutex;
std::mutex task_mutex;
std::atomic<bool> running{true};

struct NodeInfo {
    std::string id;
    std::string ip;
    int port;
    int sockfd;
    int available_memory = 0; // in MB
};

enum class TaskStatus { QUEUED, ASSIGNED, COMPLETED };

struct TaskEntry {
    std::string task;
    TaskStatus status;
    std::string assigned_node;
    int memory_required; // in MB
    std::vector<std::string> dependencies; // task IDs this task depends on
};

std::map<std::string, NodeInfo> nodes;
std::queue<std::string> task_queue;
std::map<std::string, TaskEntry> tasks;  // Task -> Entry

// Custom streambuf that duplicates output to two streambufs
class TeeBuf : public std::streambuf {
    std::streambuf* sb1;
    std::streambuf* sb2;

public:
    TeeBuf(std::streambuf* buf1, std::streambuf* buf2) : sb1(buf1), sb2(buf2) {}

protected:
    virtual int overflow(int c) override {
        if (c == EOF) return !EOF;
        if (sb1->sputc(c) == EOF) return EOF;
        if (sb2->sputc(c) == EOF) return EOF;
        return c;
    }

    virtual int sync() override {
        int const r1 = sb1->pubsync();
        int const r2 = sb2->pubsync();
        return r1 == 0 && r2 == 0 ? 0 : -1;
    }
};

void log(const std::string &level, const std::string &msg) {
    auto t = std::time(nullptr);
    char buf[100];
    std::strftime(buf, sizeof(buf), "[%F %T]", std::localtime(&t));
    std::cout << buf << " [" << level << "]    " << msg << std::endl;
}

void signal_handler(int signum) {
    log("INFO", "Caught signal " + std::to_string(signum) + ". Shutting down manager...");
    running = false;

    std::lock_guard<std::mutex> lock(node_mutex);
    for (auto &[id, node] : nodes) {
        std::string shutdown_msg = "SHUTDOWN";
        sockaddr_in node_addr{};
        node_addr.sin_family = AF_INET;
        node_addr.sin_port = htons(node.port);
        inet_pton(AF_INET, node.ip.c_str(), &node_addr.sin_addr);

        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd >= 0 && connect(sockfd, (sockaddr *)&node_addr, sizeof(node_addr)) == 0) {
            send(sockfd, shutdown_msg.c_str(), shutdown_msg.size(), 0);
            close(sockfd);
        }
    }

    log("INFO", "Manager: Shutdown complete.");
    exit(0);
}

void assign_tasks() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::lock_guard<std::mutex> lock(task_mutex);
        while (!task_queue.empty()) {
            std::lock_guard<std::mutex> nlock(node_mutex);
            bool assigned = false;

            std::string task = task_queue.front();
            auto it = tasks.find(task);
            if (it != tasks.end() && it->second.status == TaskStatus::COMPLETED) {
                log("INFO", "Skipping already completed task " + task);
                task_queue.pop();
                continue;
            }

            int mem_needed = it->second.memory_required;
            for (auto &[id, node] : nodes) {
                if (node.available_memory >= mem_needed) {
                    sockaddr_in node_addr{};
                    node_addr.sin_family = AF_INET;
                    node_addr.sin_port = htons(node.port);
                    inet_pton(AF_INET, node.ip.c_str(), &node_addr.sin_addr);

                    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
                    if (sockfd < 0 || connect(sockfd, (sockaddr *)&node_addr, sizeof(node_addr)) < 0) {
                        log("ERROR", "Manager: Failed to connect to node " + id + " at port " + std::to_string(node.port));
                        continue;
                    }

                    send(sockfd, task.c_str(), task.size(), 0);
                    close(sockfd);

                    log("INFO", "Assigned " + task + " to " + id + " at port " + std::to_string(node.port) + " (" + std::to_string(mem_needed) + " MB)");

                    tasks[task].status = TaskStatus::ASSIGNED;
                    tasks[task].assigned_node = id;
                    node.available_memory -= mem_needed;
                    task_queue.pop();
                    assigned = true;
                    break;
                }
            }

            if (!assigned) break;
        }
    }
}

void health_monitor() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::vector<std::string> down_nodes;
        {
            std::lock_guard<std::mutex> lock(node_mutex);
            for (auto &[id, node] : nodes) {
                sockaddr_in node_addr{};
                node_addr.sin_family = AF_INET;
                node_addr.sin_port = htons(node.port);
                inet_pton(AF_INET, node.ip.c_str(), &node_addr.sin_addr);
                int sockfd = socket(AF_INET, SOCK_STREAM, 0);
                bool alive = false;
                if (sockfd >= 0 && connect(sockfd, (sockaddr *)&node_addr, sizeof(node_addr)) == 0) {
                    alive = true;
                    close(sockfd);
                }
                if (!alive) {
                    down_nodes.push_back(id);
                }
            }
        }
        if (!down_nodes.empty()) {
            for (const auto &id : down_nodes) {
                log("WARN", "HealthMonitor: Node " + id + " is DOWN. Reallocating its unfinished tasks.");
                // Requeue unfinished tasks
                std::lock_guard<std::mutex> lock(task_mutex);
                for (auto &[task_id, entry] : tasks) {
                    if (entry.assigned_node == id && entry.status != TaskStatus::COMPLETED) {
                        entry.status = TaskStatus::QUEUED;
                        entry.assigned_node.clear();
                        task_queue.push(task_id);
                    }
                }
                // Remove node from nodes map
                std::lock_guard<std::mutex> nlock(node_mutex);
                nodes.erase(id);
            }
        }
        // If no nodes are available, notify manager
        {
            std::lock_guard<std::mutex> lock(node_mutex);
            if (nodes.empty()) {
                log("ERROR", "HealthMonitor: No node_agent is active!");
            }
        }
    }
}

void handle_node(int client_sock) {
    char buffer[1024] = {0};
    read(client_sock, buffer, sizeof(buffer));
    std::istringstream iss(buffer);
    std::string command, node_id;
    int port;
    int available_memory = 0;
    iss >> command >> node_id >> port >> available_memory;

    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getpeername(client_sock, (sockaddr *)&addr, &len);
    std::string ip = inet_ntoa(addr.sin_addr);

    if (command == "REGISTER") {
        NodeInfo node{node_id, ip, port, client_sock, available_memory};
        {
            std::lock_guard<std::mutex> lock(node_mutex);
            nodes[node_id] = node;
        }
        log("INFO", "Node " + node_id + " connected from " + ip + ":" + std::to_string(port) + " with " + std::to_string(available_memory) + " MB memory");
        log("INFO", "Manager: handling persistent connection for node " + node_id +
                    " (socket: " + std::to_string(client_sock) + ") to persistent handler.");
    }

    char recv_buf[1024];
    while (running) {
        ssize_t len = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, MSG_DONTWAIT);
        if (len > 0) {
            recv_buf[len] = '\0';
            std::string msg(recv_buf);
            std::istringstream stream(msg);
            std::string line;
            while (std::getline(stream, line)) {
                if (line.rfind("TASK_DONE ", 0) == 0) {
                    std::string task = line.substr(10);
                    std::lock_guard<std::mutex> lock(task_mutex);
                    auto &entry = tasks[task];
                    entry.status = TaskStatus::COMPLETED;
                    // Restore memory to node
                    auto n_it = nodes.find(node_id);
                    if (n_it != nodes.end()) {
                        n_it->second.available_memory += entry.memory_required;
                    }
                    log("INFO", "Manager: Task " + task + " marked as completed by " + node_id);
                }
            }
        } else {
            char ping[1];
            ssize_t res = recv(client_sock, ping, sizeof(ping), MSG_PEEK);
            if (res == 0) {
                log("WARN", "Node " + node_id + " disconnected unexpectedly.");

                std::lock_guard<std::mutex> lock(task_mutex);
                for (auto &[task_id, entry] : tasks) {
                    if (entry.assigned_node == node_id && entry.status != TaskStatus::COMPLETED) {
                        log("INFO", "Reassigning task " + task_id + " from failed node " + node_id);
                        entry.status = TaskStatus::QUEUED;
                        entry.assigned_node.clear();
                        task_queue.push(task_id);
                        // Restore memory to node (if node comes back)
                        auto n_it = nodes.find(node_id);
                        if (n_it != nodes.end()) {
                            n_it->second.available_memory += entry.memory_required;
                        }
                    }
                }

                {
                    std::lock_guard<std::mutex> nlock(node_mutex);
                    nodes.erase(node_id);
                }

                close(client_sock);
                return;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    close(client_sock);
}

void handle_client(int client_sock) {
    char buffer[1024] = {0};
    ssize_t valread = read(client_sock, buffer, sizeof(buffer));
    std::string task_input(buffer, valread);
    std::istringstream iss(task_input);
    std::string line;
    {
        std::lock_guard<std::mutex> lock(task_mutex);
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            // Parse task string: task_id:workload:memory:dependencies
            std::istringstream lss(line);
            std::string task_id, workload, memory_str, deps_str;
            std::getline(lss, task_id, ':');
            std::getline(lss, workload, ':');
            std::getline(lss, memory_str, ':');
            std::getline(lss, deps_str, ':');
            int memory = memory_str.empty() ? 128 : std::stoi(memory_str);
            std::vector<std::string> deps;
            // For prototype, dependencies are empty
            if (tasks.find(task_id) != tasks.end() && tasks[task_id].status == TaskStatus::COMPLETED) {
                log("INFO", "Ignoring already completed task: " + task_id);
                continue;
            }
            tasks[task_id] = TaskEntry{task_id, TaskStatus::QUEUED, "", memory, deps};
            task_queue.push(task_id);
            log("INFO", "Received task: " + task_id + " (" + std::to_string(memory) + " MB)");
        }
    }
    close(client_sock);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);

    // Open log file
    std::ofstream log_file("manager.log", std::ios::out | std::ios::app);

    // Create tee buffer to output to both console and file
    TeeBuf tee_buf(std::cout.rdbuf(), log_file.rdbuf());
    std::ostream dual_out(&tee_buf);
    std::cout.rdbuf(dual_out.rdbuf());  // Redirect std::cout to dual_out

    int port = 5000;
    if (argc == 2) port = std::stoi(argv[1]);

    log("INFO", "Manager starting...");

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    log("INFO", "Manager listening on 127.0.0.1:" + std::to_string(port));

    std::thread assign_thread(assign_tasks);
    std::thread health_thread(health_monitor);

    while (running) {
        sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);
        int new_socket = accept(server_fd, (sockaddr *)&client_addr, &addrlen);
        if (new_socket < 0) continue;

        char buffer[1024] = {0};
        recv(new_socket, buffer, 1024, MSG_PEEK);
        std::string peek(buffer);

        if (peek.rfind("REGISTER", 0) == 0) {
            std::thread(handle_node, new_socket).detach();
        } else {
            std::thread(handle_client, new_socket).detach();
        }
    }

    assign_thread.join();
    health_thread.join();
    close(server_fd);
    return 0;
}
