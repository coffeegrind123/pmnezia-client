#include "wireguardKeys.h"

#include <QByteArray>
#include <QtGlobal>

#include <openssl/evp.h>
#include <openssl/rand.h>

namespace amnezia
{

WireguardKeyPair generateWireguardKeyPair()
{
    constexpr size_t keyLength = 32;

    WireguardKeyPair keys;

    unsigned char seed[keyLength];
    if (RAND_priv_bytes(seed, keyLength) <= 0) {
        return keys;
    }

    EVP_PKEY *pKey = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, seed, keyLength);
    if (pKey == nullptr) {
        return keys;
    }

    size_t keySize = keyLength;

    unsigned char priv[keyLength];
    if (EVP_PKEY_get_raw_private_key(pKey, priv, &keySize) > 0) {
        keys.privateKey = QByteArray(reinterpret_cast<char *>(priv), static_cast<int>(keySize)).toBase64();
    }

    keySize = keyLength;
    unsigned char pub[keyLength];
    if (EVP_PKEY_get_raw_public_key(pKey, pub, &keySize) > 0) {
        keys.publicKey = QByteArray(reinterpret_cast<char *>(pub), static_cast<int>(keySize)).toBase64();
    }

    EVP_PKEY_free(pKey);
    return keys;
}

} // namespace amnezia
