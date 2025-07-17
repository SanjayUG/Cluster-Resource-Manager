#include <iostream>
#include <string>
#include <thread>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

#define BUFFER_SIZE 1024

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <manager_ip> <manager_port> <number_of_tasks>\n";
        return 1;
    }

    std::string manager_ip = argv[1];
    int manager_port = std::stoi(argv[2]);
    int num_tasks = std::stoi(argv[3]);

    for (int i = 1; i <= num_tasks; ++i) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            std::cerr << "Error creating socket\n";
            continue;
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(manager_port);
        inet_pton(AF_INET, manager_ip.c_str(), &server_addr.sin_addr);

        if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Connection to manager failed for Task_" << i << "\n";
            close(sock);
            continue;
        }

        std::string task = "Task_" + std::to_string(i) + ":Workload_" + std::to_string(i);
        send(sock, task.c_str(), task.size(), 0);
        close(sock);
        std::cout << "[CLIENT] Sent: " << task << "\n";

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}