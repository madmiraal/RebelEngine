// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "multiplayer_peer_gdnative.h"

MultiplayerPeerGDNative::MultiplayerPeerGDNative() {
    interface = nullptr;
}

MultiplayerPeerGDNative::~MultiplayerPeerGDNative() {}

void MultiplayerPeerGDNative::set_native_multiplayer_peer(
    const rebel_net_multiplayer_peer* p_interface
) {
    interface = p_interface;
}

Error MultiplayerPeerGDNative::get_packet(
    const uint8_t** r_buffer,
    int& r_buffer_size
) {
    ERR_FAIL_COND_V(interface == nullptr, ERR_UNCONFIGURED);
    return (Error
    )interface->get_packet(interface->data, r_buffer, &r_buffer_size);
}

Error MultiplayerPeerGDNative::put_packet(
    const uint8_t* p_buffer,
    int p_buffer_size
) {
    ERR_FAIL_COND_V(interface == nullptr, ERR_UNCONFIGURED);
    return (Error
    )interface->put_packet(interface->data, p_buffer, p_buffer_size);
}

int MultiplayerPeerGDNative::get_max_packet_size() const {
    ERR_FAIL_COND_V(interface == nullptr, 0);
    return interface->get_max_packet_size(interface->data);
}

int MultiplayerPeerGDNative::get_available_packet_count() const {
    ERR_FAIL_COND_V(interface == nullptr, 0);
    return interface->get_available_packet_count(interface->data);
}

/* NetworkedMultiplayerPeer */
void MultiplayerPeerGDNative::set_transfer_mode(TransferMode p_mode) {
    ERR_FAIL_COND(interface == nullptr);
    interface->set_transfer_mode(interface->data, (rebel_int)p_mode);
}

NetworkedMultiplayerPeer::TransferMode MultiplayerPeerGDNative::
    get_transfer_mode() const {
    ERR_FAIL_COND_V(interface == nullptr, TRANSFER_MODE_UNRELIABLE);
    return (TransferMode)interface->get_transfer_mode(interface->data);
}

void MultiplayerPeerGDNative::set_target_peer(int p_peer_id) {
    ERR_FAIL_COND(interface == nullptr);
    interface->set_target_peer(interface->data, p_peer_id);
}

int MultiplayerPeerGDNative::get_packet_peer() const {
    ERR_FAIL_COND_V(interface == nullptr, 0);
    return interface->get_packet_peer(interface->data);
}

bool MultiplayerPeerGDNative::is_server() const {
    ERR_FAIL_COND_V(interface == nullptr, false);
    return interface->is_server(interface->data);
}

void MultiplayerPeerGDNative::poll() {
    ERR_FAIL_COND(interface == nullptr);
    interface->poll(interface->data);
}

int MultiplayerPeerGDNative::get_unique_id() const {
    ERR_FAIL_COND_V(interface == nullptr, 0);
    return interface->get_unique_id(interface->data);
}

void MultiplayerPeerGDNative::set_refuse_new_connections(bool p_enable) {
    ERR_FAIL_COND(interface == nullptr);
    interface->set_refuse_new_connections(interface->data, p_enable);
}

bool MultiplayerPeerGDNative::is_refusing_new_connections() const {
    ERR_FAIL_COND_V(interface == nullptr, true);
    return interface->is_refusing_new_connections(interface->data);
}

NetworkedMultiplayerPeer::ConnectionStatus MultiplayerPeerGDNative::
    get_connection_status() const {
    ERR_FAIL_COND_V(interface == nullptr, CONNECTION_DISCONNECTED);
    return (ConnectionStatus)interface->get_connection_status(interface->data);
}

void MultiplayerPeerGDNative::_bind_methods() {
    ADD_PROPERTY_DEFAULT("transfer_mode", TRANSFER_MODE_UNRELIABLE);
    ADD_PROPERTY_DEFAULT("refuse_new_connections", true);
}

extern "C" {

void GDAPI rebel_net_bind_multiplayer_peer(
    rebel_object* p_obj,
    const rebel_net_multiplayer_peer* p_impl
) {
    ((MultiplayerPeerGDNative*)p_obj)->set_native_multiplayer_peer(p_impl);
}
}
