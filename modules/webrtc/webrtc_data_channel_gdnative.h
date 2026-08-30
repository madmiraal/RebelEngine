// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef WEBRTC_DATA_CHANNEL_GDNATIVE_H
#define WEBRTC_DATA_CHANNEL_GDNATIVE_H

#ifdef WEBRTC_GDNATIVE_ENABLED

#include "modules/gdnative/include/net/rebel_net.h"
#include "webrtc_data_channel.h"

class WebRTCDataChannelGDNative : public WebRTCDataChannel {
    REBEL_OBJECT(WebRTCDataChannelGDNative, WebRTCDataChannel);

protected:
    static void _bind_methods();

private:
    const rebel_net_webrtc_data_channel* interface;

public:
    void set_native_webrtc_data_channel(
        const rebel_net_webrtc_data_channel* p_impl
    );

    void set_write_mode(WriteMode mode) override;
    WriteMode get_write_mode() const override;
    bool was_string_packet() const override;

    ChannelState get_ready_state() const override;
    String get_label() const override;
    bool is_ordered() const override;
    int get_id() const override;
    int get_max_packet_life_time() const override;
    int get_max_retransmits() const override;
    String get_protocol() const override;
    bool is_negotiated() const override;
    int get_buffered_amount() const override;

    Error poll() override;
    void close() override;

    /** Inherited from PacketPeer: **/
    int get_available_packet_count() const override;
    Error get_packet(const uint8_t** r_buffer, int& r_buffer_size) override;
    Error put_packet(const uint8_t* p_buffer, int p_buffer_size) override;

    int get_max_packet_size() const override;

    WebRTCDataChannelGDNative();
    ~WebRTCDataChannelGDNative() override;
};

#endif // WEBRTC_GDNATIVE_ENABLED

#endif // WEBRTC_DATA_CHANNEL_GDNATIVE_H
