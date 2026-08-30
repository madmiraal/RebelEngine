// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef WEBRTC_DATA_CHANNEL_JS_H
#define WEBRTC_DATA_CHANNEL_JS_H

#ifdef WEB_ENABLED

#include "webrtc_data_channel.h"

class WebRTCDataChannelJS : public WebRTCDataChannel {
    REBEL_OBJECT(WebRTCDataChannelJS, WebRTCDataChannel);

private:
    String _label;
    String _protocol;

    bool _was_string;
    WriteMode _write_mode;

    enum {
        PACKET_BUFFER_SIZE = 65536 - 5 // 4 bytes for the size, 1 for for type
    };

    int _js_id;
    RingBuffer<uint8_t> in_buffer;
    int queue_count;
    uint8_t packet_buffer[PACKET_BUFFER_SIZE];

    static void _on_open(void* p_obj);
    static void _on_close(void* p_obj);
    static void _on_error(void* p_obj);
    static void _on_message(
        void* p_obj,
        const uint8_t* p_data,
        int p_size,
        int p_is_string
    );

public:
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

    WebRTCDataChannelJS();
    WebRTCDataChannelJS(int js_id);
    ~WebRTCDataChannelJS() override;
};

#endif // WEB_ENABLED

#endif // WEBRTC_DATA_CHANNEL_JS_H
