// SPDX-License-Identifier: GPL-3.0-or-later

#include "trustTunnel.h"

#include <QJsonArray>
#include <QRegularExpression>

namespace amnezia::serialization::trusttunnel
{

namespace
{

// DEEP_LINK.md tag table.
enum Tag : quint64 {
    TagVersion = 0x00,
    TagHostname = 0x01,
    TagAddress = 0x02,
    TagCustomSni = 0x03,
    TagHasIpv6 = 0x04,
    TagUsername = 0x05,
    TagPassword = 0x06,
    TagSkipVerification = 0x07,
    TagCertificate = 0x08,
    TagUpstreamProtocol = 0x09,
    TagAntiDpi = 0x0A,
    TagClientRandomPrefix = 0x0B,
    TagName = 0x0C,
    TagDnsUpstreams = 0x0D,
    TagSubscriptionUrl = 0x0E,
    TagClientRandomAuthKey = 0x0F,
};

constexpr quint8 protocolHttp2 = 0x01;
constexpr quint8 protocolHttp3 = 0x02;
constexpr int maxSupportedVersion = 2;

const QString http2 = QStringLiteral("http2");
const QString http3 = QStringLiteral("http3");

void appendVarint(QByteArray &out, quint64 v)
{
    if (v <= 0x3F) {
        out.append(char(v));
    } else if (v <= 0x3FFF) {
        const quint16 x = quint16(v) | 0x4000;
        out.append(char(x >> 8)).append(char(x & 0xFF));
    } else if (v <= 0x3FFFFFFF) {
        const quint32 x = quint32(v) | 0x80000000u;
        for (int s = 24; s >= 0; s -= 8) {
            out.append(char((x >> s) & 0xFF));
        }
    } else {
        const quint64 x = v | 0xC000000000000000ull;
        for (int s = 56; s >= 0; s -= 8) {
            out.append(char((x >> s) & 0xFF));
        }
    }
}

bool readVarint(const QByteArray &in, qsizetype &pos, quint64 &v)
{
    if (pos >= in.size()) {
        return false;
    }
    const quint8 first = quint8(in.at(pos));
    const int len = 1 << (first >> 6);
    if (pos + len > in.size()) {
        return false;
    }
    v = first & 0x3F;
    for (int i = 1; i < len; ++i) {
        v = (v << 8) | quint8(in.at(pos + i));
    }
    pos += len;
    return true;
}

void appendField(QByteArray &out, quint64 tag, const QByteArray &value)
{
    appendVarint(out, tag);
    appendVarint(out, quint64(value.size()));
    out.append(value);
}

void appendString(QByteArray &out, quint64 tag, const QString &value)
{
    appendField(out, tag, value.toUtf8());
}

void appendBool(QByteArray &out, quint64 tag, bool value)
{
    appendField(out, tag, QByteArray(1, value ? '\x01' : '\x00'));
}

bool fail(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
    return false;
}

std::optional<bool> decodeBool(const QByteArray &v)
{
    if (v.size() != 1 || quint8(v.at(0)) > 1) {
        return std::nullopt;
    }
    return v.at(0) == '\x01';
}

bool isHex(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("^(?:[0-9A-Fa-f]{2})*$"));
    return re.match(s).hasMatch();
}

// "hex[/hexmask]" as client_random; the mask, when present, must be non-empty
// (trusttunnel_client refuses "prefix/").
bool isValidClientRandom(const QString &s)
{
    if (s.isEmpty()) {
        return true;
    }
    const int slash = s.indexOf('/');
    if (slash < 0) {
        return isHex(s);
    }
    const QString mask = s.mid(slash + 1);
    return isHex(s.left(slash)) && !mask.isEmpty() && isHex(mask);
}

QJsonArray toJsonArray(const QStringList &list)
{
    QJsonArray a;
    for (const QString &s : list) {
        a.append(s);
    }
    return a;
}

QStringList toStringList(const QJsonValue &v)
{
    QStringList out;
    for (const QJsonValue &x : v.toArray()) {
        const QString s = x.toString();
        if (!s.isEmpty()) {
            out.append(s);
        }
    }
    return out;
}

} // namespace

QString Endpoint::missingRequiredField() const
{
    // DEEP_LINK.md v2: the four are optional only when a subscription URL
    // supplies them instead.
    if (!subscriptionUrl.isEmpty()) {
        return {};
    }
    if (hostname.isEmpty()) {
        return QStringLiteral("hostname");
    }
    if (addresses.isEmpty()) {
        return QStringLiteral("addresses");
    }
    if (username.isEmpty()) {
        return QStringLiteral("username");
    }
    if (password.isEmpty()) {
        return QStringLiteral("password");
    }
    return {};
}

QJsonObject Endpoint::toJson() const
{
    QJsonObject o;
    o[QStringLiteral("hostname")] = hostname;
    o[QStringLiteral("addresses")] = toJsonArray(addresses);
    if (!customSni.isEmpty()) {
        o[QStringLiteral("custom_sni")] = customSni;
    }
    o[QStringLiteral("has_ipv6")] = hasIpv6;
    o[QStringLiteral("username")] = username;
    o[QStringLiteral("password")] = password;
    o[QStringLiteral("skip_verification")] = skipVerification;
    if (!certificate.isEmpty()) {
        o[QStringLiteral("certificate")] = certificate;
    }
    o[QStringLiteral("upstream_protocol")] = upstreamProtocol;
    o[QStringLiteral("anti_dpi")] = antiDpi;
    if (!clientRandom.isEmpty()) {
        o[QStringLiteral("client_random")] = clientRandom;
    }
    if (!name.isEmpty()) {
        o[QStringLiteral("name")] = name;
    }
    if (!dnsUpstreams.isEmpty()) {
        o[QStringLiteral("dns_upstreams")] = toJsonArray(dnsUpstreams);
    }
    if (!subscriptionUrl.isEmpty()) {
        o[QStringLiteral("subscription_url")] = subscriptionUrl;
    }
    if (!clientRandomAuthKey.isEmpty()) {
        o[QStringLiteral("client_random_auth_key")] = clientRandomAuthKey;
    }
    return o;
}

Endpoint Endpoint::fromJson(const QJsonObject &json)
{
    Endpoint e;
    e.hostname = json.value(QStringLiteral("hostname")).toString();
    e.addresses = toStringList(json.value(QStringLiteral("addresses")));
    e.customSni = json.value(QStringLiteral("custom_sni")).toString();
    e.hasIpv6 = json.value(QStringLiteral("has_ipv6")).toBool(true);
    e.username = json.value(QStringLiteral("username")).toString();
    e.password = json.value(QStringLiteral("password")).toString();
    e.skipVerification = json.value(QStringLiteral("skip_verification")).toBool(false);
    e.certificate = json.value(QStringLiteral("certificate")).toString();
    e.upstreamProtocol = json.value(QStringLiteral("upstream_protocol")).toString(http2);
    if (e.upstreamProtocol != http3) {
        e.upstreamProtocol = http2;
    }
    e.antiDpi = json.value(QStringLiteral("anti_dpi")).toBool(false);
    // The endpoint's own export spells this client_random_prefix.
    e.clientRandom = json.value(QStringLiteral("client_random")).toString();
    if (e.clientRandom.isEmpty()) {
        e.clientRandom = json.value(QStringLiteral("client_random_prefix")).toString();
    }
    e.name = json.value(QStringLiteral("name")).toString();
    e.dnsUpstreams = toStringList(json.value(QStringLiteral("dns_upstreams")));
    e.subscriptionUrl = json.value(QStringLiteral("subscription_url")).toString();
    e.clientRandomAuthKey = json.value(QStringLiteral("client_random_auth_key")).toString();
    if (!e.subscriptionUrl.isEmpty() || !e.clientRandomAuthKey.isEmpty()) {
        e.version = 2;
    }
    return e;
}

std::optional<Endpoint> decodeDeepLink(const QString &uri, QString *error)
{
    QString payload = uri.trimmed();
    if (!payload.startsWith(QStringLiteral("tt://"))) {
        fail(error, QStringLiteral("not a tt:// link"));
        return std::nullopt;
    }
    payload = payload.mid(5);
    if (payload.startsWith('?')) {
        payload = payload.mid(1);
    }

    // Strict base64url: QByteArray::fromBase64 would silently skip junk.
    static const QRegularExpression b64url(QStringLiteral("^[A-Za-z0-9_-]+=*$"));
    if (!b64url.match(payload).hasMatch()) {
        fail(error, QStringLiteral("payload is not base64url"));
        return std::nullopt;
    }
    const auto decoded = QByteArray::fromBase64Encoding(
            payload.toLatin1(), QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded) {
        fail(error, QStringLiteral("payload is not base64url"));
        return std::nullopt;
    }
    const QByteArray data = *decoded;

    Endpoint e;
    QByteArray certificateDer;
    qsizetype pos = 0;
    while (pos < data.size()) {
        quint64 tag = 0;
        quint64 len = 0;
        if (!readVarint(data, pos, tag) || !readVarint(data, pos, len)) {
            fail(error, QStringLiteral("truncated TLV header"));
            return std::nullopt;
        }
        if (len > quint64(data.size() - pos)) {
            fail(error, QStringLiteral("truncated TLV value for tag 0x%1").arg(tag, 2, 16, QChar('0')));
            return std::nullopt;
        }
        const QByteArray value = data.mid(pos, qsizetype(len));
        pos += qsizetype(len);
        const QString text = QString::fromUtf8(value);

        switch (tag) {
        case TagVersion: {
            qsizetype vp = 0;
            quint64 version = 0;
            if (!readVarint(value, vp, version)) {
                fail(error, QStringLiteral("malformed version"));
                return std::nullopt;
            }
            if (version > quint64(maxSupportedVersion)) {
                fail(error, QStringLiteral("deep-link version %1 is newer than supported (%2)")
                                    .arg(version).arg(maxSupportedVersion));
                return std::nullopt;
            }
            e.version = int(version);
            break;
        }
        case TagHostname: e.hostname = text; break;
        case TagAddress: e.addresses.append(text); break;
        case TagCustomSni: e.customSni = text; break;
        case TagUsername: e.username = text; break;
        case TagPassword: e.password = text; break;
        case TagCertificate: certificateDer = value; break;
        case TagClientRandomPrefix: e.clientRandom = text; break;
        case TagName: e.name = text; break;
        case TagSubscriptionUrl: e.subscriptionUrl = text; break;
        case TagClientRandomAuthKey: e.clientRandomAuthKey = text; break;
        case TagHasIpv6:
        case TagSkipVerification:
        case TagAntiDpi: {
            const auto b = decodeBool(value);
            if (!b) {
                fail(error, QStringLiteral("malformed boolean for tag 0x%1").arg(tag, 2, 16, QChar('0')));
                return std::nullopt;
            }
            (tag == TagHasIpv6 ? e.hasIpv6 : tag == TagSkipVerification ? e.skipVerification : e.antiDpi) = *b;
            break;
        }
        case TagUpstreamProtocol:
            if (value.size() != 1 || (quint8(value.at(0)) != protocolHttp2 && quint8(value.at(0)) != protocolHttp3)) {
                fail(error, QStringLiteral("unknown upstream protocol"));
                return std::nullopt;
            }
            e.upstreamProtocol = quint8(value.at(0)) == protocolHttp3 ? http3 : http2;
            break;
        case TagDnsUpstreams: {
            e.dnsUpstreams.clear();
            qsizetype lp = 0;
            while (lp < value.size()) {
                quint64 elen = 0;
                if (!readVarint(value, lp, elen) || elen > quint64(value.size() - lp)) {
                    fail(error, QStringLiteral("truncated dns_upstreams entry"));
                    return std::nullopt;
                }
                e.dnsUpstreams.append(QString::fromUtf8(value.mid(lp, qsizetype(elen))));
                lp += qsizetype(elen);
            }
            break;
        }
        default:
            // Unknown tags are skipped for forward compatibility (DEEP_LINK.md).
            break;
        }
    }

    if (!e.clientRandom.isEmpty() && !isValidClientRandom(e.clientRandom)) {
        fail(error, QStringLiteral("client_random_prefix is not hex[/hexmask]"));
        return std::nullopt;
    }
    if (!certificateDer.isEmpty()) {
        e.certificate = derChainToPem(certificateDer);
        if (e.certificate.isEmpty()) {
            fail(error, QStringLiteral("certificate is not a DER chain"));
            return std::nullopt;
        }
    }
    if (const QString missing = e.missingRequiredField(); !missing.isEmpty()) {
        fail(error, QStringLiteral("missing required field: %1").arg(missing));
        return std::nullopt;
    }
    return e;
}

QString encodeDeepLink(const Endpoint &e)
{
    const bool v2 = !e.subscriptionUrl.isEmpty() || !e.clientRandomAuthKey.isEmpty();

    QByteArray out;
    QByteArray version;
    appendVarint(version, v2 ? 2 : 1);
    appendField(out, TagVersion, version);
    appendString(out, TagHostname, e.hostname);
    appendString(out, TagUsername, e.username);
    appendString(out, TagPassword, e.password);
    for (const QString &a : e.addresses) {
        appendString(out, TagAddress, a);
    }
    if (!e.clientRandom.isEmpty()) {
        appendString(out, TagClientRandomPrefix, e.clientRandom);
    }
    if (!e.customSni.isEmpty()) {
        appendString(out, TagCustomSni, e.customSni);
    }
    // Defaults are omitted: has_ipv6 defaults to true, the rest to false.
    if (!e.hasIpv6) {
        appendBool(out, TagHasIpv6, false);
    }
    if (e.skipVerification) {
        appendBool(out, TagSkipVerification, true);
    }
    if (e.antiDpi) {
        appendBool(out, TagAntiDpi, true);
    }
    if (const QByteArray der = pemChainToDer(e.certificate); !der.isEmpty()) {
        appendField(out, TagCertificate, der);
    }
    if (e.upstreamProtocol == http3) {
        appendField(out, TagUpstreamProtocol, QByteArray(1, char(protocolHttp3)));
    }
    if (!e.name.isEmpty()) {
        appendString(out, TagName, e.name);
    }
    if (!e.dnsUpstreams.isEmpty()) {
        QByteArray list;
        for (const QString &d : e.dnsUpstreams) {
            const QByteArray u = d.toUtf8();
            appendVarint(list, quint64(u.size()));
            list.append(u);
        }
        appendField(out, TagDnsUpstreams, list);
    }
    if (!e.subscriptionUrl.isEmpty()) {
        appendString(out, TagSubscriptionUrl, e.subscriptionUrl);
    }
    if (!e.clientRandomAuthKey.isEmpty()) {
        appendString(out, TagClientRandomAuthKey, e.clientRandomAuthKey);
    }

    return QStringLiteral("tt://?")
            + QString::fromLatin1(out.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString derChainToPem(const QByteArray &der)
{
    QString pem;
    qsizetype pos = 0;
    while (pos < der.size()) {
        // Each certificate is one DER SEQUENCE: 0x30, then a short- or
        // long-form length.
        if (pos + 2 > der.size() || quint8(der.at(pos)) != 0x30) {
            return {};
        }
        const quint8 l0 = quint8(der.at(pos + 1));
        qsizetype header = 2;
        quint64 body = l0;
        if (l0 & 0x80) {
            const int n = l0 & 0x7F;
            if (n == 0 || n > 4 || pos + 2 + n > der.size()) {
                return {};
            }
            body = 0;
            for (int i = 0; i < n; ++i) {
                body = (body << 8) | quint8(der.at(pos + 2 + i));
            }
            header += n;
        }
        if (body > quint64(der.size() - pos - header)) {
            return {};
        }
        const qsizetype total = header + qsizetype(body);
        const QByteArray b64 = der.mid(pos, total).toBase64();
        pem += QStringLiteral("-----BEGIN CERTIFICATE-----\n");
        for (qsizetype i = 0; i < b64.size(); i += 64) {
            pem += QString::fromLatin1(b64.mid(i, 64)) + '\n';
        }
        pem += QStringLiteral("-----END CERTIFICATE-----\n");
        pos += total;
    }
    return pem;
}

QByteArray pemChainToDer(const QString &pem)
{
    static const QRegularExpression block(
            QStringLiteral("-----BEGIN CERTIFICATE-----([A-Za-z0-9+/=\\s]*)-----END CERTIFICATE-----"));
    QByteArray der;
    auto it = block.globalMatch(pem);
    while (it.hasNext()) {
        QString body = it.next().captured(1);
        body.remove(QRegularExpression(QStringLiteral("\\s")));
        const auto decoded = QByteArray::fromBase64Encoding(body.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
        if (!decoded) {
            return {};
        }
        der.append(*decoded);
    }
    return der;
}

QString tomlString(const QString &value)
{
    QString out;
    out.reserve(value.size() + 2);
    out += '"';
    for (const QChar c : value) {
        switch (c.unicode()) {
        case '"': out += QStringLiteral("\\\""); break;
        case '\\': out += QStringLiteral("\\\\"); break;
        case '\n': out += QStringLiteral("\\n"); break;
        case '\r': out += QStringLiteral("\\r"); break;
        case '\t': out += QStringLiteral("\\t"); break;
        case '\b': out += QStringLiteral("\\b"); break;
        case '\f': out += QStringLiteral("\\f"); break;
        default:
            if (c.unicode() < 0x20 || c.unicode() == 0x7F) {
                out += QStringLiteral("\\u%1").arg(uint(c.unicode()), 4, 16, QChar('0'));
            } else {
                out += c;
            }
        }
    }
    out += '"';
    return out;
}

namespace
{

QString tomlArray(const QStringList &values)
{
    QStringList quoted;
    for (const QString &v : values) {
        quoted.append(tomlString(v));
    }
    return '[' + quoted.join(QStringLiteral(", ")) + ']';
}

QString tomlBool(bool b)
{
    return b ? QStringLiteral("true") : QStringLiteral("false");
}

} // namespace

QString buildClientToml(const Endpoint &e, const SocksListener &socks)
{
    QString t;
    t += QStringLiteral("loglevel = \"info\"\n");
    t += QStringLiteral("vpn_mode = \"general\"\n");
    t += QStringLiteral("killswitch_enabled = false\n");
    t += QStringLiteral("post_quantum_group_enabled = true\n");
    t += QStringLiteral("exclusions = []\n");
    t += QStringLiteral("\n[endpoint]\n");
    t += QStringLiteral("hostname = %1\n").arg(tomlString(e.hostname));
    t += QStringLiteral("addresses = %1\n").arg(tomlArray(e.addresses));
    if (!e.customSni.isEmpty()) {
        t += QStringLiteral("custom_sni = %1\n").arg(tomlString(e.customSni));
    }
    t += QStringLiteral("has_ipv6 = %1\n").arg(tomlBool(e.hasIpv6));
    t += QStringLiteral("username = %1\n").arg(tomlString(e.username));
    t += QStringLiteral("password = %1\n").arg(tomlString(e.password));
    t += QStringLiteral("client_random = %1\n").arg(tomlString(e.clientRandom));
    t += QStringLiteral("skip_verification = %1\n").arg(tomlBool(e.skipVerification));
    t += QStringLiteral("certificate = %1\n").arg(tomlString(e.certificate));
    t += QStringLiteral("upstream_protocol = %1\n").arg(tomlString(e.upstreamProtocol == http3 ? http3 : http2));
    t += QStringLiteral("anti_dpi = %1\n").arg(tomlBool(e.antiDpi));
    t += QStringLiteral("dns_upstreams = %1\n").arg(tomlArray(e.dnsUpstreams));
    t += QStringLiteral("\n[listener.socks]\n");
    t += QStringLiteral("address = %1\n").arg(tomlString(socks.address));
    if (!socks.username.isEmpty()) {
        t += QStringLiteral("username = %1\n").arg(tomlString(socks.username));
        t += QStringLiteral("password = %1\n").arg(tomlString(socks.password));
    }
    return t;
}

// ---------------------------------------------------------------- TOML ----

namespace toml
{

namespace
{

class Parser
{
public:
    explicit Parser(const QString &text) : m_s(text) {}

    std::optional<QMap<QString, QJsonObject>> run(QString *error)
    {
        QMap<QString, QJsonObject> tables;
        tables.insert(QString(), QJsonObject());
        QString current;
        bool skippingArrayTable = false;

        while (true) {
            skipBlankAndComments();
            if (atEnd()) {
                break;
            }
            if (peek() == '[') {
                const bool arrayTable = peekAt(1) == '[';
                m_pos += arrayTable ? 2 : 1;
                QString name;
                if (!readTableName(name)) {
                    return failAt(error, QStringLiteral("malformed table header"));
                }
                if (arrayTable) {
                    if (!consume(']')) {
                        return failAt(error, QStringLiteral("malformed array-of-tables header"));
                    }
                }
                if (!endOfLine()) {
                    return failAt(error, QStringLiteral("junk after table header"));
                }
                skippingArrayTable = arrayTable;
                current = name;
                if (!arrayTable && !tables.contains(current)) {
                    tables.insert(current, QJsonObject());
                }
                continue;
            }

            QString key;
            if (!readKey(key)) {
                return failAt(error, QStringLiteral("expected a key"));
            }
            skipInlineSpace();
            if (!consume('=')) {
                return failAt(error, QStringLiteral("expected '=' after key"));
            }
            skipInlineSpace();
            QJsonValue value;
            if (!readValue(value)) {
                return failAt(error, m_valueError.isEmpty() ? QStringLiteral("malformed value") : m_valueError);
            }
            if (!endOfLine()) {
                return failAt(error, QStringLiteral("junk after value"));
            }
            if (!skippingArrayTable) {
                tables[current].insert(key, value);
            }
        }
        return tables;
    }

private:
    bool atEnd() const { return m_pos >= m_s.size(); }
    QChar peek() const { return atEnd() ? QChar() : m_s.at(m_pos); }
    QChar peekAt(qsizetype off) const { return m_pos + off < m_s.size() ? m_s.at(m_pos + off) : QChar(); }

    bool consume(QChar c)
    {
        if (peek() == c) {
            ++m_pos;
            return true;
        }
        return false;
    }

    bool startsWith(const QString &s) const { return QStringView(m_s).mid(m_pos).startsWith(s); }

    std::nullopt_t failAt(QString *error, const QString &what) const
    {
        if (error) {
            const qsizetype line = QStringView(m_s).left(m_pos).count('\n') + 1;
            *error = QStringLiteral("TOML line %1: %2").arg(line).arg(what);
        }
        return std::nullopt;
    }

    void skipInlineSpace()
    {
        while (peek() == ' ' || peek() == '\t') {
            ++m_pos;
        }
    }

    void skipComment()
    {
        if (peek() == '#') {
            while (!atEnd() && peek() != '\n') {
                ++m_pos;
            }
        }
    }

    void skipBlankAndComments()
    {
        while (!atEnd()) {
            const QChar c = peek();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++m_pos;
            } else if (c == '#') {
                skipComment();
            } else {
                break;
            }
        }
    }

    bool endOfLine()
    {
        skipInlineSpace();
        skipComment();
        if (consume('\r')) {
            return consume('\n');
        }
        return atEnd() || consume('\n');
    }

    static bool isBareKeyChar(QChar c)
    {
        return c.isLetterOrNumber() || c == '_' || c == '-';
    }

    bool readKeyPart(QString &out)
    {
        if (peek() == '"') {
            return readBasicString(out);
        }
        if (peek() == '\'') {
            return readLiteralString(out);
        }
        const qsizetype start = m_pos;
        while (!atEnd() && isBareKeyChar(peek())) {
            ++m_pos;
        }
        out = m_s.mid(start, m_pos - start);
        return !out.isEmpty();
    }

    // Dotted keys are joined with '.' and not split into sub-tables; the
    // formats read here never use them.
    bool readKey(QString &out)
    {
        QStringList parts;
        while (true) {
            QString part;
            if (!readKeyPart(part)) {
                return false;
            }
            parts.append(part);
            skipInlineSpace();
            if (!consume('.')) {
                break;
            }
            skipInlineSpace();
        }
        out = parts.join('.');
        return true;
    }

    bool readTableName(QString &out)
    {
        skipInlineSpace();
        if (!readKey(out)) {
            return false;
        }
        skipInlineSpace();
        return consume(']');
    }

    bool readValue(QJsonValue &out)
    {
        if (startsWith(QStringLiteral("\"\"\""))) {
            QString s;
            if (!readMultilineBasicString(s)) {
                return false;
            }
            out = s;
            return true;
        }
        if (startsWith(QStringLiteral("'''"))) {
            QString s;
            if (!readMultilineLiteralString(s)) {
                return false;
            }
            out = s;
            return true;
        }
        if (peek() == '"') {
            QString s;
            if (!readBasicString(s)) {
                return false;
            }
            out = s;
            return true;
        }
        if (peek() == '\'') {
            QString s;
            if (!readLiteralString(s)) {
                return false;
            }
            out = s;
            return true;
        }
        if (peek() == '[') {
            return readArray(out);
        }
        if (startsWith(QStringLiteral("true"))) {
            m_pos += 4;
            out = true;
            return true;
        }
        if (startsWith(QStringLiteral("false"))) {
            m_pos += 5;
            out = false;
            return true;
        }
        // Anchored by AnchorAtOffsetMatchOption; a "^" would only match at 0.
        static const QRegularExpression intRe(QStringLiteral("[+-]?[0-9](?:_?[0-9])*"));
        const auto m = intRe.match(m_s, m_pos, QRegularExpression::NormalMatch,
                                   QRegularExpression::AnchorAtOffsetMatchOption);
        if (m.hasMatch()) {
            bool ok = false;
            const qint64 v = m.captured(0).remove('_').toLongLong(&ok);
            if (!ok) {
                return false;
            }
            m_pos += m.capturedLength(0);
            out = double(v);
            return true;
        }
        m_valueError = QStringLiteral("unsupported value type");
        return false;
    }

    bool readArray(QJsonValue &out)
    {
        ++m_pos; // '['
        QJsonArray arr;
        while (true) {
            skipBlankAndComments();
            if (consume(']')) {
                out = arr;
                return true;
            }
            QJsonValue item;
            if (!readValue(item)) {
                return false;
            }
            arr.append(item);
            skipBlankAndComments();
            if (consume(',')) {
                continue;
            }
            skipBlankAndComments();
            if (consume(']')) {
                out = arr;
                return true;
            }
            m_valueError = QStringLiteral("expected ',' or ']' in array");
            return false;
        }
    }

    bool readEscape(QString &out)
    {
        const QChar c = peek();
        ++m_pos;
        switch (c.unicode()) {
        case 'b': out += '\b'; return true;
        case 't': out += '\t'; return true;
        case 'n': out += '\n'; return true;
        case 'f': out += '\f'; return true;
        case 'r': out += '\r'; return true;
        case 'e': out += QChar(0x1B); return true;
        case '"': out += '"'; return true;
        case '\\': out += '\\'; return true;
        case 'u':
        case 'U': {
            const int n = c == 'u' ? 4 : 8;
            if (m_pos + n > m_s.size()) {
                return false;
            }
            bool ok = false;
            const uint cp = m_s.mid(m_pos, n).toUInt(&ok, 16);
            if (!ok || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
                return false;
            }
            m_pos += n;
            const char32_t ch = cp;
            out += QString::fromUcs4(&ch, 1);
            return true;
        }
        default:
            m_valueError = QStringLiteral("invalid escape sequence");
            return false;
        }
    }

    bool readBasicString(QString &out)
    {
        ++m_pos; // '"'
        out.clear();
        while (!atEnd()) {
            const QChar c = peek();
            if (c == '"') {
                ++m_pos;
                return true;
            }
            if (c == '\n') {
                return false;
            }
            ++m_pos;
            if (c == '\\') {
                if (!readEscape(out)) {
                    return false;
                }
            } else {
                out += c;
            }
        }
        return false;
    }

    bool readLiteralString(QString &out)
    {
        ++m_pos; // '\''
        const qsizetype start = m_pos;
        while (!atEnd() && peek() != '\'' && peek() != '\n') {
            ++m_pos;
        }
        if (peek() != '\'') {
            return false;
        }
        out = m_s.mid(start, m_pos - start);
        ++m_pos;
        return true;
    }

    // A newline right after the opening delimiter is trimmed (TOML spec).
    void trimLeadingNewline()
    {
        if (startsWith(QStringLiteral("\r\n"))) {
            m_pos += 2;
        } else if (peek() == '\n') {
            ++m_pos;
        }
    }

    bool readMultilineBasicString(QString &out)
    {
        m_pos += 3;
        trimLeadingNewline();
        out.clear();
        while (!atEnd()) {
            if (startsWith(QStringLiteral("\"\"\""))) {
                m_pos += 3;
                // Up to two quotes may sit directly before the closing delimiter.
                for (int extra = 0; extra < 2 && peek() == '"'; ++extra) {
                    out += '"';
                    ++m_pos;
                }
                return true;
            }
            const QChar c = peek();
            ++m_pos;
            if (c != '\\') {
                out += c;
                continue;
            }
            // Line-ending backslash: drop the newline and leading whitespace.
            qsizetype p = m_pos;
            while (p < m_s.size() && (m_s.at(p) == ' ' || m_s.at(p) == '\t')) {
                ++p;
            }
            if (p < m_s.size() && (m_s.at(p) == '\n' || m_s.at(p) == '\r')) {
                m_pos = p;
                while (!atEnd() && peek().isSpace()) {
                    ++m_pos;
                }
                continue;
            }
            if (!readEscape(out)) {
                return false;
            }
        }
        return false;
    }

    bool readMultilineLiteralString(QString &out)
    {
        m_pos += 3;
        trimLeadingNewline();
        const qsizetype start = m_pos;
        const qsizetype end = m_s.indexOf(QStringLiteral("'''"), m_pos);
        if (end < 0) {
            return false;
        }
        m_pos = end + 3;
        qsizetype extra = 0;
        while (peek() == '\'' && extra < 2) {
            ++m_pos;
            ++extra;
        }
        out = m_s.mid(start, end - start) + QString(extra, '\'');
        return true;
    }

    const QString &m_s;
    qsizetype m_pos = 0;
    QString m_valueError;
};

} // namespace

std::optional<QMap<QString, QJsonObject>> parse(const QString &text, QString *error)
{
    return Parser(text).run(error);
}

} // namespace toml

bool looksLikeEndpointToml(const QString &text)
{
    static const QRegularExpression hostname(QStringLiteral("(?m)^\\s*hostname\\s*="));
    static const QRegularExpression addresses(QStringLiteral("(?m)^\\s*addresses\\s*="));
    static const QRegularExpression username(QStringLiteral("(?m)^\\s*username\\s*="));
    return hostname.match(text).hasMatch() && addresses.match(text).hasMatch() && username.match(text).hasMatch();
}

std::optional<Endpoint> parseToml(const QString &text, QString *error)
{
    const auto tables = toml::parse(text, error);
    if (!tables) {
        return std::nullopt;
    }

    // trusttunnel_client config: [endpoint]; endpoint export: root table.
    const QJsonObject src = tables->contains(QStringLiteral("endpoint"))
            ? tables->value(QStringLiteral("endpoint"))
            : tables->value(QString());

    const QString proto = src.value(QStringLiteral("upstream_protocol")).toString();
    if (!proto.isEmpty() && proto != http2 && proto != http3) {
        fail(error, QStringLiteral("unknown upstream_protocol \"%1\"").arg(proto));
        return std::nullopt;
    }

    Endpoint e = Endpoint::fromJson(src);
    if (!isValidClientRandom(e.clientRandom)) {
        fail(error, QStringLiteral("client_random is not hex[/hexmask]"));
        return std::nullopt;
    }
    if (!e.certificate.isEmpty() && pemChainToDer(e.certificate).isEmpty()) {
        fail(error, QStringLiteral("certificate is not a PEM chain"));
        return std::nullopt;
    }
    if (const QString missing = e.missingRequiredField(); !missing.isEmpty()) {
        fail(error, QStringLiteral("missing required field: %1").arg(missing));
        return std::nullopt;
    }
    return e;
}

} // namespace amnezia::serialization::trusttunnel
