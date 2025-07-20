#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

struct NodeInfo {
    std::string id, ip, health;
    int port, available_memory;
};
struct TaskInfo {
    std::string id, status, assigned_node;
    int memory_required;
};

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

void print_dashboard(const std::vector<NodeInfo>& nodes, const std::vector<TaskInfo>& tasks) {
    system("clear");
    std::cout << "+------------------- Nodes -----------------------------+\n";
    std::cout << "| ID     | IP         | Port | Mem(MB) | Health |\n";
    std::cout << "+--------+------------+------+---------+--------+\n";
    for (const auto& n : nodes) {
        std::string color, reset = "\033[0m";
        if (n.health == "UP") color = "\033[32m"; // green
        else color = "\033[31m"; // red
        std::cout << "| " << n.id << std::string(7-n.id.size(),' ')
                  << "| " << n.ip << std::string(11-n.ip.size(),' ')
                  << "| " << n.port << std::string(5-std::to_string(n.port).size(),' ')
                  << "| " << n.available_memory << std::string(8-std::to_string(n.available_memory).size(),' ')
                  << "| " << color << n.health << reset << std::string(6-n.health.size(),' ') << "|\n";
    }
    std::cout << "+------------------- Tasks -------------------+\n";
    std::cout << "| ID     | Status   | Node     | Mem(MB) |\n";
    std::cout << "+--------+----------+----------+---------+\n";
    for (const auto& t : tasks) {
        std::cout << "| " << t.id << std::string(7-t.id.size(),' ')
                  << "| " << t.status << std::string(9-t.status.size(),' ')
                  << "| " << t.assigned_node << std::string(9-t.assigned_node.size(),' ')
                  << "| " << t.memory_required << std::string(8-std::to_string(t.memory_required).size(),' ') << "|\n";
    }
    std::cout << "+---------------------------------------------+\n";
}

int main(int argc, char* argv[]) {
    std::string manager_ip = "127.0.0.1";
    int status_port = 6000;
    if (argc >= 2) manager_ip = argv[1];
    if (argc >= 3) status_port = std::stoi(argv[2]);
    while (true) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            std::cerr << "[DASHBOARD] Error creating socket\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(status_port);
        inet_pton(AF_INET, manager_ip.c_str(), &server_addr.sin_addr);
        if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "[DASHBOARD] Could not connect to manager at " << manager_ip << ":" << status_port << "\n";
            close(sock);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        std::string data;
        char buffer[4096];
        ssize_t n;
        while ((n = recv(sock, buffer, sizeof(buffer)-1, 0)) > 0) {
            buffer[n] = '\0';
            data += buffer;
        }
        close(sock);
        if (data.empty()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        try {
            std::istringstream iss(data);
            std::string line, section;
            std::vector<NodeInfo> nodes;
            std::vector<TaskInfo> tasks;
            const size_t MAX_LINE_LEN = 1024;
            const size_t MAX_LINES = 10000;
            size_t line_count = 0;
            while (std::getline(iss, line)) {
                if (++line_count > MAX_LINES) break;
                if (line.length() > MAX_LINE_LEN) continue;
                if (line == "NODES") { section = "NODES"; continue; }
                if (line == "TASKS") { section = "TASKS"; continue; }
                if (section == "NODES" && !line.empty()) {
                    auto fields = split(line, ',');
                    if (fields.size() != 5) continue; // id, ip, port, mem, health
                    int port_val = 0, mem_val = 0;
                    try { port_val = std::stoi(fields[2]); } catch (...) { port_val = 0; }
                    try { mem_val = std::stoi(fields[3]); } catch (...) { mem_val = 0; }
                    nodes.push_back({fields[0], fields[1], fields[4], port_val, mem_val});
                } else if (section == "TASKS" && !line.empty()) {
                    auto fields = split(line, ',');
                    if (fields.size() != 4) continue; // id, status, node, mem
                    int mem_val = 0;
                    try { mem_val = std::stoi(fields[3]); } catch (...) { mem_val = 0; }
                    tasks.push_back({fields[0], fields[1], fields[2], mem_val});
                }
            }
            print_dashboard(nodes, tasks);
        } catch (const std::exception& e) {
            std::cerr << "[DASHBOARD] Exception: " << e.what() << std::endl;
            // Sleep to avoid tight error loop
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
} 