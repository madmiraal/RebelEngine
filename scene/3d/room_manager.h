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
class GeometryInstance;
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

    bool get_debug_sprawl() const;
    void set_debug_sprawl(bool p_enable);
    real_t get_default_portal_margin() const;
    void set_default_portal_margin(real_t p_dist);
    bool get_gameplay_monitor_enabled() const;
    void set_gameplay_monitor_enabled(bool p_enable);
    bool get_merge_meshes() const;
    void set_merge_meshes(bool p_enable);
    int get_overlap_warning_threshold() const;
    void set_overlap_warning_threshold(int p_value);
    int get_portal_depth_limit() const;
    void set_portal_depth_limit(int p_limit);
    NodePath get_preview_camera_path() const;
    void set_preview_camera_path(const NodePath& p_path);
    String get_pvs_filename() const;
    void set_pvs_filename(String p_filename);
    PVSMode get_pvs_mode() const;
    void set_pvs_mode(PVSMode p_mode);
    real_t get_roaming_expansion_margin() const;
    void set_roaming_expansion_margin(real_t p_dist);
    NodePath get_roomlist_path() const;
    void set_roomlist_path(const NodePath& p_path);
    real_t get_room_simplify() const;
    void set_room_simplify(real_t p_value);
    bool get_show_margins() const;
    void set_show_margins(bool p_show);
    bool get_use_secondary_pvs() const;
    void set_use_secondary_pvs(bool p_enable);

    bool rooms_get_active() const;
    void rooms_set_active(bool p_active);
    void rooms_clear();
    void rooms_convert();
    void rooms_flip_portals();

    String get_configuration_warning() const;

    static void show_warning(
        const String& p_string,
        const String& p_extra_string = "",
        bool p_alert                 = true
    );

    static real_t _get_default_portal_margin();
    static String _find_name_before(
        Node* p_node,
        String p_postfix,
        bool p_allow_no_postfix = false
    );

    // for internal use in the editor..
    // either we can clear the rooms and unload,
    // or reconvert.
    void _rooms_changed(String p_reason);

#ifdef TOOLS_ENABLED
    bool _room_regenerate_bound(Room* p_room);

    // Static versions of methods for use in editor toolbars.
    static bool static_rooms_get_active();
    static void static_rooms_set_active(bool p_active);
    static bool static_rooms_get_active_and_loaded();
    static void static_rooms_convert();
#endif // TOOLS_ENABLED

protected:
    static void _bind_methods();
    void _notification(int p_what);
    void _refresh_from_project_settings();

private:
    NodePath _settings_path_roomlist;
    NodePath _settings_path_preview_camera;

    // Only used during conversion.
    // Could be invalidated later by user deleting rooms etc.
    LocalVector<Room*, int32_t> _rooms;

    // Debug override camera
    ObjectID _preview_camera_ID = -1;

    PVSMode _pvs_mode = PVS_MODE_PARTIAL;

    Room::SimplifyInfo _room_simplify_info;

    String _pvs_filename;

    Spatial* _roomlist = nullptr;

    // Local version of the camera frustum.
    // Prevents updating the visual server, which causes a screen refresh,
    // if not necessary.
    Vector3 _camera_pos;
    Vector<Plane> _camera_planes;

    real_t _overlap_warning_threshold         = 1.0;
    real_t _settings_roaming_expansion_margin = 1.0;
    int _conversion_tick                      = 0;
    int _settings_portal_depth_limit          = 16;

    bool _active                             = true;
    bool _debug_sprawl                       = false;
    bool _settings_gameplay_monitor_enabled  = false;
    bool _settings_merge_meshes              = false;
    bool _settings_log_pvs_generation        = false;
    bool _settings_remove_danglers           = true;
    bool _settings_use_secondary_pvs         = false;
    bool _settings_use_signals               = true;
    bool _settings_use_simple_pvs            = false;
    bool _show_debug                         = true;
    bool _warning_misnamed_nodes_detected    = false;
    bool _warning_portal_link_room_not_found = false;
    bool _warning_portal_autolink_failed     = false;
    bool _warning_room_overlap_detected      = false;

    static real_t _default_portal_margin;

    void _preview_camera_update();
    bool resolve_preview_camera_path();

    // Conversion.
    // First pass.
    void _convert_portal(
        Room* p_room,
        Spatial* p_node,
        LocalVector<Portal*>& portals
    );
    void _convert_room(
        Spatial* p_node,
        LocalVector<Portal*>& r_portals,
        const LocalVector<RoomGroup*>& p_roomgroups,
        int p_roomgroup
    );
    int _convert_roomgroup(
        Spatial* p_node,
        LocalVector<RoomGroup*>& r_roomgroups
    );
    void _convert_rooms_recursive(
        Spatial* p_node,
        LocalVector<Portal*>& r_portals,
        LocalVector<RoomGroup*>& r_roomgroups,
        int p_roomgroup = -1
    );
    void _find_portals_recursive(
        Spatial* p_node,
        Room* p_room,
        LocalVector<Portal*>& r_portals
    );

    // Second pass.
    bool _bound_findpoints_geom_instance(
        GeometryInstance* p_gi,
        Vector<Vector3>& r_room_pts,
        AABB& r_aabb
    );
    bool _bound_findpoints_mesh_instance(
        MeshInstance* p_mi,
        Vector<Vector3>& r_room_pts,
        AABB& r_aabb
    );
    void _check_portal_for_warnings(
        Portal* p_portal,
        const AABB& p_room_aabb_without_portals
    );
    bool _convert_manual_bound(
        Room* p_room,
        Spatial* p_node,
        const LocalVector<Portal*>& p_portals
    );
    bool _convert_room_hull_preliminary(
        Room* p_room,
        const Vector<Vector3>& p_room_pts,
        const LocalVector<Portal*>& p_portals
    );
    void _find_statics_recursive(
        Room* p_room,
        Spatial* p_node,
        Vector<Vector3>& r_room_pts,
        bool p_add_to_portal_renderer
    );
    void _process_static(
        Room* p_room,
        Spatial* p_node,
        Vector<Vector3>& r_room_pts,
        bool p_add_to_portal_renderer
    );
    void _second_pass_portals(
        Spatial* p_roomlist,
        LocalVector<Portal*>& r_portals
    );
    void _second_pass_room(
        Room* p_room,
        const LocalVector<RoomGroup*>& p_roomgroups,
        const LocalVector<Portal*>& p_portals
    );
    void _second_pass_rooms(
        const LocalVector<RoomGroup*>& p_roomgroups,
        const LocalVector<Portal*>& p_portals
    );

    // Third pass.
    void _autolink_portals(
        Spatial* p_roomlist,
        LocalVector<Portal*>& r_portals
    );
    void _build_simplified_bound(
        const Room* p_room,
        Geometry::MeshData& r_md,
        LocalVector<Plane, int32_t>& r_planes,
        int p_num_portal_planes
    );
    bool _convert_room_hull_final(
        Room* p_room,
        const LocalVector<Portal*>& p_portals
    );
    void _third_pass_rooms(const LocalVector<Portal*>& p_portals);

    // Automatically place STATIC and DYNAMICs that are not within a room into
    // the most appropriate room, and sprawl.
    void _autoplace_recursive(Spatial* p_node);
    bool _autoplace_object(VisualInstance* p_vi);

    bool _add_plane_if_unique(
        const Room* p_room,
        LocalVector<Plane, int32_t>& r_planes,
        const Plane& p
    );
    Error _build_room_convex_hull(
        const Room* p_room,
        const Vector<Vector3>& p_points,
        Geometry::MeshData& r_mesh
    );
    bool _check_roomlist_validity(Node* p_node);
    void _cleanup_after_conversion();
    void _update_portal_gizmos(Spatial* p_node);

    // Merging
    void _list_mergeable_mesh_instances(
        Spatial* p_node,
        LocalVector<MeshInstance*, int32_t>& r_list
    );
    void _merge_log(String p_string);
    void _merge_meshes_in_room(Room* p_room);
    bool _remove_redundant_dangling_nodes(Spatial* p_node);

    // Helper methods.
    Error _build_convex_hull(
        const Vector<Vector3>& p_points,
        Geometry::MeshData& r_mesh,
        real_t p_epsilon = 3.0 * UNIT_EPSILON
    );
    void _flip_portals_recursive(Spatial* p_node);
    bool _name_ends_with(const Node* p_node, String p_postfix) const;
    void _set_owner_recursive(Node* p_node, Node* p_owner);
    void _update_gizmos_recursive(Node* p_node);

    template <class NODE_TYPE>
    NODE_TYPE* _resolve_path(NodePath p_path) const;

    template <class NODE_TYPE>
    bool _node_is_type(Node* p_node) const;

    template <class T>
    T* _change_node_type(
        Spatial* p_node,
        String p_prefix,
        bool p_delete = true
    );

    // Output strings during conversion process.
    void convert_log(String p_string, int p_priority = 0);
    // Only prints when user has set 'debug' in the room manager inspector.
    void debug_print_line(String p_string, int p_priority = 0);

#ifdef TOOLS_ENABLED
    void _generate_room_overlap_zones();
#endif // TOOLS_ENABLED
};

VARIANT_ENUM_CAST(RoomManager::PVSMode);

#endif // ROOM_MANAGER_H
