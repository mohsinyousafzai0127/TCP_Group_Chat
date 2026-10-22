#include "protocol.h"
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <cerrno>

static bool sendAll(int fd, const uint8_t* buf, size_t len) {
    size_t totalSent = 0;
    while (totalSent < len) {
        ssize_t sent = send(fd, buf + totalSent, len - totalSent, 0);
        if (sent <= 0) {
            if (sent < 0 && errno == EINTR) continue;
            return false;
        }
        totalSent += sent;
    }
    return true;
}

static bool recvAll(int fd, uint8_t* buf, size_t len) {
    size_t totalRecvd = 0;
    while (totalRecvd < len) {
        ssize_t recvd = recv(fd, buf + totalRecvd, len - totalRecvd, 0);
        if (recvd <= 0) {
            if (recvd < 0 && errno == EINTR) continue;
            return false;
        }
        totalRecvd += recvd;
    }
    return true;
}

bool sendMsg(int fd, MsgType type, uint32_t seq, const uint8_t* data, uint32_t len) {
    if (len > MAX_PAYLOAD_SIZE) return false;

    uint8_t header[HEADER_SIZE];
    header[0] = static_cast<uint8_t>(type);
    
    uint32_t net_seq = htonl(seq);
    uint32_t net_len = htonl(len);
    std::memcpy(header + 1, &net_seq, 4);
    std::memcpy(header + 5, &net_len, 4);

    if (!sendAll(fd, header, HEADER_SIZE)) return false;
    if (len > 0 && data != nullptr) {
        if (!sendAll(fd, data, len)) return false;
    }
    return true;
}

bool sendMsg(int fd, MsgType type, uint32_t seq, const std::string& payload) {
    return sendMsg(fd, type, seq, reinterpret_cast<const uint8_t*>(payload.data()), static_cast<uint32_t>(payload.size()));
}

bool recvMsg(int fd, Header& header, std::vector<uint8_t>& payload) {
    uint8_t rawHeader[HEADER_SIZE];
    if (!recvAll(fd, rawHeader, HEADER_SIZE)) return false;

    header.type = static_cast<MsgType>(rawHeader[0]);
    
    uint32_t net_seq, net_len;
    std::memcpy(&net_seq, rawHeader + 1, 4);
    std::memcpy(&net_len, rawHeader + 5, 4);

    header.seq = ntohl(net_seq);
    header.len = ntohl(net_len);

    if (header.len > MAX_PAYLOAD_SIZE) {
        return false;
    }

    payload.resize(header.len);
    if (header.len > 0) {
        if (!recvAll(fd, payload.data(), header.len)) return false;
    }
    return true;
}

std::vector<uint8_t> packChatPayload(const std::string& name, const std::string& text) {
    std::vector<uint8_t> result;
    if (name.size() > 255) return result;
    
    uint8_t name_len = static_cast<uint8_t>(name.size());
    result.push_back(name_len);
    result.insert(result.end(), name.begin(), name.end());
    result.insert(result.end(), text.begin(), text.end());
    return result;
}

bool unpackChatPayload(const std::vector<uint8_t>& payload, std::string& name, std::string& text) {
    if (payload.empty()) return false;
    uint8_t name_len = payload[0];
    if (payload.size() < 1 + name_len) return false;

    name.assign(reinterpret_cast<const char*>(payload.data() + 1), name_len);
    text.assign(reinterpret_cast<const char*>(payload.data() + 1 + name_len), payload.size() - 1 - name_len);
    return true;
}
