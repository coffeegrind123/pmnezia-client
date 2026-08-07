// SPDX-License-Identifier: GPL-3.0-or-later

#include "codec.h"

#include "dnsframing.h"

namespace amnezia::qqdns {

namespace {

// Lowercase RFC 4648 base32 alphabet.
const char *const kAlphabet = "abcdefghijklmnopqrstuvwxyz234567";

// Reverse lookup for decode; -1 for non-alphabet bytes (accepts both cases).
int b32Value(quint8 c)
{
    if (c >= 'a' && c <= 'z') {
        return c - 'a';
    }
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= '2' && c <= '7') {
        return c - '2' + 26;
    }
    return -1;
}

} // namespace

QByteArray b32EncodeNoPadLower(const QByteArray &data)
{
    QByteArray out;
    out.reserve((data.size() + 4) / 5 * 8);
    quint64 bits = 0;
    int nbits = 0;
    for (int i = 0; i < data.size(); ++i) {
        bits = (bits << 8) | static_cast<quint8>(data[i]);
        nbits += 8;
        while (nbits >= 5) {
            nbits -= 5;
            out.append(kAlphabet[(bits >> nbits) & 0x1f]);
        }
    }
    if (nbits > 0) {
        out.append(kAlphabet[(bits << (5 - nbits)) & 0x1f]);
    }
    return out;
}

bool b32DecodeNoPad(const QByteArray &s, QByteArray &out)
{
    out.clear();
    out.reserve(s.size() * 5 / 8 + 1);
    quint64 bits = 0;
    int nbits = 0;
    for (int i = 0; i < s.size(); ++i) {
        const int v = b32Value(static_cast<quint8>(s[i]));
        if (v < 0) {
            out.clear();
            return false;
        }
        bits = (bits << 5) | static_cast<quint64>(v);
        nbits += 5;
        if (nbits >= 8) {
            nbits -= 8;
            out.append(static_cast<char>((bits >> nbits) & 0xff));
        }
    }
    return true;
}

QByteArray numberToBase32Lower(quint32 n, int width)
{
    QByteArray result(width, '\0');
    for (int i = width - 1; i >= 0; --i) {
        result[i] = kAlphabet[n & 31];
        n >>= 5;
    }
    return result;
}

bool base32ToNumber(const QByteArray &s, quint32 &out)
{
    quint32 value = 0;
    for (int i = 0; i < s.size(); ++i) {
        const int v = b32Value(static_cast<quint8>(s[i]));
        if (v < 0) {
            return false;
        }
        value = (value << 5) + static_cast<quint32>(v);
    }
    out = value;
    return true;
}

qint64 computeMaxM(qint64 s, qint64 maxAllowed)
{
    if (maxAllowed <= 0) {
        return 0;
    }
    const qint64 q = maxAllowed / (s + 1);
    const qint64 remaining = maxAllowed - q * (s + 1);
    const qint64 r = qMax<qint64>(0, remaining - 1);
    return q * s + r;
}

bool getChunkLen(qint64 maxEncodedDomainLen, qint64 qnameEncodedLen, qint64 maxSubLen,
                 qint64 dataOffsetWidth, int &outChunkLen)
{
    const qint64 maxAllowed = maxEncodedDomainLen - qnameEncodedLen;
    const qint64 m = computeMaxM(maxSubLen, maxAllowed);
    const qint64 chunkLen = m - dataOffsetWidth - 2; // fragment header is 2 chars beyond the offset
    if (chunkLen <= 0) {
        return false;
    }
    outChunkLen = static_cast<int>(chunkLen);
    return true;
}

QList<QByteArray> getBase32FinalDomains(const QByteArray &raw, quint32 dataOffset, int sendDomainIdx,
                                        const QList<SendDomain> &sendDomains, int maxSubLen,
                                        int dataOffsetWidth, int maxEncodedDomainLen)
{
    const QByteArray data = b32EncodeNoPadLower(raw);
    const int lenData = data.size();
    const QByteArray dataOffsetBytes = numberToBase32Lower(dataOffset, dataOffsetWidth);

    QList<QByteArray> finalDomains;
    int i = 0;
    int sIndex = 0;
    bool cLoop = true;
    const int nDomains = sendDomains.size();

    while (cLoop) {
        if (i == 64) {
            // max_domain_len too small for this datagram — drop it.
            return QList<QByteArray>();
        }
        const SendDomain &sd = sendDomains[sendDomainIdx];
        sendDomainIdx = (sendDomainIdx + 1) % nDomains;

        const int end = qMin(sIndex + sd.chunkLen, lenData);
        const QByteArray chunkSlice = data.mid(sIndex, end - sIndex);
        sIndex += sd.chunkLen;

        const char fragChar = kAlphabet[i & 31];
        char magic;
        if (sIndex < lenData) {
            magic = (i & 32) ? '8' : '0';
        } else {
            cLoop = false;
            magic = (i & 32) ? '9' : '1';
        }

        QByteArray labeled;
        labeled.reserve(dataOffsetWidth + 2 + chunkSlice.size());
        labeled.append(dataOffsetBytes);
        labeled.append(fragChar);
        labeled.append(magic);
        labeled.append(chunkSlice);

        QByteArray finalDomain = insertDots(labeled, maxSubLen);
        finalDomain.append(sd.qnameEncoded);

        Q_ASSERT(finalDomain.size() <= maxEncodedDomainLen);
        Q_UNUSED(maxEncodedDomainLen);

        finalDomains.append(finalDomain);
        ++i;
    }

    return finalDomains;
}

ChunkData getChunkData(const QByteArray &data, int dataOffsetWidth)
{
    ChunkData cd;
    if (data.size() < dataOffsetWidth + 2) {
        return cd;
    }
    if (!base32ToNumber(data.left(dataOffsetWidth), cd.dataOffset)) {
        return cd;
    }

    const int fragRaw = [&]() {
        const quint8 c = static_cast<quint8>(data[dataOffsetWidth]);
        if (c >= 'a' && c <= 'z') return int(c - 'a');
        if (c >= 'A' && c <= 'Z') return int(c - 'A');
        if (c >= '2' && c <= '7') return int(c - '2' + 26);
        return -1;
    }();
    if (fragRaw < 0) {
        return cd;
    }

    const quint8 magic = static_cast<quint8>(data[dataOffsetWidth + 1]);
    switch (magic) {
    case '0': cd.fragmentPart = fragRaw; cd.lastFragment = false; break;
    case '1': cd.fragmentPart = fragRaw; cd.lastFragment = true; break;
    case '8': cd.fragmentPart = fragRaw | 32; cd.lastFragment = false; break;
    case '9': cd.fragmentPart = fragRaw | 32; cd.lastFragment = true; break;
    default: return cd; // unknown magic
    }

    cd.chunk = data.mid(dataOffsetWidth + 2);
    cd.ok = true;
    return cd;
}

} // namespace amnezia::qqdns
