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

    void rooms_clear();
    void rooms_convert();
    void rooms_flip_portals();

    String get_configuration_warning() const;

    static void show_alert(const String& message);
    static real_t _get_default_portal_margin();
    static String _find_name_before(
        const Node* p_node,
        String p_postfix,
        bool p_allow_no_postfix = false
    );

#ifdef TOOLS_ENABLED
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

    // Only used during conversion.
    // Could be invalidated later by user deleting rooms etc.
    LocalVector<Room*, int32_t> rooms;

    ObjectID preview_camera_ID = 0;

    PVSMode pvs_mode = PVS_MODE_PARTIAL;

    Room::SimplifyInfo room_simplify_info;

    String pvs_filename;

    Camera* preview_camera   = nullptr;
    Spatial* rooms_root_node = nullptr;

    // Local version of the camera frustum.
    // Prevents updating the visual server, which causes a screen refresh,
    // if not necessary.
    Vector3 camera_position;
    Vector<Plane> camera_planes;

    real_t overlap_warning_threshold = 1.0;
    real_t roaming_expansion_margin  = 1.0;
    int conversion_count             = 0;
    int portal_depth_limit           = 16;

    bool active                     = true;
    bool debug_sprawl               = false;
    bool gameplay_monitor_enabled   = false;
    bool merge_meshes               = false;
    bool pvs_logging                = false;
    bool remove_danglers            = true;
    bool use_secondary_pvs          = false;
    bool use_signals                = true;
    bool use_simple_pvs             = false;
    bool debug_logging              = true;
    bool misnamed_nodes_detected    = false;
    bool portal_link_room_not_found = false;
    bool portal_autolink_failed     = false;
    bool room_overlap_detected      = false;

    static real_t default_portal_margin;

    void update_preview_camera();

    // Create the rooms, room groups and portals.
    void add_nodes(
        Spatial* node,
        LocalVector<Portal*>& portals,
        LocalVector<RoomGroup*>& room_groups,
        int room_group = -1
    );
    void add_room(
        Spatial* node,
        LocalVector<Portal*>& portals,
        const LocalVector<RoomGroup*>& room_groups,
        int room_group
    );
    int add_room_group(Spatial* node, LocalVector<RoomGroup*>& room_groups);
    void add_portals(Spatial* node, Room* room, LocalVector<Portal*>& portals);
    void add_portal(Spatial* node, LocalVector<Portal*>& portals, int room_id);
    void add_portal_links(LocalVector<Portal*>& portals);

    // Second pass.
    bool _bound_findpoints_geom_instance(
        GeometryInstance* p_gi,
        Vector<Vector3>& r_room_pts,
        AABB& r_aabb
    );
    void _check_portal_for_warnings(
        Portal* p_portal,
        const AABB& p_room_aabb_without_portals
    );
    bool _convert_manual_bound(
        Room* room,
        Spatial* node,
        const LocalVector<Portal*>& portals
    );
    bool _convert_room_hull_preliminary(
        Room* room,
        const Vector<Vector3>& room_points,
        const LocalVector<Portal*>& portals
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
    void add_room_bounds(
        Room* room,
        const LocalVector<RoomGroup*>& room_groups,
        const LocalVector<Portal*>& portals
    );
    void create_room_statics(
        const LocalVector<RoomGroup*>& room_groups,
        const LocalVector<Portal*>& portals
    );

    // Third pass.
    void finalize_portals(Spatial* p_roomlist, LocalVector<Portal*>& r_portals);
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
    void finalize_rooms(const LocalVector<Portal*>& p_portals);

    // Automatically place STATIC and DYNAMICs that are not within a room into
    // the most appropriate room, and sprawl.
    void place_remaining_statics(Spatial* p_node);
    bool _autoplace_object(VisualInstance* p_vi);

    bool add_plane_if_unique(
        const Room* room,
        LocalVector<Plane, int32_t>& planes,
        const Plane& plane
    ) const;
    Error build_room_convex_hull(
        const Room* room,
        const Vector<Vector3>& points,
        Geometry::MeshData& mesh_data
    ) const;
    void _cleanup_after_conversion();
    void update_portal_gizmos(Spatial* p_node);

    // Merging
    void _list_mergeable_mesh_instances(
        Spatial* p_node,
        LocalVector<MeshInstance*, int32_t>& r_list
    );
    void _merge_log(String p_string);
    void merge_room_meshes(Room* p_room);
    bool _remove_redundant_dangling_nodes(Spatial* p_node);

    // Helper methods.
    void _flip_portals_recursive(Spatial* p_node);
    void _update_gizmos_recursive(Node* p_node);

    template <class T>
    T* convert_node_to(
        Spatial* original_node,
        const String& prefix_original,
        bool delete_original = true
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
