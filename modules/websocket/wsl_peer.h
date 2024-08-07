// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef WSLPEER_H
#define WSLPEER_H

#ifndef WEB_ENABLED

#include "core/error_list.h"
#include "core/io/packet_peer.h"
#include "core/io/stream_peer_tcp.h"
#include "core/ring_buffer.h"
#include "packet_buffer.h"
#include "websocket_peer.h"
#include "wslay/wslay.h"

#define WSL_MAX_HEADER_SIZE 4096

class WSLPeer : public WebSocketPeer {
    GDCIIMPL(WSLPeer, WebSocketPeer);

public:
    struct PeerData {
        bool polling;
        bool destroy;
        bool valid;
        bool is_server;
        bool closing;
        void* obj;
        void* peer;
        Ref<StreamPeer> conn;
        Ref<StreamPeerTCP> tcp;
        int id;
        wslay_event_context_ptr ctx;

        PeerData() {
            polling   = false;
            destroy   = false;
            valid     = false;
            is_server = false;
            id        = 1;
            ctx       = nullptr;
            obj       = nullptr;
            closing   = false;
            peer      = nullptr;
        }
    };

    static String compute_key_response(String p_key);
    static String generate_key();

private:
    static bool _wsl_poll(struct PeerData* p_data);
    static void _wsl_destroy(struct PeerData** p_data);

    struct PeerData* _data;
    uint8_t _is_string;
    // Our packet info is just a boolean (is_string), using uint8_t for it.
    PacketBuffer<uint8_t> _in_buffer;

    PoolVector<uint8_t> _packet_buffer;

    WriteMode write_mode;

    int _out_buf_size;
    int _out_pkt_size;

public:
    int close_code;
    String close_reason;
    void poll(); // Used by client and server.

    virtual int get_available_packet_count() const;
    virtual Error get_packet(const uint8_t** r_buffer, int& r_buffer_size);
    virtual Error put_packet(const uint8_t* p_buffer, int p_buffer_size);

    virtual int get_max_packet_size() const {
        return _packet_buffer.size();
    };

    virtual int get_current_outbound_buffered_amount() const;

    virtual void close_now();
    virtual void close(int p_code = 1000, String p_reason = "");
    virtual bool is_connected_to_host() const;
    virtual IP_Address get_connected_host() const;
    virtual uint16_t get_connected_port() const;

    virtual WriteMode get_write_mode() const;
    virtual void set_write_mode(WriteMode p_mode);
    virtual bool was_string_packet() const;
    virtual void set_no_delay(bool p_enabled);

    void make_context(
        PeerData* p_data,
        unsigned int p_in_buf_size,
        unsigned int p_in_pkt_size,
        unsigned int p_out_buf_size,
        unsigned int p_out_pkt_size
    );
    Error parse_message(const wslay_event_on_msg_recv_arg* arg);
    void invalidate();

    WSLPeer();
    ~WSLPeer();
};

#endif // WEB_ENABLED

#endif // LSWPEER_H
