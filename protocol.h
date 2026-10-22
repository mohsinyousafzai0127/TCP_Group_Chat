#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <string>
#include <vector>

constexpr uint32_t MAX_PAYLOAD_SIZE = 64 * 1024; // 64 KB Sanity Cap
constexpr size_t HEADER_SIZE = 9;

enum class MsgType : uint8_t {
    JOIN            = 1,
    JOIN_OK         = 2,
    JOIN_ERR        = 3,
    HISTORY         = 4,
    HISTORY_END     = 5,
    CHAT            = 6,
    CHAT_BCAST      = 7,
    PEER_LIST_REQ   = 8,
    PEER_LIST       = 9,
    PEER_JOINED     = 10,
    PEER_LEFT       = 11,
    LEAVE           = 12
};

struct Header {
    MsgType type;
    uint32_t seq;
    uint32_t len;
};

bool sendMsg(int fd, MsgType type, uint32_t seq, const uint8_t* data, uint32_t len);
bool sendMsg(int fd, MsgType type, uint32_t seq, const std::string& payload);
bool recvMsg(int fd, Header& header, std::vector<uint8_t>& payload);

std::vector<uint8_t> packChatPayload(const std::string& name, const std::string& text);
bool unpackChatPayload(const std::vector<uint8_t>& payload, std::string& name, std::string& text);

#endif
