// node_agent.cpp
#include <arpa/inet.h>
#include <csignal>
#include <cstring>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <atomic>
#include <ctime>

std::string node_id;
int task_listener_fd = -1;
int manager_fd = -1;
std::atomic<bool> running{true};

void log(const std::string &level, const std::string &msg) {
    auto t = std::time(nullptr);
    char buf[100];
    std::strftime(buf, sizeof(buf), "[%F %T]", std::localtime(&t));
    std::cout << buf << " [" << level << "]    " << msg << std::endl;
}

void signal_handler(int signum) {
    log("INFO", "Caught signal " + std::to_string(signum) + ". Shutting down node...");
    running = false;
    log("INFO", "Node " + node_id + ": Shutting down...");
    if (manager_fd != -1) close(manager_fd);
    if (task_listener_fd != -1) close(task_listener_fd);
    exit(0);
}

void execute_task(const std::string &task) {
    log("INFO", "Node " + node_id + ": Received task: " + task);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    log("INFO", "Node " + node_id + ": Completed task: " + task);

    std::string clean_task = task;
    size_t first = clean_task.find_first_not_of(" \t\n\r");
    size_t last = clean_task.find_last_not_of(" \t\n\r");
    if (first != std::string::npos)
        clean_task = clean_task.substr(first, last - first + 1);
    else
        clean_task = "";

    if (!clean_task.empty()) {
        std::string done_msg = "TASK_DONE " + clean_task + "\n";
        send(manager_fd, done_msg.c_str(), done_msg.length(), 0);
    }
}

void task_listener(int port) {
    sockaddr_in server_addr{}, client_addr{};
    socklen_t client_len = sizeof(client_addr);
    task_listener_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (task_listener_fd < 0) {
        log("ERROR", "Node " + node_id + ": Failed to create task listener socket.");
        return;
    }

    int opt = 1;
    setsockopt(task_listener_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(task_listener_fd, (sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log("ERROR", "Node " + node_id + ": Bind failed on task port.");
        return;
    }

    listen(task_listener_fd, 5);
    log("INFO", "Node " + node_id + ": Listening for tasks on port " + std::to_string(port) + "...");

    while (running) {
        int client_fd = accept(task_listener_fd, (sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        char buffer[1024] = {0};
        ssize_t valread = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (valread <= 0) {
            close(client_fd);
            continue;
        }
        buffer[valread] = '\0';
        std::string task_raw(buffer);

        size_t first = task_raw.find_first_not_of(" \t\n\r");
        size_t last = task_raw.find_last_not_of(" \t\n\r");
        task_raw = (first == std::string::npos) ? "" : task_raw.substr(first, last - first + 1);

        if (task_raw == "SHUTDOWN") {
            log("INFO", "Node " + node_id + ": Received shutdown signal from manager.");
            running = false;
            close(client_fd);
            break;
        } else if (!task_raw.empty()) {
            execute_task(task_raw);
        }

        close(client_fd);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <node_id> <manager_ip> <manager_port> <listen_port>\n";
        return 1;
    }

    signal(SIGINT, signal_handler);

    node_id = argv[1];
    std::string manager_ip = argv[2];
    int manager_port = std::stoi(argv[3]);
    int task_port = std::stoi(argv[4]);

    log("INFO", "NodeAgent " + node_id + ": initialized.");

    sockaddr_in manager_addr{};
    manager_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (manager_fd < 0) {
        log("ERROR", "Node " + node_id + ": Failed to create socket to manager.");
        return 1;
    }

    manager_addr.sin_family = AF_INET;
    manager_addr.sin_port = htons(manager_port);
    inet_pton(AF_INET, manager_ip.c_str(), &manager_addr.sin_addr);

    if (connect(manager_fd, (sockaddr *)&manager_addr, sizeof(manager_addr)) < 0) {
        log("ERROR", "Node " + node_id + ": Could not connect to manager.");
        close(manager_fd);
        return 1;
    }

    log("INFO", "Node " + node_id + ": Connected to manager at " + manager_ip + ":" + std::to_string(manager_port));

    int available_memory_mb = 512; // For prototype, hardcoded
    std::string reg_msg = "REGISTER " + node_id + " " + std::to_string(task_port) + " " + std::to_string(available_memory_mb);
    send(manager_fd, reg_msg.c_str(), reg_msg.length(), 0);
    log("INFO", "Node " + node_id + ": Sent registration message to manager with memory info.");

    std::thread listener(task_listener, task_port);
    listener.join();

    log("INFO", "Node " + node_id + ": Shutdown complete.");
    return 0;
}
