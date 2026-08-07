// SPDX-License-Identifier: GPL-3.0-or-later

#include "dnsframing.h"

namespace amnezia::qqdns {

QList<QByteArray> labelDomain(const QByteArray &domain)
{
    QList<QByteArray> out;
    for (const QByteArray &label : domain.split('.')) {
        if (!label.isEmpty()) {
            out.append(label.toLower());
        }
    }
    return out;
}

QByteArray encodeQname(const QByteArray &domain)
{
    QByteArray out;
    out.reserve(domain.size() + 2);
    for (const QByteArray &label : domain.split('.')) {
        if (!label.isEmpty()) {
            out.append(static_cast<char>(label.size() & 0xff));
            out.append(label);
        }
    }
    out.append('\0');
    return out;
}

QByteArray insertDots(const QByteArray &data, int maxSub)
{
    QByteArray out;
    out.reserve(data.size() + data.size() / (maxSub > 0 ? maxSub : 1) + 1);
    int i = 0;
    const int n = data.size();
    while (i < n) {
        const int end = qMin(i + maxSub, n);
        const int segLen = end - i;
        out.append(static_cast<char>(segLen & 0xff));
        out.append(data.constData() + i, segLen);
        i = end;
    }
    return out;
}

static void appendBE16(QByteArray &b, quint16 v)
{
    b.append(static_cast<char>((v >> 8) & 0xff));
    b.append(static_cast<char>(v & 0xff));
}

QByteArray buildDnsQuery(const QByteArray &qnameEncoded, quint16 qId, quint16 qtype)
{
    QByteArray msg;
    msg.reserve(12 + qnameEncoded.size() + 4);
    appendBE16(msg, qId);
    appendBE16(msg, 0x0100); // flags: recursion desired
    appendBE16(msg, 1);      // QDCOUNT
    appendBE16(msg, 0);      // ANCOUNT
    appendBE16(msg, 0);      // NSCOUNT
    appendBE16(msg, 0);      // ARCOUNT
    msg.append(qnameEncoded);
    appendBE16(msg, qtype);
    appendBE16(msg, 1); // QCLASS = IN
    return msg;
}

// Walk one uncompressed question; fills labels/qtype/nextQuestion. Returns false
// on malformed input.
static bool handleQuestion(const QByteArray &data, int offset, QList<QByteArray> &labels,
                           quint16 &qtype, int &nextQuestion)
{
    const int lenData = data.size();
    const auto u = [&](int i) { return static_cast<quint8>(data[i]); };
    while (offset < lenData) {
        const int labelLen = u(offset);
        if (labelLen == 0) {
            if (offset + 5 > lenData) {
                return false;
            }
            qtype = static_cast<quint16>((u(offset + 1) << 8) | u(offset + 2));
            const quint16 qclass = static_cast<quint16>((u(offset + 3) << 8) | u(offset + 4));
            if (qclass != 1) {
                return false;
            }
            nextQuestion = offset + 5;
            return true;
        }
        if (labelLen > 63) {
            return false;
        }
        const int start = offset + 1;
        offset = start + labelLen;
        if (offset > lenData) {
            return false;
        }
        labels.append(data.mid(start, labelLen).toLower());
    }
    return false;
}

ParsedQuery handleDnsRequest(const QByteArray &data)
{
    ParsedQuery p;
    if (data.size() < 17) {
        return p;
    }
    const auto u = [&](int i) { return static_cast<quint8>(data[i]); };
    p.qid = static_cast<quint16>((u(0) << 8) | u(1));
    p.qflags = static_cast<quint16>((u(2) << 8) | u(3));
    const quint16 qdcount = static_cast<quint16>((u(4) << 8) | u(5));
    if (qdcount != 1) {
        return p;
    }
    if (p.qflags & 0x8000) { // QR set => a response, not a query
        return p;
    }
    if (!handleQuestion(data, 12, p.labels, p.qtype, p.nextQuestion)) {
        return p;
    }
    p.ok = true;
    return p;
}

QByteArray createNoerrorEmptyResponse(quint16 qid, quint16 qflags, const QByteArray &question)
{
    const quint16 rflags = static_cast<quint16>(
            0x8400 | (qflags & 0x7910) | (((qflags & 0x7800) != 0) ? 1 : 0) << 2);
    QByteArray msg;
    msg.reserve(12 + question.size());
    appendBE16(msg, qid);
    appendBE16(msg, rflags);
    appendBE16(msg, 1); // QDCOUNT
    appendBE16(msg, 0); // ANCOUNT
    appendBE16(msg, 0); // NSCOUNT
    appendBE16(msg, 0); // ARCOUNT
    msg.append(question);
    return msg;
}

int matchRecvSuffix(const QList<QByteArray> &labels, const QList<QList<QByteArray>> &recvDomains)
{
    for (const QList<QByteArray> &rd : recvDomains) {
        const int n = rd.size();
        if (n == 0 || labels.size() < n) {
            continue;
        }
        bool match = true;
        for (int i = 0; i < n; ++i) {
            if (labels[labels.size() - n + i] != rd[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            return n;
        }
    }
    return -1;
}

} // namespace amnezia::qqdns
