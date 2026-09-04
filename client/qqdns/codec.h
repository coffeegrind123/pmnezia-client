// SPDX-License-Identifier: GPL-3.0-or-later
//
// Wire codec for the QQ-DNS transport — a faithful C++ port of the Rust
// reference (coffeeblack-vpn `src/qqdns/codec.rs`) / QQ-Tunnel's `base32.py` +
// `data_cap.py`. Turns a raw UDP datagram into a list of DNS-label-encoded
// QNAMEs (one per fragment) and back. Byte-compatible with the reference so
// this client interoperates on the wire with the coffeeblack-vpn server.
//
// One fragment's data labels, before the send-domain suffix is appended
// (all chars lowercase base32, a-z2-7):
//
//   [ dataOffset : WIDTH ][ frag : 1 ][ magic : 1 ][ chunk base32 data … ]
//
//   dataOffset — per-datagram id (round-robins mod 2^(5*WIDTH)); reassembly key
//   frag       — low 5 bits of the fragment index
//   magic      — bit 5 of the index + last-fragment flag: 0/1/8/9
//   chunk      — a slice of the whole datagram's base32 encoding

#ifndef QQDNS_CODEC_H
#define QQDNS_CODEC_H

#include <QByteArray>
#include <QList>

#include <cstdint>

namespace amnezia::qqdns {

constexpr int kDataOffsetWidth = 3;
constexpr quint32 kTotalDataOffset = 1u << (5 * kDataOffsetWidth); // 32768

QByteArray b32EncodeNoPadLower(const QByteArray &data);
// Returns false (and leaves `out` empty) on any non-alphabet byte.
bool b32DecodeNoPad(const QByteArray &s, QByteArray &out);

QByteArray numberToBase32Lower(quint32 n, int width);
// Returns false on any non-alphabet byte.
bool base32ToNumber(const QByteArray &s, quint32 &out);

// Maximum m such that m + ceil(m/s) <= maxAllowed (signed, mirrors the Rust).
qint64 computeMaxM(qint64 s, qint64 maxAllowed);

// How many base32 data chars fit in one QNAME alongside the send-domain suffix
// and the 5-char per-fragment header. Returns false if nothing fits.
bool getChunkLen(qint64 maxEncodedDomainLen, qint64 qnameEncodedLen, qint64 maxSubLen,
                 qint64 dataOffsetWidth, int &outChunkLen);

// A send domain paired with the chunk length it can carry.
struct SendDomain {
    QByteArray qnameEncoded; // wire-encoded QNAME ending in a zero byte
    int chunkLen = 0;
};

// Encode `data` to base32, split it across fragments (round-robining
// sendDomains from sendDomainIdx), prepend each fragment header, dot-segment
// into DNS labels, append the send-domain QNAME. Returns the wire-encoded QNAME
// bytes per fragment. Empty if the datagram needs > 64 fragments (dropped,
// matching the reference).
QList<QByteArray> getBase32FinalDomains(const QByteArray &data, quint32 dataOffset,
                                        int sendDomainIdx, const QList<SendDomain> &sendDomains,
                                        int maxSubLen, int dataOffsetWidth, int maxEncodedDomainLen);

// One fragment's decoded header + (still-base32) payload.
struct ChunkData {
    quint32 dataOffset = 0;
    int fragmentPart = 0;
    bool lastFragment = false;
    QByteArray chunk;
    bool ok = false;
};

// `data` is the concatenation of the received QNAME's labels with the
// send-domain suffix already stripped (the dot-free offset|frag|magic|chunk).
ChunkData getChunkData(const QByteArray &data, int dataOffsetWidth);

} // namespace amnezia::qqdns

#endif // QQDNS_CODEC_H
