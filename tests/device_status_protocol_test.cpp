#include "device_status_protocol.h"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace device_status;

int main() {
    Message m{};
    m.sender[0] = 1;
    Message out{};
    auto wire = encode(m);
    assert(decode(wire.data(), wire.size(), out));
    assert(out.sender == m.sender);
    assert(!decode(nullptr, WireSize, out));
    for (size_t n = 0; n < WireSize; ++n) assert(!decode(wire.data(), n, out));
    assert(!decode(wire.data(), WireSize + 1, out));
    for (size_t i = 16; i < WireSize; ++i) {
        auto bad = wire; bad[i] = 1;
        assert(!decode(bad.data(), bad.size(), out));
    }
    for (size_t i : {size_t(0), size_t(6), size_t(7)}) {
        auto bad = wire; bad[i] ^= 1;
        assert(!decode(bad.data(), bad.size(), out));
    }
    auto bad = wire; bad[4] = 255;
    assert(!decode(bad.data(), bad.size(), out));
    bad = wire; bad[8] = 0;
    assert(!decode(bad.data(), bad.size(), out));
    m.receiver[0] = 2; m.nonce[0] = 3;
    for (Type type : {Type::Request, Type::Accept, Type::Status, Type::Disconnect, Type::Heartbeat}) {
        m.type = type;
        m.status = type == Type::Status ? Status::Ready : Status::None;
        m.sequence = type == Type::Request || type == Type::Accept ? 0 : 0x12345678;
        wire = encode(m);
        assert(decode(wire.data(), wire.size(), out));
        assert(out.sequence == m.sequence && out.nonce == m.nonce && out.receiver == m.receiver);
        assert(belongsToSession(out, m.receiver, m.sender, m.nonce));
        Id other{}; other[0] = 4;
        assert(!belongsToSession(out, other, m.sender, m.nonce));
        assert(!belongsToSession(out, m.receiver, other, m.nonce));
        Nonce otherNonce{}; otherNonce[0] = 4;
        assert(!belongsToSession(out, m.receiver, m.sender, otherNonce));
        bad = wire; bad[16] = 0;
        assert(!decode(bad.data(), bad.size(), out));
        bad = wire; bad[28] = 0;
        assert(!decode(bad.data(), bad.size(), out));
        bad = wire;
        for (size_t i = 24; i < 28; ++i) bad[i] = 0;
        if (!m.sequence) bad[27] = 1;
        assert(!decode(bad.data(), bad.size(), out));
        for (unsigned s = 0; s < 256; ++s) {
            bad = wire; bad[5] = s;
            const bool valid = type == Type::Status ? s >= 1 && s <= 4 : s == 0;
            assert(decode(bad.data(), bad.size(), out) == valid);
        }
    }
    Key key{};
    assert(parseKey("0123456789abcdefABCDEF9876543201", key));
    assert(key[0] == 1 && key[15] == 1);
    const Key before = key;
    for (auto text : {"", "1", "00000000000000000000000000000000", "z123456789abcdefABCDEF98765432101"}) {
        assert(!parseKey(text, key)); assert(key == before);
    }
    assert(!parseKey(nullptr, key));
    assert(isFresh(1, 0)); assert(!isFresh(0, 0));
    assert(!isFresh(42, 42)); assert(!isFresh(41, 42)); assert(isFresh(43, 42));
    assert(!isFresh(1, std::numeric_limits<uint32_t>::max()));
    std::array<uint8_t, 32> a{}, b{};
    assert(sameTag(a.data(), b.data()));
    for (size_t i = 0; i < b.size(); ++i) {
        b[i] = 1; assert(!sameTag(a.data(), b.data())); b[i] = 0;
    }
    std::puts("Device status protocol: all tests passed");
}
