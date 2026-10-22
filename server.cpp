#include "protocol.h"
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <string>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

struct Client {
    int fd;
    std::string username;
};

struct ChatMessage {
    uint32_t seq;
    std::string name;
    std::string text;
};

std::mutex server_mutex;
std::vector<Client> clients;
std::vector<ChatMessage> history_log;
uint32_t global_seq = 0;

void broadcast(MsgType type, uint32_t seq, const std::vector<uint8_t>& payload, int exclude_fd = -1) {
    std::lock_guard<std::mutex> lock(server_mutex);
    for (const auto& c : clients) {
        if (c.fd != exclude_fd) {
            sendMsg(c.fd, type, seq, payload.data(), payload.size());
        }
    }
}

void handle_client(int client_fd) {
    Header header;
    std::vector<uint8_t> payload;

    if (!recvMsg(client_fd, header, payload) || header.type != MsgType::JOIN) {
        close(client_fd);
        return;
    }

    std::string username(payload.begin(), payload.end());
    uint32_t client_last_seq = header.seq;

    {
        std::lock_guard<std::mutex> lock(server_mutex);
        auto it = std::find_if(clients.begin(), clients.end(), [&](const Client& c){ return c.username == username; });
        if (it != clients.end() || username.empty()) {
            sendMsg(client_fd, MsgType::JOIN_ERR, 0, "Username taken or invalid");
            close(client_fd);
            return;
        }
    }

    if (!sendMsg(client_fd, MsgType::JOIN_OK, 0, "")) {
        close(client_fd);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(server_mutex);
        for (const auto& msg : history_log) {
            if (msg.seq > client_last_seq) {
                auto p = packChatPayload(msg.name, msg.text);
                sendMsg(client_fd, MsgType::HISTORY, msg.seq, p.data(), p.size());
            }
        }
        sendMsg(client_fd, MsgType::HISTORY_END, global_seq, "");
        clients.push_back({client_fd, username});
    }

    std::vector<uint8_t> join_p(username.begin(), username.end());
    broadcast(MsgType::PEER_JOINED, 0, join_p, client_fd);

    while (true) {
        Header h;
        std::vector<uint8_t> p;
        if (!recvMsg(client_fd, h, p)) break;

        if (h.type == MsgType::CHAT) {
            std::string text(p.begin(), p.end());
            uint32_t assigned_seq = 0;
            std::vector<uint8_t> bcast_payload;

            {
                std::lock_guard<std::mutex> lock(server_mutex);
                global_seq++;
                assigned_seq = global_seq;
                history_log.push_back({assigned_seq, username, text});
                bcast_payload = packChatPayload(username, text);
            }
            broadcast(MsgType::CHAT_BCAST, assigned_seq, bcast_payload);
        } 
        else if (h.type == MsgType::PEER_LIST_REQ) {
            std::string list_str;
            {
                std::lock_guard<std::mutex> lock(server_mutex);
                for (size_t i = 0; i < clients.size(); ++i) {
                    list_str += clients[i].username + (i + 1 < clients.size() ? "\n" : "");
                }
            }
            sendMsg(client_fd, MsgType::PEER_LIST, 0, list_str);
        } 
        else if (h.type == MsgType::LEAVE) {
            break;
        }
    }

    {
        std::lock_guard<std::mutex> lock(server_mutex);
        clients.erase(std::remove_if(clients.begin(), clients.end(), [client_fd](const Client& c){ return c.fd == client_fd; }), clients.end());
    }

    std::vector<uint8_t> left_p(username.begin(), username.end());
    broadcast(MsgType::PEER_LEFT, 0, left_p);
    close(client_fd);
}

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);
    int port = (argc > 1) ? std::stoi(argv[1]) : 8080;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);

    std::cout << "[Server] Running on port " << port << "\n";

    while (true) {
        sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_fd < 0) continue;

        std::thread(handle_client, client_fd).detach();
    }
    return 0;
}