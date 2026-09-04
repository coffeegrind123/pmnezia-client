// SPDX-License-Identifier: GPL-3.0-or-later
//
// Per-datagram fragment reassembler for QQ-DNS — a C++ port of the Rust
// reference (coffeeblack-vpn `src/qqdns/reassembly.rs`) / QQ-Tunnel's
// `data_handler.py`. Fragments of one datagram share a dataOffset (the key);
// each carries its fragmentPart index (0..63) and a last-fragment flag. A
// datagram is complete once the last fragment is seen and every index up to it
// has arrived; the base32 chunks are then concatenated in order and returned
// (the caller base32-decodes the whole thing).
//
// Defences mirror the reference exactly: duplicate fragments are ignored, and
// structurally inconsistent sets (two "last" fragments, or a fragment beyond an
// already-seen last) poison the whole key so a spoofed fragment can't corrupt a
// real datagram. Completed/poisoned keys expire after assembleMs so their
// offsets can be reused when the sender's counter wraps.
//
// Single-threaded: driven only from the engine's Qt event loop (the wan socket
// readyRead slot), so no locking is needed — unlike the Rust version, which
// guards against a concurrent sweeper task.

#ifndef QQDNS_REASSEMBLY_H
#define QQDNS_REASSEMBLY_H

#include <QByteArray>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace amnezia::qqdns {

class DataHandler
{
public:
    // `offsetsSize` is the offset space (kTotalDataOffset); `assembleMs` is how
    // long a partial/completed key lives before its offset may be reused.
    DataHandler(int offsetsSize, qint64 assembleMs);

    struct Result {
        bool complete = false;
        QByteArray joined; // valid only when complete
    };

    // Feed one fragment. `nowMs` is a monotonic millisecond clock. Returns
    // complete=true with the joined base32 when this fragment finishes the
    // datagram; otherwise complete=false (stored / duplicate / rejected).
    Result newDataEvent(quint32 key, int fragmentPart, bool lastFragment, const QByteArray &data,
                        qint64 nowMs);

    // Optional periodic hygiene: clear slots older than assembleMs. Correctness
    // does not depend on it (every access lazily expires its own key first).
    void sweep(qint64 nowMs);

private:
    static constexpr int kNumSlots = 64;

    enum class Kind { Empty, Assembling, Done, Rejected };

    struct Assembly {
        std::array<QByteArray, kNumSlots> parts;
        std::array<bool, kNumSlots> present {}; // parts[i] filled?
        int recNums = 0;
        int biggestIndexPlusOne = 0;
        bool seenLastFragment = false;
    };

    struct Slot {
        Kind kind = Kind::Empty;
        qint64 createdAt = 0;
        std::unique_ptr<Assembly> asm_; // only for Assembling
    };

    qint64 m_assembleMs;
    // std::vector (not QVector/QList): Slot holds a unique_ptr, so it is
    // move-only, and Qt's copy-on-write containers require a copyable value type.
    std::vector<Slot> m_slots;
};

} // namespace amnezia::qqdns

#endif // QQDNS_REASSEMBLY_H
