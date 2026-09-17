#pragma once
#include "device_status_protocol.h"
#include <Arduino.h>

namespace device_status {
constexpr size_t MaxPeers = 4;
constexpr size_t MaxFound = 8;
struct Found { Id id{}; uint32_t seen = 0; bool used = false; };
struct PeerView { Id id{}; Status status = Status::None; bool used = false; bool connected = false; };
struct View {
    bool running = false;
    bool online = false;
    Id local{};
    std::array<Found, MaxFound> found{};
    std::array<PeerView, MaxPeers> peers{};
    bool offering = false;
    Key offer{};
    bool pending = false;
    Id requester{};
};
View statusView();
bool openStatusOffer();
void closeStatusOffer();
bool requestStatusPeer(const Id &id, const Key &key);
bool approveStatusPeer();
void disconnectStatusPeers();
void publishDeviceStatus(Status status);
const char *statusLabel(Status status);
String idLabel(const Id &id);
String keyLabel(const Key &key);
} // namespace device_status
