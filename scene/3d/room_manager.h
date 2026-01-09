// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef ROOM_MANAGER_H
#define ROOM_MANAGER_H

#include "core/local_vector.h"
#include "core/math/geometry.h"
#include "core/math/vector3.h"
#include "core/node_path.h"
#include "core/object_id.h"
#include "core/ustring.h"
#include "core/vector.h"
#include "scene/3d/room.h"
#include "scene/3d/spatial.h"

class AABB;
class Camera;
class GeometryInstance;
class Light;
class MeshInstance;
class Node;
class Plane;
class Portal;
class RoomGroup;
class VisualInstance;

class RoomManager : public Spatial {
    GDCLASS(RoomManager, Spatial);

public:
    enum PVSMode {
        PVS_MODE_DISABLED,
        PVS_MODE_PARTIAL,
        PVS_MODE_FULL,
    };

#ifdef TOOLS_ENABLED
    static RoomManager* active_room_manager;
#endif // TOOLS_ENABLED

    RoomManager();

    bool rooms_get_active() const;
    void rooms_set_active(bool enabled);
    bool get_debug_sprawl() const;
    void set_debug_sprawl(bool enabled);
    real_t get_default_portal_margin() const;
    void set_default_portal_margin(real_t new_margin);
    bool get_gameplay_monitor_enabled() const;
    void set_gameplay_monitor_enabled(bool enabled);
    bool get_merge_meshes() const;
    void set_merge_meshes(bool enabled);
    int get_overlap_warning_threshold() const;
    void set_overlap_warning_threshold(int new_threshold);
    int get_portal_depth_limit() const;
    void set_portal_depth_limit(int new_limit);
    NodePath get_preview_camera_path() const;
    void set_preview_camera_path(const NodePath& new_path);
    String get_pvs_filename() const;
    void set_pvs_filename(const String& new_filename);
    PVSMode get_pvs_mode() const;
    void set_pvs_mode(PVSMode new_mode);
    real_t get_roaming_expansion_margin() const;
    void set_roaming_expansion_margin(real_t new_margin);
    NodePath get_roomlist_path() const;
    void set_roomlist_path(const NodePath& new_path);
    real_t get_room_simplify() const;
    void set_room_simplify(real_t new_value);
    bool get_show_margins() const;
    void set_show_margins(bool show);
    bool get_use_secondary_pvs() const;
    void set_use_secondary_pvs(bool enabled);

    void rooms_clear() const;
    void rooms_convert();

    String get_configuration_warning() const;

    static void show_alert(const String& message);
    static real_t _get_default_portal_margin();

#ifdef TOOLS_ENABLED
    void rooms_flip_portals();

    bool _room_regenerate_bound(Room* p_room);
    void _rooms_changed(String p_reason);

    // Static versions of methods for use in editor toolbars.
    static bool static_rooms_get_active();
    static void static_rooms_set_active(bool p_active);
    static bool static_rooms_get_active_and_loaded();
    static void static_rooms_convert();
#endif // TOOLS_ENABLED

protected:
    static void _bind_methods();
    void _notification(int notification_id);
    void get_project_settings();

private:
    NodePath room_list_node_path;
    NodePath preview_camera_node_path;

    ObjectID preview_camera_ID = 0;

    PVSMode pvs_mode = PVS_MODE_PARTIAL;

    Room::SimplifyInfo default_simplify_info;

    String pvs_filename;

    Camera* preview_camera = nullptr;

    // Local version of the camera frustum.
    // Prevents unnecessary updates to the visual server.
    Vector3 camera_position;
    Vector<Plane> camera_planes;

    real_t overlap_warning_threshold = 1.0;
    real_t roaming_expansion_margin  = 1.0;
    int conversion_count             = 0;
    int portal_depth_limit           = 16;

    bool active                   = true;
    bool debug_logging            = true;
    bool debug_sprawl             = false;
    bool gameplay_monitor_enabled = false;
    bool merge_meshes             = false;
    bool pvs_logging              = false;
    bool remove_danglers          = true;
    bool use_secondary_pvs        = false;
    bool use_signals              = true;
    bool use_simple_pvs           = false;

    static real_t default_portal_margin;

    void update_preview_camera();

#ifdef TOOLS_ENABLED
    void flip_portals(Spatial* rooms_root_node);
#endif // TOOLS_ENABLED

    static void auto_place_objects(
        Spatial* node,
        LocalVector<Room*, int32_t>& rooms,
        bool debug_logging
    );
    static void auto_place_object(
        VisualInstance* visual_instance,
        LocalVector<Room*, int32_t>& rooms,
        bool debug_logging
    );

    // Create portal links.
    static bool add_portal_links(
        LocalVector<Room*, int32_t>& rooms,
        LocalVector<Portal*>& portals,
        const Spatial* rooms_root_node
    );
    static bool add_imported_portal_portal_link(
        Portal* portal,
        const Spatial* rooms_root_node
    );

    static void create_simplified_bound(
        const Room* room,
        Geometry::MeshData& mesh_data,
        LocalVector<Plane, int32_t>& planes,
        const Room::SimplifyInfo& default_simplify_info,
        int portal_planes_count
    );

    static void check_portal_link(
        const Portal* portal,
        LocalVector<Room*, int32_t>& rooms,
        bool portal_links_out
    );
    static void check_rooms_portals_links(
        LocalVector<Room*, int32_t>& rooms,
        const LocalVector<Portal*>& portals
    );
    static void finalize_rooms(
        LocalVector<Room*, int32_t>& rooms,
        const LocalVector<Portal*>& portals,
        const Room::SimplifyInfo& default_simplify_info,
        bool debug_logging
    );

    template <class T>
    static T* convert_node_to(
        Spatial* original_node,
        const String& prefix_original,
        bool delete_original = true
    );

    // Create the rooms, room groups, portals.
    static void add_nodes(
        Spatial* node,
        LocalVector<Room*, int32_t>& rooms,
        LocalVector<Portal*>& portals,
        LocalVector<RoomGroup*>& room_groups,
        int room_group,
        int conversion_count,
        bool debug_logging
    );
    static void add_room(
        Spatial* node,
        LocalVector<Room*, int32_t>& rooms,
        LocalVector<Portal*>& portals,
        const LocalVector<RoomGroup*>& room_groups,
        int room_group,
        int conversion_count
    );
    static int add_room_group(
        Spatial* node,
        LocalVector<RoomGroup*>& room_groups,
        int conversion_count,
        bool debug_logging
    );
    static void add_portals(
        Spatial* node,
        int room_id,
        LocalVector<Portal*>& portals,
        int conversion_count
    );
    static void add_portal(
        Spatial* node,
        int room_id,
        LocalVector<Portal*>& portals,
        int conversion_count
    );
    static void add_rooms_to_room_groups(
        LocalVector<Room*, int32_t>& rooms,
        const LocalVector<RoomGroup*>& room_groups
    );
    static void add_room_to_room_groups(
        Room* room,
        const LocalVector<RoomGroup*>& room_groups
    );

    // Create room bounds.
    static void add_rooms_bounds(
        LocalVector<Room*, int32_t>& rooms,
        const LocalVector<Portal*>& portals,
        const Room::SimplifyInfo& default_simplify_info
    );
    static void add_room_bounds(
        Room* room,
        const LocalVector<Portal*>& portals,
        const Room::SimplifyInfo& default_simplify_info
    );
    static bool add_plane_if_unique(
        const Plane& plane,
        LocalVector<Plane, int32_t>& planes,
        const Room* room,
        const Room::SimplifyInfo& default_simplify_info
    );
    static void add_mesh_planes(
        Room* room,
        const Geometry::MeshData& mesh_data,
        const Room::SimplifyInfo& default_simplify_info
    );
    static void add_portal_planes(
        Room* room,
        const LocalVector<Portal*>& portals,
        const Room::SimplifyInfo& default_simplify_info
    );
    static void check_portal_for_warnings(
        Portal* portal,
        const AABB& room_aabb
    );
    static Vector<Vector3> convert_manual_bound(MeshInstance* mesh_instance);
    static bool create_initial_room_hull(
        Room* room,
        const Vector<Vector3>& room_points,
        const LocalVector<Portal*>& portals,
        const Room::SimplifyInfo& default_simplify_info
    );
    static bool create_final_room_hull(
        Room* room,
        const LocalVector<Portal*>& portals,
        const Room::SimplifyInfo& default_simplify_info,
        bool debug_logging
    );
    static bool get_room_points(
        Room* room,
        Vector<Vector3>& room_points,
        const LocalVector<Portal*>& portals,
        const Room::SimplifyInfo& default_simplify_info
    );
    static void update_room_points(
        const Room* room,
        Vector<Vector3>& room_points
    );

    // Auto link portals.
    static bool room_has_point(Room* room, const Vector3& point);
    static int find_best_destination_room_id_at_point(
        LocalVector<Room*, int32_t>& rooms,
        int source_room_id,
        const Vector3& point
    );
    static int find_portal_best_destination_room_id(
        const Portal* portal,
        LocalVector<Room*, int32_t>& rooms
    );
    static bool auto_link_portals(
        LocalVector<Room*, int32_t>& rooms,
        LocalVector<Portal*>& portals,
        bool debug_logging
    );

    static void clean_up_rooms(LocalVector<Room*, int32_t>& rooms);

#ifdef TOOLS_ENABLED
    static bool create_room_overlap_zones(
        LocalVector<Room*, int32_t>& rooms,
        real_t rooms_overlap_volume_threshold
    );
#endif // TOOLS_ENABLED
};

VARIANT_ENUM_CAST(RoomManager::PVSMode);

#endif // ROOM_MANAGER_H
