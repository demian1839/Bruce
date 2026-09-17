#include "device_status.h"
#include "device_status_service.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <mbedtls/md.h>

// Intentionally independent of all command dispatchers and file-transfer services.
namespace device_status {
namespace {
constexpr uint16_t Port = 47653;
constexpr uint32_t BeaconMs = 3000, ExpireMs = 15000, PairMs = 60000;
struct Peer {
    bool used = false, connected = false;
    Id id{};
    Key key{};
    Nonce nonce{};
    IPAddress address;
    uint32_t tx = 0, rx = 0, seen = 0, started = 0;
    Status status = Status::None;
};
struct Pending {
    bool used = false;
    Message message{};
    IPAddress address;
};
WiFiUDP udp;
SemaphoreHandle_t mutex = nullptr;
View view;
std::array<Peer, MaxPeers> peers{};
std::array<IPAddress, MaxFound> addresses{};
Pending pending;
uint32_t offeredAt = 0, lastBeacon = 0, lastRequestCheck = 0;
IPAddress station, accessPoint, stationMask, apMask;

struct Lock {
    bool held;
    Lock() : held(mutex && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {}
    ~Lock() { if (held) xSemaphoreGive(mutex); }
};

template <size_t N> void randomBytes(std::array<uint8_t, N> &bytes) {
    do { esp_fill_random(bytes.data(), bytes.size()); } while (zero(bytes.data(), bytes.size()));
}

bool tag(const Wire &wire, const Key &key, uint8_t *result) {
    const auto *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    return md && mbedtls_md_hmac(md, key.data(), key.size(), wire.data(), SignedSize, result) == 0;
}
bool authentic(const Wire &wire, const Key &key) {
    uint8_t expected[32]{};
    return tag(wire, key, expected) && sameTag(expected, wire.data() + SignedSize);
}
void send(const Message &message, IPAddress address, const Key *key = nullptr) {
    Wire wire = encode(message);
    if (key && !tag(wire, *key, wire.data() + SignedSize)) return;
    if (!udp.beginPacket(address, Port)) return;
    udp.write(wire.data(), wire.size());
    udp.endPacket();
}
bool sameNetwork(IPAddress address, IPAddress local, IPAddress mask) {
    return uint32_t(local) != 0 && uint32_t(mask) != 0 &&
           (uint32_t(address) & uint32_t(mask)) == (uint32_t(local) & uint32_t(mask));
}
bool localAddress(IPAddress address) {
    return sameNetwork(address, station, stationMask) || sameNetwork(address, accessPoint, apMask);
}
void clearOffer() {
    view.offering = false;
    view.offer.fill(0);
    pending = Pending{};
}
void resetNetwork() {
    udp.stop();
    view.online = false;
    view.found = {};
    addresses = {};
    peers = {};
    clearOffer();
    randomBytes(view.local);
}
Peer *freePeer() {
    for (auto &p : peers) if (!p.used) return &p;
    return nullptr;
}
Peer *findPeer(const Id &id) {
    for (auto &p : peers) if (p.used && p.id == id) return &p;
    return nullptr;
}
void sendSession(Peer &p, Type type, Status status = Status::None) {
    // Never wrap the sequence counter and reuse an authenticated sequence.
    if (p.tx == UINT32_MAX) { p = Peer{}; return; }
    Message m{};
    m.type = type;
    m.status = status;
    m.sender = view.local;
    m.receiver = p.id;
    m.nonce = p.nonce;
    m.sequence = ++p.tx;
    send(m, p.address, &p.key);
}
void sendPair(Peer &p, Type type) {
    Message m{};
    m.type = type;
    m.sender = view.local;
    m.receiver = p.id;
    m.nonce = p.nonce;
    send(m, p.address, &p.key);
}
void receive(uint32_t now) {
    // Bound work per tick even under a stream of unsolicited datagrams.
    for (unsigned n = 0; n < 8; ++n) {
        const int size = udp.parsePacket();
        if (!size) break;
        const IPAddress source = udp.remoteIP();
        if (size != int(WireSize) || udp.remotePort() != Port || !localAddress(source)) {
            udp.clear();
            continue;
        }
        Wire wire{};
        const int read = udp.read(wire.data(), wire.size());
        Message m{};
        if (read != int(WireSize) || !decode(wire.data(), wire.size(), m) || m.sender == view.local) continue;
        if (m.type == Type::Beacon) {
            // Discovery is untrusted. It can never establish a session.
            size_t slot = MaxFound;
            for (size_t i = 0; i < MaxFound; ++i) {
                if (view.found[i].used && view.found[i].id == m.sender) { slot = i; break; }
                if (!view.found[i].used && slot == MaxFound) slot = i;
            }
            if (slot != MaxFound) {
                view.found[slot] = {m.sender, now, true};
                addresses[slot] = source;
            }
            continue;
        }
        if (m.receiver != view.local) continue;
        if (m.type == Type::Request) {
            if (!view.offering || pending.used || findPeer(m.sender) || !freePeer() ||
                now - lastRequestCheck < 250) continue;
            lastRequestCheck = now;
            if (!authentic(wire, view.offer)) continue;
            pending.used = true;
            pending.message = m;
            pending.address = source;
            // No acceptance or remote action without local UI confirmation.
            continue;
        }
        Peer *p = findPeer(m.sender);
        if (!p || p->address != source || !belongsToSession(m, view.local, p->id, p->nonce) ||
            !authentic(wire, p->key)) continue;
        if (m.type == Type::Accept) {
            if (!p->connected) { p->connected = true; p->seen = now; }
            continue;
        }
        if (!isFresh(m.sequence, p->rx)) continue;
        p->rx = m.sequence;
        p->seen = now;
        // A signed heartbeat also confirms acceptance if the Accept datagram was lost.
        p->connected = true;
        if (m.type == Type::Disconnect) *p = Peer{};
        else if (m.type == Type::Status) p->status = m.status; // DISPLAY ONLY.
    }
}
void serviceTask(void *) {
    for (;;) {
        {
            Lock lock;
            const uint32_t now = millis();
            const auto mode = WiFi.getMode();
            IPAddress sta, ap, sm, am;
            if ((mode & WIFI_STA) && WiFi.status() == WL_CONNECTED) {
                sta = WiFi.localIP();
                sm = WiFi.subnetMask();
            }
            if (mode & WIFI_AP) { ap = WiFi.softAPIP(); am = WiFi.softAPSubnetMask(); }
            if (sta != station || ap != accessPoint || sm != stationMask || am != apMask) {
                resetNetwork();
                station = sta; accessPoint = ap; stationMask = sm; apMask = am;
            }
            if (!view.online && (uint32_t(station) || uint32_t(accessPoint))) {
                view.online = udp.begin(Port) != 0;
                lastBeacon = now - BeaconMs;
            }
            if (view.offering && now - offeredAt >= PairMs) clearOffer();
            for (auto &f : view.found) if (f.used && now - f.seen >= ExpireMs) f = Found{};
            for (auto &p : peers) {
                if (p.used && ((p.connected && now - p.seen >= ExpireMs) ||
                               (!p.connected && now - p.started >= PairMs))) p = Peer{};
            }
            if (view.online) {
                receive(now);
                if (now - lastBeacon >= BeaconMs) {
                    lastBeacon = now;
                    Message beacon{};
                    beacon.sender = view.local;
                    if (uint32_t(station)) send(beacon, IPAddress(uint32_t(station) | ~uint32_t(stationMask)));
                    if (uint32_t(accessPoint)) send(beacon, IPAddress(uint32_t(accessPoint) | ~uint32_t(apMask)));
                    for (auto &p : peers) if (p.used) {
                        if (p.connected) sendSession(p, Type::Heartbeat);
                        else sendPair(p, Type::Request);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
} // namespace

View statusView() {
    Lock lock;
    if (!lock.held) return View{};
    View result = view;
    result.pending = pending.used;
    result.requester = pending.message.sender;
    for (size_t i = 0; i < MaxPeers; ++i)
        result.peers[i] = {peers[i].id, peers[i].status, peers[i].used, peers[i].connected};
    return result;
}
bool openStatusOffer() {
    Lock lock;
    if (!lock.held || !view.online || !freePeer()) return false;
    clearOffer();
    randomBytes(view.offer);
    offeredAt = millis();
    view.offering = true;
    return true;
}
void closeStatusOffer() { Lock lock; if (lock.held) clearOffer(); }
bool requestStatusPeer(const Id &id, const Key &key) {
    Lock lock;
    if (!lock.held || !view.online || findPeer(id) || zero(key.data(), key.size())) return false;
    Peer *p = freePeer();
    if (!p) return false;
    for (size_t i = 0; i < MaxFound; ++i) {
        if (!view.found[i].used || view.found[i].id != id || millis() - view.found[i].seen >= ExpireMs) continue;
        *p = Peer{};
        p->used = true;
        p->id = id;
        p->key = key;
        p->address = addresses[i];
        p->started = millis();
        randomBytes(p->nonce);
        sendPair(*p, Type::Request);
        return true;
    }
    return false;
}
bool approveStatusPeer() {
    Lock lock;
    if (!lock.held || !view.online || !view.offering || !pending.used || millis() - offeredAt >= PairMs) return false;
    Peer *p = freePeer();
    if (!p || findPeer(pending.message.sender)) return false;
    *p = Peer{};
    p->used = p->connected = true;
    p->id = pending.message.sender;
    p->key = view.offer;
    p->nonce = pending.message.nonce;
    p->address = pending.address;
    p->seen = millis();
    sendPair(*p, Type::Accept);
    clearOffer(); // One key, one acceptance; never reuse an offer.
    return true;
}
void disconnectStatusPeers() {
    Lock lock;
    if (!lock.held) return;
    for (auto &p : peers) if (p.used) { sendSession(p, Type::Disconnect); p = Peer{}; }
    clearOffer();
}
void publishDeviceStatus(Status status) {
    Lock lock;
    if (!lock.held || status < Status::Ready || status > Status::Hello) return;
    for (auto &p : peers) if (p.used && p.connected) sendSession(p, Type::Status, status);
}
const char *statusLabel(Status status) {
    switch (status) {
        case Status::Ready: return "Bereit";
        case Status::Busy: return "Beschaeftigt";
        case Status::LowBattery: return "Akku niedrig";
        case Status::Hello: return "Hallo";
        default: return "Kein Status";
    }
}
String idLabel(const Id &id) {
    char text[17]{};
    for (size_t i = 0; i < id.size(); ++i) snprintf(text + 2 * i, 3, "%02x", id[i]);
    return String(text);
}
String keyLabel(const Key &key) {
    char text[33]{};
    for (size_t i = 0; i < key.size(); ++i) snprintf(text + 2 * i, 3, "%02x", key[i]);
    return String(text);
}
} // namespace device_status

void startDeviceStatus() {
    using namespace device_status;
    // Called once from setup, before any menu can access the service.
    if (mutex) return;
    mutex = xSemaphoreCreateMutex();
    if (!mutex) return;
    randomBytes(view.local);
    view.running = true;
    if (xTaskCreate(serviceTask, "DeviceStatus", 6144, nullptr, 1, nullptr) != pdPASS) {
        view.running = false;
        vSemaphoreDelete(mutex);
        mutex = nullptr;
    }
}
