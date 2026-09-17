#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

// Separate from ESP-NOW commands/file sharing. No variable-length data or executable messages.
namespace device_status {
constexpr size_t WireSize = 76;
constexpr size_t SignedSize = 44;
using Id = std::array<uint8_t, 8>;
using Key = std::array<uint8_t, 16>;
using Nonce = std::array<uint8_t, 16>;
using Wire = std::array<uint8_t, WireSize>;
enum class Type : uint8_t { Beacon = 0, Request, Accept, Status, Disconnect, Heartbeat };
enum class Status : uint8_t { None = 0, Ready, Busy, LowBattery, Hello };

struct Message {
    Type type = Type::Beacon;
    Status status = Status::None;
    Id sender{};
    Id receiver{};
    uint32_t sequence = 0;
    Nonce nonce{};
};

inline bool zero(const uint8_t *data, size_t size) {
    uint8_t result = 0;
    for (size_t i = 0; i < size; ++i) result |= data[i];
    return result == 0;
}

inline Wire encode(const Message &m) {
    Wire wire{};
    std::memcpy(wire.data(), "BSD1", 4);
    wire[4] = static_cast<uint8_t>(m.type);
    wire[5] = static_cast<uint8_t>(m.status);
    std::memcpy(wire.data() + 8, m.sender.data(), 8);
    std::memcpy(wire.data() + 16, m.receiver.data(), 8);
    for (size_t i = 0; i < 4; ++i) wire[24 + i] = static_cast<uint8_t>(m.sequence >> (24 - 8 * i));
    std::memcpy(wire.data() + 28, m.nonce.data(), 16);
    return wire;
}

inline bool decode(const uint8_t *wire, size_t size, Message &out) {
    if (!wire || size != WireSize || std::memcmp(wire, "BSD1", 4) || wire[6] || wire[7]) return false;
    if (wire[4] > static_cast<uint8_t>(Type::Heartbeat)) return false;
    Message m;
    m.type = static_cast<Type>(wire[4]);
    m.status = static_cast<Status>(wire[5]);
    if (m.type == Type::Status) {
        if (wire[5] < 1 || wire[5] > static_cast<uint8_t>(Status::Hello)) return false;
    } else if (wire[5] != 0) return false;
    std::memcpy(m.sender.data(), wire + 8, 8);
    std::memcpy(m.receiver.data(), wire + 16, 8);
    for (size_t i = 0; i < 4; ++i) m.sequence = (m.sequence << 8) | wire[24 + i];
    std::memcpy(m.nonce.data(), wire + 28, 16);
    if (zero(m.sender.data(), m.sender.size())) return false;
    if (m.type == Type::Beacon) {
        if (!zero(wire + 16, WireSize - 16)) return false;
    } else {
        if (zero(m.receiver.data(), m.receiver.size()) || zero(m.nonce.data(), m.nonce.size())) return false;
        if (m.type == Type::Request || m.type == Type::Accept) {
            if (m.sequence != 0) return false;
        } else if (m.sequence == 0) return false;
    }
    out = m;
    return true;
}

inline bool sameTag(const uint8_t *a, const uint8_t *b) {
    volatile uint8_t difference = 0;
    for (size_t i = 0; i < 32; ++i) difference = difference | (a[i] ^ b[i]);
    return difference == 0;
}

inline bool parseKey(const char *text, Key &key) {
    if (!text || std::strlen(text) != 32) return false;
    Key parsed{};
    for (size_t i = 0; i < 32; ++i) {
        const char c = text[i];
        int value = c >= '0' && c <= '9' ? c - '0' :
                    c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                    c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (value < 0) return false;
        parsed[i / 2] = static_cast<uint8_t>((parsed[i / 2] << 4) | value);
    }
    if (zero(parsed.data(), parsed.size())) return false;
    key = parsed;
    return true;
}

// Only the exact authenticated session may update an established peer.
inline bool belongsToSession(const Message &m, const Id &local, const Id &remote, const Nonce &nonce) {
    return m.receiver == local && m.sender == remote && m.nonce == nonce;
}
inline bool isFresh(uint32_t received, uint32_t previous) { return received != 0 && received > previous; }
} // namespace device_status
