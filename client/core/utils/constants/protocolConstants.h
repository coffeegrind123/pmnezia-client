#ifndef PROTOCOLCONSTANTS_H
#define PROTOCOLCONSTANTS_H

namespace amnezia
{

    namespace protocols
    {

        namespace dns
        {
            constexpr char amneziaDnsIp[] = "172.29.172.254";
        }



        namespace xray
        {
            constexpr char serverConfigPath[] = "/opt/amnezia/xray/server.json";
            constexpr char uuidPath[] = "/opt/amnezia/xray/xray_uuid.key";
            constexpr char PublicKeyPath[] = "/opt/amnezia/xray/xray_public.key";
            constexpr char PrivateKeyPath[] = "/opt/amnezia/xray/xray_private.key";
            constexpr char shortidPath[] = "/opt/amnezia/xray/xray_short_id.key";
            constexpr char xhttpPathFile[] = "/opt/amnezia/xray/xray_xhttp_path.key";
            constexpr char defaultSite[] = "www.googletagmanager.com";

            constexpr char defaultPort[] = "443";
            constexpr char defaultLocalProxyPort[] = "10808";
            constexpr char defaultLocalAddr[] = "10.33.0.2";
            constexpr char defaultLocalListenAddr[] = "127.0.0.1";

            constexpr char defaultSecurity[] = "reality";
            constexpr char defaultFlow[] = "xtls-rprx-vision";
            constexpr char defaultTransport[] = "raw";
            constexpr char defaultFingerprint[] = "chrome";
            constexpr char defaultSni[] = "www.googletagmanager.com";
            constexpr char defaultAlpn[] = "h2";

            constexpr char defaultXhttpMode[] = "Auto";
            constexpr char defaultXhttpUplinkMethod[] = "POST";
            constexpr char defaultXhttpSessionPlacement[] = "Path";
            constexpr char defaultXhttpSessionKey[] = "";
            constexpr char defaultXhttpSeqPlacement[] = "Path";
            constexpr char defaultXhttpUplinkDataPlacement[] = "Body";

            constexpr char defaultXhttpHost[] = "www.googletagmanager.com";
            constexpr char defaultXhttpUplinkChunkSize[] = "0";
            constexpr char defaultXhttpScMaxEachPostBytesMin[] = "1";
            constexpr char defaultXhttpScMaxEachPostBytesMax[] = "100";
            constexpr char defaultXhttpScMinPostsIntervalMsMin[] = "100";
            constexpr char defaultXhttpScMinPostsIntervalMsMax[] = "800";
            constexpr char defaultXhttpScStreamUpServerSecsMin[] = "1";
            constexpr char defaultXhttpScStreamUpServerSecsMax[] = "100";

            constexpr char defaultXPaddingPlacement[] = "Cookie";
            constexpr char defaultXPaddingMethod[] = "Repeat-x";
            constexpr char defaultXPaddingKey[] = "x_padding";
            constexpr char defaultXPaddingHeader[] = "X-Padding";
            constexpr char defaultXPaddingBytesMin[] = "1";
            constexpr char defaultXPaddingBytesMax[] = "256";

            constexpr char defaultMkcpTti[] = "50";
            constexpr char defaultMkcpUplinkCapacity[] = "5";
            constexpr char defaultMkcpDownlinkCapacity[] = "20";
            constexpr char defaultMkcpReadBufferSize[] = "2";
            constexpr char defaultMkcpWriteBufferSize[] = "2";

            constexpr char outbounds[] = "outbounds";
            constexpr char inbounds[] = "inbounds";
            constexpr char settings[] = "settings";
            constexpr char streamSettings[] = "streamSettings";
            constexpr char vnext[] = "vnext";
            constexpr char users[] = "users";
            constexpr char servers[] = "servers";
            constexpr char clients[] = "clients";
            constexpr char id[] = "id";
            constexpr char port[] = "port";
            constexpr char address[] = "address";
            constexpr char flow[] = "flow";
            constexpr char encryption[] = "encryption";
            constexpr char network[] = "network";
            constexpr char security[] = "security";
            constexpr char realitySettings[] = "realitySettings";
            constexpr char serverNames[] = "serverNames";
            constexpr char serverName[] = "serverName";
            constexpr char publicKey[] = "publicKey";
            constexpr char shortId[] = "shortId";
            constexpr char fingerprint[] = "fingerprint";
            constexpr char spiderX[] = "spiderX";
            constexpr char user[] = "user";
            constexpr char pass[] = "pass";
        }


        namespace wireguard
        {
            // Config file keys ([Interface] / [Peer] sections) - case-sensitive
            constexpr char PrivateKey[] = "PrivateKey";
            constexpr char Address[] = "Address";
            constexpr char PublicKey[] = "PublicKey";
            constexpr char PresharedKey[] = "PresharedKey";
            constexpr char PreSharedKey[] = "PreSharedKey";
            constexpr char AllowedIPs[] = "AllowedIPs";
            constexpr char Endpoint[] = "Endpoint";
            constexpr char PersistentKeepalive[] = "PersistentKeepalive";
            constexpr char MTU[] = "MTU";

            constexpr char defaultSubnetAddress[] = "10.8.1.0";
            constexpr char defaultSubnetMask[] = "255.255.255.0";
            constexpr char defaultSubnetCidr[] = "24";

            constexpr char defaultPort[] = "51820";
            constexpr char defaultPersistentKeepAlive[] = "25";

#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS) || defined(MACOS_NE)
            constexpr char defaultMtu[] = "1280";
#else
            constexpr char defaultMtu[] = "1376";
#endif
            constexpr char serverConfigPath[] = "/opt/amnezia/wireguard/wg0.conf";
            constexpr char serverPublicKeyPath[] = "/opt/amnezia/wireguard/wireguard_server_public_key.key";
            constexpr char serverPskKeyPath[] = "/opt/amnezia/wireguard/wireguard_psk.key";

        }


        namespace awg
        {
            constexpr char defaultPort[] = "55424";
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS) || defined(MACOS_NE)
            constexpr char defaultMtu[] = "1280";
#else
            constexpr char defaultMtu[] = "1376";
#endif

            constexpr char serverConfigPath[] = "/opt/amnezia/awg/awg0.conf";
            constexpr char serverLegacyConfigPath[] = "/opt/amnezia/awg/wg0.conf";
            constexpr char serverPublicKeyPath[] = "/opt/amnezia/awg/wireguard_server_public_key.key";
            constexpr char serverPskKeyPath[] = "/opt/amnezia/awg/wireguard_psk.key";

            constexpr char defaultJunkPacketCount[] = "3";
            constexpr char defaultJunkPacketMinSize[] = "10";
            constexpr char defaultJunkPacketMaxSize[] = "30";
            constexpr int junkPacketSizeMin = 12;
            constexpr int initPacketJunkSizeMax = 150;
            constexpr int responsePacketJunkSizeMax = 150;
            constexpr int cookieReplyPacketJunkSizeMax = 64;

            constexpr char defaultInitPacketMagicHeader[] = "1";
            constexpr char defaultResponsePacketMagicHeader[] = "2";
            constexpr char defaultUnderloadPacketMagicHeader[] = "3";
            constexpr char defaultTransportPacketMagicHeader[] = "4";
            constexpr char defaultSpecialJunk1[] = "<r 2><b 0x858000010001000000000669636c6f756403636f6d0000010001c00c000100010000105a00044d583737>";
            constexpr char defaultSpecialJunk2[] = "";
            constexpr char defaultSpecialJunk3[] = "";
            constexpr char defaultSpecialJunk4[] = "";
            constexpr char defaultSpecialJunk5[] = "";
            constexpr char defaultPadding[] = "12";

            constexpr char awgV1_5[] = "1.5";
            constexpr char awgV2[] = "2";
            constexpr char awgV3[] = "3.1";

            constexpr char defaultContentPaddingAddition[] = "10-100";
            constexpr char defaultRekeyAfterTime[] = "100-120";
            constexpr char defaultRekeyTimeout[] = "3-7";
            constexpr char defaultRejectAfterTime[] = "150-180";
            constexpr char defaultKeepaliveTimeout[] = "5-15";
            constexpr char defaultMaxHandshakeAttempts[] = "15-20";
            constexpr char defaultPersistentKeepAlive[] = "25-35";
            constexpr char defaultRandomTrailers[] = "on";
            constexpr char defaultDisableCookies[] = "on";
            constexpr char awgBoolOn[] = "on";
            constexpr char awgBoolOff[] = "off";


        }


        namespace masterDnsVpn
        {
            // Operator-facing TUN gateway / address. Mirrors xray's defaultLocalAddr —
            // arbitrary RFC1918 range that won't collide with the operator's LAN.
            constexpr char defaultLocalAddr[] = "10.34.0.2";

            // Local SOCKS5 the bundled mdnsvpn client opens; tun2socks dials this.
            constexpr char defaultLocalProxyPort[] = "18000";

            // Default UDP port the operator's mdnsvpn server listens on (a NS-delegated
            // tunnel subdomain points public resolvers here).
            constexpr char defaultPort[] = "53";

            // JSON keys in the mdnsvpn_config_data wrapper carried over the wire.
            constexpr char domains[] = "domains";
            constexpr char encryptionMethod[] = "encryptionMethod";
            constexpr char encryptionKey[] = "encryptionKey";
            constexpr char protocolType[] = "protocolType";
            constexpr char resolvers[] = "resolvers";
            constexpr char listenPort[] = "listenPort";
            constexpr char socks5User[] = "socks5User";
            constexpr char socks5Pass[] = "socks5Pass";
            constexpr char additionalConfig[] = "additionalConfig";

            // Encryption methods accepted by the mdnsvpn core.
            // 0 = None, 1 = XOR, 2 = ChaCha20, 3..5 = AES-128/192/256-GCM.
            constexpr int encryptionMethodNone = 0;
            constexpr int encryptionMethodXor = 1;
            constexpr int encryptionMethodChaCha20 = 2;
            constexpr int encryptionMethodAes128Gcm = 3;
            constexpr int encryptionMethodAes192Gcm = 4;
            constexpr int encryptionMethodAes256Gcm = 5;

            // Only the AES-GCM methods authenticate the ciphertext. None is
            // plaintext, XOR is a repeating-key xor, and the mdnsvpn core's
            // ChaCha20 is bare ChaCha20 without Poly1305 — so on 0/1/2 an
            // active on-path attacker can tamper with tunnel payloads
            // undetected. Mirrors keys::is_aead in coffeeblack-vpn.
            constexpr bool isAeadEncryptionMethod(int method)
            {
                return method >= encryptionMethodAes128Gcm && method <= encryptionMethodAes256Gcm;
            }

            // Method fresh deployments get. Matches coffeeblack-vpn's
            // RECOMMENDED_ENCRYPTION_METHOD. Safe for any existing key: the
            // mdnsvpn core derives the AES key as sha256(rawKey) for methods 2
            // and 5, so switching an existing key from XOR needs no re-keying —
            // but it does need every distributed client config re-issued, since
            // the method is baked into each one. Imported configs are therefore
            // left on whatever method the server declares.
            constexpr int recommendedEncryptionMethod = encryptionMethodAes256Gcm;
            constexpr int defaultEncryptionMethod = recommendedEncryptionMethod;
        }

        namespace qqDns
        {
            // QQ-Tunnel UDP-over-DNS transport. Unlike MasterDnsVPN it does not
            // produce a SOCKS5 tunnel: the in-process engine binds a loopback
            // UDP port and AmneziaWG runs on top of it, so QqDnsProtocol
            // composes under Awg (endpoint rewritten to 127.0.0.1:<port>).

            // Default UDP port the operator's server listens on for tunnel
            // queries (an NS-delegated subdomain points public resolvers here).
            constexpr char defaultPort[] = "53";

            // Keys of the engine blob passed to amnezia::qqdns::Engine::start.
            // The model stores the config already in this snake_case shape plus
            // an embedded "awg" object; the protocol strips "awg" out and hands
            // the rest to the engine, so no field translation is needed.
            constexpr char dnsIps[] = "dns_ips";
            constexpr char sendDomains[] = "send_domains";
            constexpr char recvDomains[] = "recv_domains";
            constexpr char sendInterfaceIp[] = "send_interface_ip";
            constexpr char receiveInterfaceIp[] = "receive_interface_ip";
            constexpr char receivePort[] = "receive_port";
            constexpr char hInPort[] = "h_in_port";
            constexpr char maxDomainLen[] = "max_domain_len";
            constexpr char maxSubLen[] = "max_sub_len";
            constexpr char retries[] = "retries";
            constexpr char sendQueryType[] = "send_query_type";
            constexpr char packetsSendIntervalMs[] = "packets_send_interval_ms";
            constexpr char packetsWaitTimeLimitMs[] = "packets_wait_time_limit_ms";
            constexpr char sendSockNumbers[] = "send_sock_numbers";

            // The embedded AmneziaWG protocol_config_data QqDnsProtocol wraps.
            constexpr char awg[] = "awg";
            constexpr char additionalConfig[] = "additionalConfig";

            // Sensible defaults matching the reference client.
            constexpr int defaultMaxDomainLen = 253;
            constexpr int defaultMaxSubLen = 63;
            constexpr int defaultRetries = 1;
            constexpr int defaultSendQueryType = 1; // A
            constexpr int defaultPacketsSendIntervalMs = 1;
            constexpr int defaultPacketsWaitTimeLimitMs = 1000;
            constexpr int defaultSendSockNumbers = 16;
        }




    } // namespace protocols
}

#endif // PROTOCOLCONSTANTS_H
