#ifndef WIREGUARDKEYS_H
#define WIREGUARDKEYS_H

#include <QString>

namespace amnezia
{

// A freshly generated Curve25519 key pair, base64-encoded the way WireGuard
// and AmneziaWG configs carry them. Both fields are empty if generation failed.
struct WireguardKeyPair
{
    QString privateKey;
    QString publicKey;
};

WireguardKeyPair generateWireguardKeyPair();

} // namespace amnezia

#endif // WIREGUARDKEYS_H
