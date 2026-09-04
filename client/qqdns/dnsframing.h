// SPDX-License-Identifier: GPL-3.0-or-later
//
// DNS wire framing for the QQ-DNS transport — a faithful C++ port of the
// Rust reference (coffeeblack-vpn `src/qqdns/dns.rs`), which is in turn a port of
// QQ-Tunnel's `utility/dns.py`. Minimal, allocation-light DNS message handling:
// build a data-bearing query, parse an inbound query far enough to recover its
// labels, and synthesise the NOERROR/empty response the authoritative side
// always returns (tunnel data rides in the QNAME of queries in each direction,
// never in answers).

#ifndef QQDNS_DNSFRAMING_H
#define QQDNS_DNSFRAMING_H

#include <QByteArray>
#include <QList>

#include <cstdint>

namespace amnezia::qqdns {

// Split a domain into its non-empty, lowercased labels.
QList<QByteArray> labelDomain(const QByteArray &domain);

// Wire-encode a domain as length-prefixed labels terminated by a zero byte.
QByteArray encodeQname(const QByteArray &domain);

// Chunk `data` into <= maxSub byte labels, each length-prefixed — the DNS-label
// encoding of the data portion of a QNAME (no terminating zero; the send-domain
// QNAME is appended after by the codec).
QByteArray insertDots(const QByteArray &data, int maxSub);

// Build a standard recursion-desired query for a wire-encoded QNAME (must end
// in a zero byte).
QByteArray buildDnsQuery(const QByteArray &qnameEncoded, quint16 qId, quint16 qtype);

// A parsed inbound query — the fields the receive path needs.
struct ParsedQuery {
    quint16 qid = 0;
    quint16 qflags = 0;
    QList<QByteArray> labels; // lowercased label bytes, no length prefix
    quint16 qtype = 0;
    int nextQuestion = 0; // one past the question section
    bool ok = false;      // false => malformed / not a query
};

// Parse a single-question query message. Rejects responses (QR set),
// multi-question messages, and truncated input (ok=false).
ParsedQuery handleDnsRequest(const QByteArray &data);

// Echo the question with QR=1, AA=1, RCODE=0 (4 for a non-standard opcode) and
// no answer records — what the authoritative side replies to every tunnel query.
QByteArray createNoerrorEmptyResponse(quint16 qid, quint16 qflags, const QByteArray &question);

// If `labels` ends with one of `recvDomains`, return the number of suffix labels
// matched (so the caller can strip them); -1 if none match.
int matchRecvSuffix(const QList<QByteArray> &labels, const QList<QList<QByteArray>> &recvDomains);

} // namespace amnezia::qqdns

#endif // QQDNS_DNSFRAMING_H
