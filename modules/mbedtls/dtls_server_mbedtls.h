// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef MBED_DTLS_SERVER_H
#define MBED_DTLS_SERVER_H

#include "core/io/dtls_server.h"
#include "ssl_context_mbedtls.h"

class DTLSServerMbedTLS : public DTLSServer {
private:
    static DTLSServer* _create_func();
    Ref<CryptoKey> _key;
    Ref<X509Certificate> _cert;
    Ref<X509Certificate> _ca_chain;
    Ref<CookieContextMbedTLS> _cookies;

public:
    static void initialize();
    static void finalize();

    Error setup(
        Ref<CryptoKey> p_key,
        Ref<X509Certificate> p_cert,
        Ref<X509Certificate> p_ca_chain = Ref<X509Certificate>()
    ) override;
    void stop() override;
    Ref<PacketPeerDTLS> take_connection(Ref<PacketPeerUDP> p_peer) override;

    DTLSServerMbedTLS();
    ~DTLSServerMbedTLS() override;
};

#endif // MBED_DTLS_SERVER_H
