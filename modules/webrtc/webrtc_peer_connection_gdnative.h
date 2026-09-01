// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef WEBRTC_PEER_CONNECTION_GDNATIVE_H
#define WEBRTC_PEER_CONNECTION_GDNATIVE_H

#ifdef WEBRTC_GDNATIVE_ENABLED

#include "modules/gdnative/include/net/rebel_net.h"
#include "webrtc_peer_connection.h"

class WebRTCPeerConnectionGDNative : public WebRTCPeerConnection {
    GDCLASS(WebRTCPeerConnectionGDNative, WebRTCPeerConnection);

protected:
    static void _bind_methods();
    static WebRTCPeerConnection* _create();

private:
    static const rebel_net_webrtc_library* default_library;
    const rebel_net_webrtc_peer_connection* interface;

public:
    static Error set_default_library(const rebel_net_webrtc_library* p_library);

    static void make_default() {
        WebRTCPeerConnection::_create = WebRTCPeerConnectionGDNative::_create;
    }

    void set_native_webrtc_peer_connection(
        const rebel_net_webrtc_peer_connection* p_impl
    );

    ConnectionState get_connection_state() const override;

    Error initialize(Dictionary p_config = Dictionary()) override;
    Ref<WebRTCDataChannel> create_data_channel(
        String p_label,
        Dictionary p_options = Dictionary()
    ) override;
    Error create_offer() override;
    Error set_remote_description(String type, String sdp) override;
    Error set_local_description(String type, String sdp) override;
    Error add_ice_candidate(
        String sdpMidName,
        int sdpMlineIndexName,
        String sdpName
    ) override;
    Error poll() override;
    void close() override;

    WebRTCPeerConnectionGDNative();
    ~WebRTCPeerConnectionGDNative() override;
};

#endif // WEBRTC_GDNATIVE_ENABLED

#endif // WEBRTC_PEER_CONNECTION_GDNATIVE_H
