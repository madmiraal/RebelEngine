// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "room_manager.h"

#include "core/bitfield_dynamic.h"
#include "core/engine.h"
#include "core/math/quick_hull.h"
#include "core/os/os.h"
#include "editor/editor_node.h"
#include "mesh_instance.h"
#include "modules/modules_enabled.gen.h"
#include "multimesh_instance.h"
#include "portal.h"
#include "room_group.h"
#include "scene/3d/camera.h"
#include "scene/3d/light.h"
#include "scene/3d/sprite_3d.h"
#include "visibility_notifier.h"

#ifdef MODULE_CSG_ENABLED
#include "modules/csg/csg_shape.h"
#endif // MODULE_CSG_ENABLED

#ifdef TOOLS_ENABLED
#include "editor/plugins/spatial_editor_plugin.h"

RoomManager* RoomManager::active_room_manager = nullptr;
#endif // TOOLS_ENABLED

real_t RoomManager::default_portal_margin = 1.0;

static bool add_mesh_instance_points(
    const MeshInstance* mesh_instance,
    Vector<Vector3>& room_points
) {
    Ref<Mesh> mesh = mesh_instance->get_mesh();
    ERR_FAIL_COND_V(!mesh.is_valid(), false);
    ERR_FAIL_COND_V_MSG(
        mesh->get_surface_count() == 0,
        false,
        "MeshInstance '" + mesh_instance->get_name()
            + "' has no surfaces, ignoring"
    );
    bool success              = false;
    const Transform transform = mesh_instance->get_global_transform();
    for (int surface = 0; surface < mesh->get_surface_count(); surface++) {
        Array arrays = mesh->surface_get_arrays(surface);
        ERR_CONTINUE_MSG(arrays.empty(), "Ignoring mesh surface with no mesh.");
        success                      = true;
        PoolVector<Vector3> vertices = arrays[VS::ARRAY_VERTEX];
        for (int vertex = 0; vertex < vertices.size(); vertex++) {
            room_points.push_back(transform.xform(vertices[vertex]));
        }
    }
    return success;
}

static Error build_convex_hull(
    const Vector<Vector3>& points,
    Geometry::MeshData& mesh,
    const real_t epsilon = 3.0 * UNIT_EPSILON
) {
    QuickHull::_flag_warnings = false;
    const Error error         = QuickHull::build(points, mesh, epsilon);
    QuickHull::_flag_warnings = true;
    return error;
}

static Error build_room_aabb_convex_hull(
    const Vector<Vector3>& room_points,
    Geometry::MeshData& mesh_data
) {
    AABB aabb;
    aabb.create_from_points(room_points);
    LocalVector<Vector3> points;
    const Vector3 minimum = aabb.position;
    const Vector3 maximum = minimum + aabb.size;
    points.push_back(Vector3(minimum.x, minimum.y, minimum.z));
    points.push_back(Vector3(minimum.x, maximum.y, minimum.z));
    points.push_back(Vector3(maximum.x, maximum.y, minimum.z));
    points.push_back(Vector3(maximum.x, minimum.y, minimum.z));
    points.push_back(Vector3(minimum.x, minimum.y, maximum.z));
    points.push_back(Vector3(minimum.x, maximum.y, maximum.z));
    points.push_back(Vector3(maximum.x, maximum.y, maximum.z));
    points.push_back(Vector3(maximum.x, minimum.y, maximum.z));
    return build_convex_hull(points, mesh_data);
}

static bool node_name_ends_with(const Node* node, const String& suffix) {
    ERR_FAIL_NULL_V(node, false);
    const String name       = node->get_name();
    const int name_length   = name.length();
    const int suffix_length = suffix.length();
    if (suffix_length > name_length) {
        return false;
    }
    return name.substr(name_length - suffix_length, suffix_length).to_lower()
        == suffix;
}

static void set_node_and_descendents_owner(Node* node, Node* owner) {
    if (!node->get_owner() && node != owner) {
        node->set_owner(owner);
    }
    for (int child = 0; child < node->get_child_count(); child++) {
        set_node_and_descendents_owner(node->get_child(child), owner);
    }
}

RoomManager::RoomManager() {
    // We set this to a high value, because we want room manager to be processed
    // after other nodes and after the camera has moved.
    set_process_priority(10000);
}

bool RoomManager::rooms_get_active() const {
    return active;
}

void RoomManager::rooms_set_active(const bool enabled) {
    if (is_inside_world() && get_world().is_valid()) {
        active = enabled;
        VisualServer::get_singleton()->rooms_set_active(
            get_world()->get_scenario(),
            active
        );
#ifdef TOOLS_ENABLED
        if (Engine::get_singleton()->is_editor_hint()) {
            SpatialEditor* spatial_editor = SpatialEditor::get_singleton();
            if (spatial_editor) {
                spatial_editor->update_portal_tools();
            }
        }
#endif
    }
}

bool RoomManager::get_debug_sprawl() const {
    return debug_sprawl;
}

void RoomManager::set_debug_sprawl(const bool enabled) {
    if (is_inside_world() && get_world().is_valid()) {
        debug_sprawl = enabled;
        VisualServer::get_singleton()->rooms_set_debug_feature(
            get_world()->get_scenario(),
            VisualServer::ROOMS_DEBUG_SPRAWL,
            debug_sprawl
        );
    }
}

real_t RoomManager::get_default_portal_margin() const {
    return default_portal_margin;
}

void RoomManager::set_default_portal_margin(const real_t new_margin) {
    default_portal_margin = new_margin;
    Spatial* room_list    = cast_to<Spatial>(get_node(room_list_node_path));
    if (!room_list) {
        return;
    }
    update_portal_gizmos(room_list);
}

bool RoomManager::get_gameplay_monitor_enabled() const {
    return gameplay_monitor_enabled;
}

void RoomManager::set_gameplay_monitor_enabled(const bool enabled) {
    gameplay_monitor_enabled = enabled;
}

bool RoomManager::get_merge_meshes() const {
    return merge_meshes;
}

void RoomManager::set_merge_meshes(const bool enabled) {
    merge_meshes = enabled;
}

int RoomManager::get_overlap_warning_threshold() const {
    return static_cast<int>(overlap_warning_threshold);
}

void RoomManager::set_overlap_warning_threshold(const int new_threshold) {
    overlap_warning_threshold = new_threshold;
}

int RoomManager::get_portal_depth_limit() const {
    return portal_depth_limit;
}

void RoomManager::set_portal_depth_limit(const int new_limit) {
    portal_depth_limit = new_limit;
    if (is_inside_world() && get_world().is_valid()) {
        VisualServer::get_singleton()->rooms_set_params(
            get_world()->get_scenario(),
            portal_depth_limit,
            roaming_expansion_margin
        );
    }
}

NodePath RoomManager::get_preview_camera_path() const {
    return preview_camera_node_path;
}

void RoomManager::set_preview_camera_path(const NodePath& new_path) {
    preview_camera = cast_to<Camera>(get_node(new_path));
    // If in the editor, use internal processing if using a preview camera.
    if (Engine::get_singleton()->is_editor_hint()) {
        if (is_inside_tree()) {
            set_process_internal(preview_camera != nullptr);
        }
    }
    if (!preview_camera) {
        if (new_path != NodePath()) {
            WARN_PRINT("Cannot resolve NodePath to a Camera.");
            preview_camera_node_path = NodePath();
        }
        // Inform the Visual Server that we are not using a preview camera.
        if (is_inside_world() && get_world().is_valid()
            && get_world()->get_scenario().is_valid()) {
            VisualServer::get_singleton()->rooms_override_camera(
                get_world()->get_scenario(),
                false,
                Vector3(),
                nullptr
            );
        }
        return;
    }
    preview_camera_node_path = new_path;
    preview_camera_ID        = preview_camera->get_instance_id();
    // Force a Visual Server update on the next internal_process.
    camera_planes.clear();
}

String RoomManager::get_pvs_filename() const {
    return pvs_filename;
}

void RoomManager::set_pvs_filename(const String& new_filename) {
    pvs_filename = new_filename;
}

RoomManager::PVSMode RoomManager::get_pvs_mode() const {
    return pvs_mode;
}

void RoomManager::set_pvs_mode(const PVSMode new_mode) {
    pvs_mode = new_mode;
}

real_t RoomManager::get_roaming_expansion_margin() const {
    return roaming_expansion_margin;
}

void RoomManager::set_roaming_expansion_margin(const real_t new_margin) {
    roaming_expansion_margin = new_margin;
    if (is_inside_world() && get_world().is_valid()) {
        VisualServer::get_singleton()->rooms_set_params(
            get_world()->get_scenario(),
            portal_depth_limit,
            roaming_expansion_margin
        );
    }
}

NodePath RoomManager::get_roomlist_path() const {
    return room_list_node_path;
}

void RoomManager::set_roomlist_path(const NodePath& new_path) {
    room_list_node_path = new_path;
    update_configuration_warning();
}

real_t RoomManager::get_room_simplify() const {
    return room_simplify_info._plane_simplify;
}

void RoomManager::set_room_simplify(real_t new_value) {
    room_simplify_info.set_simplify(new_value);
}

bool RoomManager::get_show_margins() const {
    return Portal::_settings_gizmo_show_margins;
}

void RoomManager::set_show_margins(const bool show) {
    Portal::_settings_gizmo_show_margins = show;
    Spatial* room_list = cast_to<Spatial>(get_node(room_list_node_path));
    if (!room_list) {
        return;
    }
    _update_gizmos_recursive(room_list);
}

bool RoomManager::get_use_secondary_pvs() const {
    return use_secondary_pvs;
}

void RoomManager::set_use_secondary_pvs(const bool enabled) {
    use_secondary_pvs = enabled;
}

void RoomManager::rooms_clear() {
    rooms.clear();
    if (is_inside_world() && get_world().is_valid()) {
        VisualServer::get_singleton()->rooms_and_portals_clear(
            get_world()->get_scenario()
        );
    }
}

void RoomManager::rooms_convert() {
    ERR_FAIL_COND(!is_inside_world() || !get_world().is_valid());
    rooms_root_node = cast_to<Spatial>(get_node(room_list_node_path));
    if (!rooms_root_node) {
        WARN_PRINT("Cannot resolve Room List NodePath.");
        show_alert(
            TTR("Room List path is invalid.\n"
                "Please correctly set the Room Manager's Room List Node Path.")
        );
        return;
    }
    get_project_settings();
    misnamed_nodes_detected    = false;
    portal_link_room_not_found = false;
    portal_autolink_failed     = false;
    room_overlap_detected      = false;

    conversion_count++;
    rooms_clear();
    LocalVector<Portal*> portals;
    LocalVector<RoomGroup*> room_groups;
    add_nodes(rooms_root_node, portals, room_groups);
    if (rooms.empty()) {
        rooms_clear();
        show_alert(TTR("No Rooms found!"));
        return;
    }
    add_portal_links(portals);
    create_room_statics(room_groups, portals);
    finalize_portals(rooms_root_node, portals);
    finalize_rooms(portals);
    place_remaining_statics(rooms_root_node);

    {
        bool generate_pvs = false;
        bool cull_pvs     = false;
        switch (pvs_mode) {
            default: {
            } break;
            case PVS_MODE_PARTIAL: {
                generate_pvs = true;
            } break;
            case PVS_MODE_FULL: {
                generate_pvs = true;
                cull_pvs     = true;
            } break;
        }

        VisualServer::get_singleton()->rooms_finalize(
            get_world()->get_scenario(),
            generate_pvs,
            cull_pvs,
            use_secondary_pvs,
            use_signals,
            pvs_filename,
            use_simple_pvs,
            pvs_logging
        );
    }

    VisualServer::get_singleton()->rooms_set_params(
        get_world()->get_scenario(),
        portal_depth_limit,
        roaming_expansion_margin
    );

#ifdef TOOLS_ENABLED
    _generate_room_overlap_zones();
#endif

    // just delete any intermediate data
    _cleanup_after_conversion();

    // display error dialogs
    if (misnamed_nodes_detected) {
        show_alert(TTR("Misnamed nodes detected."));
        rooms_clear();
    }

    if (portal_link_room_not_found) {
        show_alert(TTR("Portal link room not found."));
    }

    if (portal_autolink_failed) {
        show_alert(
            TTR("Portal autolink failed!\n"
                "Ensure the portal is facing outwards from the source room.")
        );
    }

    if (room_overlap_detected) {
        show_alert(
            TTR("Room overlap detected.\n"
                "Cameras may work incorrectly in overlapping area.")
        );
    }
}

void RoomManager::rooms_flip_portals() {
    // this is a helper emergency function to deal with situations where the
    // user has ended up with Portal nodes pointing in the wrong direction (by
    // doing initial conversion with flip_portal_meshes set incorrectly).
    rooms_root_node = cast_to<Spatial>(get_node(room_list_node_path));
    if (!rooms_root_node) {
        WARN_PRINT("Cannot resolve nodepath");
        show_alert(
            TTR("RoomList path is invalid.\n"
                "Please check the RoomList branch has been assigned in the "
                "RoomManager.")
        );
        return;
    }

    _flip_portals_recursive(rooms_root_node);
#ifdef TOOLS_ENABLED
    _rooms_changed("flipped Portals");
#endif // TOOLS_ENABLED
}

String RoomManager::get_configuration_warning() const {
    String warning = Spatial::get_configuration_warning();
    if (room_list_node_path == NodePath()) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("The RoomList has not been assigned.");
    } else {
        auto* roomlist = cast_to<Spatial>(get_node(room_list_node_path));
        if (!roomlist) {
            if (!warning.empty()) {
                warning += "\n\n";
            }
            warning +=
                TTR("The RoomList node should be a Spatial (or derived from "
                    "Spatial).");
        }
    }
    if (portal_depth_limit == 0) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning +=
            TTR("Portal Depth Limit is set to Zero.\n"
                "Only the Room that the Camera is in will render.");
    }
    if (Room::detect_nodes_of_type<RoomManager>(this)) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning +=
            TTR("There should only be one RoomManager in the SceneTree.");
    }
    return warning;
}

void RoomManager::show_alert(const String& message) {
#ifdef TOOLS_ENABLED
    if (Engine::get_singleton()->is_editor_hint()) {
        EditorNode::get_singleton()->show_warning(message, TTR("Room Manager"));
    }
#endif // TOOLS_ENABLED
}

real_t RoomManager::_get_default_portal_margin() {
    return default_portal_margin;
}

String RoomManager::_find_name_before(
    const Node* p_node,
    String p_postfix,
    bool p_allow_no_postfix
) {
    ERR_FAIL_NULL_V(p_node, String());
    String name = p_node->get_name();

    int pf_l = p_postfix.length();
    int l    = name.length();

    if (pf_l > l) {
        if (!p_allow_no_postfix) {
            return String();
        }
    } else {
        if (name.substr(l - pf_l, pf_l) == p_postfix) {
            name = name.substr(0, l - pf_l);
        } else {
            if (!p_allow_no_postfix) {
                return String();
            }
        }
    }

    // Rebel Engine doesn't support multiple nodes with the same name.
    // Therefore, we strip everything after a '*'.
    // e.g. kitchen*2-portal -> kitchen*
    for (int c = 0; c < name.length(); c++) {
        if (name[c] == '*') {
            name = name.substr(0, c);
            break;
        }
    }

    return name;
}

#ifdef TOOLS_ENABLED
bool RoomManager::_room_regenerate_bound(Room* p_room) {
    // for a preview, we allow the editor to change the bound
    ERR_FAIL_COND_V(!p_room, false);

    if (!p_room->_bound_pts.size()) {
        return false;
    }

    // can't do yet if not in the tree
    if (!p_room->is_inside_tree()) {
        return false;
    }

    Transform tr = p_room->get_global_transform();

    Vector<Vector3> pts;
    pts.resize(p_room->_bound_pts.size());
    for (int n = 0; n < pts.size(); n++) {
        pts.set(n, tr.xform(p_room->_bound_pts[n]));
    }

    Geometry::MeshData md;
    Error err = build_room_convex_hull(p_room, pts, md);

    if (err != OK) {
        return false;
    }

    p_room->_bound_mesh_data = md;
    p_room->update_gizmo();

    return true;
}

void RoomManager::_rooms_changed(String p_reason) {
    rooms.clear();
    if (is_inside_world() && get_world().is_valid()) {
        VisualServer::get_singleton()->rooms_unload(
            get_world()->get_scenario(),
            p_reason
        );
    }
}

bool RoomManager::static_rooms_get_active() {
    if (active_room_manager) {
        return active_room_manager->rooms_get_active();
    }

    return false;
}

void RoomManager::static_rooms_set_active(bool p_active) {
    if (active_room_manager) {
        active_room_manager->rooms_set_active(p_active);
        active_room_manager->property_list_changed_notify();
    }
}

bool RoomManager::static_rooms_get_active_and_loaded() {
    if (active_room_manager) {
        if (active_room_manager->rooms_get_active()) {
            Ref<World> world = active_room_manager->get_world();
            RID scenario     = world->get_scenario();
            return active_room_manager->rooms_get_active()
                && VisualServer::get_singleton()->rooms_is_loaded(scenario);
        }
    }

    return false;
}

void RoomManager::static_rooms_convert() {
    if (active_room_manager) {
        return active_room_manager->rooms_convert();
    }
}
#endif // TOOLS_ENABLED

void RoomManager::_bind_methods() {
    BIND_ENUM_CONSTANT(PVS_MODE_DISABLED);
    BIND_ENUM_CONSTANT(PVS_MODE_PARTIAL);
    BIND_ENUM_CONSTANT(PVS_MODE_FULL);

    ClassDB::bind_method(
        D_METHOD("rooms_get_active"),
        &RoomManager::rooms_get_active
    );
    ClassDB::bind_method(
        D_METHOD("rooms_set_active", "active"),
        &RoomManager::rooms_set_active
    );
    ClassDB::bind_method(
        D_METHOD("get_debug_sprawl"),
        &RoomManager::get_debug_sprawl
    );
    ClassDB::bind_method(
        D_METHOD("set_debug_sprawl", "debug_sprawl"),
        &RoomManager::set_debug_sprawl
    );
    ClassDB::bind_method(
        D_METHOD("get_default_portal_margin"),
        &RoomManager::get_default_portal_margin
    );
    ClassDB::bind_method(
        D_METHOD("set_default_portal_margin", "default_portal_margin"),
        &RoomManager::set_default_portal_margin
    );
    ClassDB::bind_method(
        D_METHOD("get_gameplay_monitor_enabled"),
        &RoomManager::get_gameplay_monitor_enabled
    );
    ClassDB::bind_method(
        D_METHOD("set_gameplay_monitor_enabled", "gameplay_monitor"),
        &RoomManager::set_gameplay_monitor_enabled
    );
    ClassDB::bind_method(
        D_METHOD("get_merge_meshes"),
        &RoomManager::get_merge_meshes
    );
    ClassDB::bind_method(
        D_METHOD("set_merge_meshes", "merge_meshes"),
        &RoomManager::set_merge_meshes
    );
    ClassDB::bind_method(
        D_METHOD("get_overlap_warning_threshold"),
        &RoomManager::get_overlap_warning_threshold
    );
    ClassDB::bind_method(
        D_METHOD("set_overlap_warning_threshold", "overlap_warning_threshold"),
        &RoomManager::set_overlap_warning_threshold
    );
    ClassDB::bind_method(
        D_METHOD("get_portal_depth_limit"),
        &RoomManager::get_portal_depth_limit
    );
    ClassDB::bind_method(
        D_METHOD("set_portal_depth_limit", "portal_depth_limit"),
        &RoomManager::set_portal_depth_limit
    );
    ClassDB::bind_method(
        D_METHOD("get_preview_camera_path"),
        &RoomManager::get_preview_camera_path
    );
    ClassDB::bind_method(
        D_METHOD("set_preview_camera_path", "preview_camera"),
        &RoomManager::set_preview_camera_path
    );
    // Uncomment to add the ability to cache PVS to disk.
    // ClassDB::bind_method(
    //     D_METHOD("get_pvs_filename"),
    //     &RoomManager::get_pvs_filename
    // );
    // ClassDB::bind_method(
    //     D_METHOD("set_pvs_filename", "pvs_filename"),
    //     &RoomManager::set_pvs_filename
    // );
    ClassDB::bind_method(D_METHOD("get_pvs_mode"), &RoomManager::get_pvs_mode);
    ClassDB::bind_method(
        D_METHOD("set_pvs_mode", "pvs_mode"),
        &RoomManager::set_pvs_mode
    );
    ClassDB::bind_method(
        D_METHOD("get_roaming_expansion_margin"),
        &RoomManager::get_roaming_expansion_margin
    );
    ClassDB::bind_method(
        D_METHOD("set_roaming_expansion_margin", "roaming_expansion_margin"),
        &RoomManager::set_roaming_expansion_margin
    );
    ClassDB::bind_method(
        D_METHOD("get_roomlist_path"),
        &RoomManager::get_roomlist_path
    );
    ClassDB::bind_method(
        D_METHOD("set_roomlist_path", "p_path"),
        &RoomManager::set_roomlist_path
    );
    ClassDB::bind_method(
        D_METHOD("get_room_simplify"),
        &RoomManager::get_room_simplify
    );
    ClassDB::bind_method(
        D_METHOD("set_room_simplify", "room_simplify"),
        &RoomManager::set_room_simplify
    );
    ClassDB::bind_method(
        D_METHOD("get_show_margins"),
        &RoomManager::get_show_margins
    );
    ClassDB::bind_method(
        D_METHOD("set_show_margins", "show_margins"),
        &RoomManager::set_show_margins
    );
    ClassDB::bind_method(
        D_METHOD("get_use_secondary_pvs"),
        &RoomManager::get_use_secondary_pvs
    );
    ClassDB::bind_method(
        D_METHOD("set_use_secondary_pvs", "use_secondary_pvs"),
        &RoomManager::set_use_secondary_pvs
    );
    ClassDB::bind_method(D_METHOD("rooms_clear"), &RoomManager::rooms_clear);
    ClassDB::bind_method(
        D_METHOD("rooms_convert"),
        &RoomManager::rooms_convert
    );

    ADD_GROUP("Main", "");
    ADD_PROPERTY(
        PropertyInfo(Variant::BOOL, "active"),
        "rooms_set_active",
        "rooms_get_active"
    );
    ADD_PROPERTY(
        PropertyInfo(
            Variant::NODE_PATH,
            "roomlist",
            PROPERTY_HINT_NODE_PATH_VALID_TYPES,
            "Spatial"
        ),
        "set_roomlist_path",
        "get_roomlist_path"
    );

    ADD_GROUP("PVS", "");
    ADD_PROPERTY(
        PropertyInfo(
            Variant::INT,
            "pvs_mode",
            PROPERTY_HINT_ENUM,
            "Disabled,Partial,Full"
        ),
        "set_pvs_mode",
        "get_pvs_mode"
    );
    // Uncomment to add the ability to cache PVS to disk.
    // ADD_PROPERTY(
    //     PropertyInfo(
    //         Variant::STRING,
    //         "pvs_filename",
    //         PROPERTY_HINT_FILE,
    //         "*.pvs"
    //     ),
    //     "set_pvs_filename",
    //     "get_pvs_filename"
    // );

    ADD_GROUP("Gameplay", "");
    ADD_PROPERTY(
        PropertyInfo(Variant::BOOL, "gameplay_monitor"),
        "set_gameplay_monitor_enabled",
        "get_gameplay_monitor_enabled"
    );
    ADD_PROPERTY(
        PropertyInfo(Variant::BOOL, "use_secondary_pvs"),
        "set_use_secondary_pvs",
        "get_use_secondary_pvs"
    );

    ADD_GROUP("Optimize", "");
    ADD_PROPERTY(
        PropertyInfo(Variant::BOOL, "merge_meshes"),
        "set_merge_meshes",
        "get_merge_meshes"
    );

    ADD_GROUP("Debug", "");
    ADD_PROPERTY(
        PropertyInfo(Variant::BOOL, "show_margins"),
        "set_show_margins",
        "get_show_margins"
    );
    ADD_PROPERTY(
        PropertyInfo(Variant::BOOL, "debug_sprawl"),
        "set_debug_sprawl",
        "get_debug_sprawl"
    );
    ADD_PROPERTY(
        PropertyInfo(
            Variant::INT,
            "overlap_warning_threshold",
            PROPERTY_HINT_RANGE,
            "1,1000,1"
        ),
        "set_overlap_warning_threshold",
        "get_overlap_warning_threshold"
    );
    ADD_PROPERTY(
        PropertyInfo(Variant::NODE_PATH, "preview_camera"),
        "set_preview_camera_path",
        "get_preview_camera_path"
    );

    ADD_GROUP("Advanced", "");
    ADD_PROPERTY(
        PropertyInfo(
            Variant::INT,
            "portal_depth_limit",
            PROPERTY_HINT_RANGE,
            "0,255,1"
        ),
        "set_portal_depth_limit",
        "get_portal_depth_limit"
    );
    ADD_PROPERTY(
        PropertyInfo(
            Variant::REAL,
            "room_simplify",
            PROPERTY_HINT_RANGE,
            "0.0,1.0,0.005"
        ),
        "set_room_simplify",
        "get_room_simplify"
    );
    ADD_PROPERTY(
        PropertyInfo(
            Variant::REAL,
            "default_portal_margin",
            PROPERTY_HINT_RANGE,
            "0.0, 10.0, 0.01"
        ),
        "set_default_portal_margin",
        "get_default_portal_margin"
    );
    ADD_PROPERTY(
        PropertyInfo(
            Variant::REAL,
            "roaming_expansion_margin",
            PROPERTY_HINT_RANGE,
            "0.0, 3.0, 0.01"
        ),
        "set_roaming_expansion_margin",
        "get_roaming_expansion_margin"
    );
}

void RoomManager::_notification(const int notification_id) {
    switch (notification_id) {
        case NOTIFICATION_ENTER_TREE: {
            if (Engine::get_singleton()->is_editor_hint()) {
                set_process_internal(preview_camera != nullptr);
#ifdef TOOLS_ENABLED
                // note this mechanism may fail to work correctly if the user
                // creates two room managers, but should not create major
                // problems as it is just used to auto update when portals etc
                // are changed in the editor, and there is a check for nullptr.
                active_room_manager           = this;
                SpatialEditor* spatial_editor = SpatialEditor::get_singleton();
                if (spatial_editor) {
                    spatial_editor->update_portal_tools();
                }
#endif
            } else {
                if (gameplay_monitor_enabled) {
                    set_process_internal(true);
                }
            }
        } break;
        case NOTIFICATION_EXIT_TREE: {
#ifdef TOOLS_ENABLED
            active_room_manager = nullptr;
            if (Engine::get_singleton()->is_editor_hint()) {
                SpatialEditor* spatial_editor = SpatialEditor::get_singleton();
                if (spatial_editor) {
                    spatial_editor->update_portal_tools();
                }
            }
#endif
        } break;
        case NOTIFICATION_INTERNAL_PROCESS: {
            // can't call visual server if not inside world
            if (!is_inside_world()) {
                return;
            }

            if (Engine::get_singleton()->is_editor_hint()) {
                update_preview_camera();
                return;
            }

            if (gameplay_monitor_enabled) {
                Ref<World> world = get_world();
                RID scenario     = world->get_scenario();

                List<Camera*> cameras;
                world->get_camera_list(&cameras);

                Vector<Vector3> positions;

                for (int n = 0; n < cameras.size(); n++) {
                    positions.push_back(
                        cameras[n]->get_global_transform().origin
                    );
                }

                VisualServer::get_singleton()->rooms_update_gameplay_monitor(
                    scenario,
                    positions
                );
            }

        } break;
        default:;
    }
}

void RoomManager::get_project_settings() {
    Portal::_portal_plane_convention =
        GLOBAL_GET("rendering/portals/advanced/flip_imported_portals");
    use_simple_pvs  = GLOBAL_GET("rendering/portals/pvs/use_simple_pvs");
    use_signals     = GLOBAL_GET("rendering/portals/gameplay/use_signals");
    remove_danglers = GLOBAL_GET("rendering/portals/optimize/remove_danglers");
    pvs_logging     = GLOBAL_GET("rendering/portals/pvs/pvs_logging");
    debug_logging   = GLOBAL_GET("rendering/portals/debug/logging");
    // Only use logging in the editor.
    if (!Engine::get_singleton()->is_editor_hint()) {
        pvs_logging   = false;
        debug_logging = false;
    }
}

void RoomManager::update_preview_camera() {
    // Check if preview_camera assigned.
    // Note: preview_camera_ID is only valid if preview_camera is not a nullptr.
    if (preview_camera == nullptr) {
        return;
    }
    // Ensure the instance still exists.
    preview_camera = cast_to<Camera>(ObjectDB::get_instance(preview_camera_ID));
    if (!preview_camera) {
        return;
    }
    Ref<World> world   = get_world();
    const RID scenario = world->get_scenario();
    const Vector3 current_camera_position =
        preview_camera->get_global_transform().origin;
    const Vector<Plane> current_camera_planes = preview_camera->get_frustum();

    // Only update the visual server if the camera has changed.
    if (current_camera_position != camera_position
        || current_camera_planes.size() != camera_planes.size()) {
        return;
    }
    // Check each plane for a change.
    bool changed = false;
    for (int n = 0; n < current_camera_planes.size(); n++) {
        if (current_camera_planes[n] != camera_planes[n]) {
            changed = true;
            break;
        }
    }
    if (!changed) {
        return;
    }

    camera_position = current_camera_position;
    camera_planes   = current_camera_planes;
    VisualServer::get_singleton()->rooms_override_camera(
        scenario,
        true,
        current_camera_position,
        &current_camera_planes
    );
}

void RoomManager::add_nodes(
    Spatial* node,
    LocalVector<Portal*>& portals,
    LocalVector<RoomGroup*>& room_groups,
    int room_group
) {
    if (cast_to<Room>(node) || node_name_ends_with(node, "-room")) {
        add_room(node, portals, room_groups, room_group);
    }
    if (cast_to<RoomGroup>(node) || node_name_ends_with(node, "-roomgroup")) {
        room_group = add_room_group(node, room_groups);
    }
    for (int index = 0; index < node->get_child_count(); index++) {
        auto* child = cast_to<Spatial>(node->get_child(index));
        if (child) {
            add_nodes(child, portals, room_groups, room_group);
        }
    }
}

void RoomManager::add_room(
    Spatial* node,
    LocalVector<Portal*>& portals,
    const LocalVector<RoomGroup*>& room_groups,
    const int room_group
) {
    auto* room = cast_to<Room>(node);
    if (room && room->_conversion_tick == conversion_count) {
        return;
    }
    if (!room) {
        room = convert_node_to<Room>(node, "G");
    }

    room->clear();
    room->_conversion_tick = conversion_count;
    if (room_group != -1) {
        room->_roomgroups.push_back(room_group);
        room->_room_priority = room_groups[room_group]->_settings_priority;
        VisualServer::get_singleton()->room_prepare(
            room->_room_rid,
            room->_room_priority
        );
    }
    room->_room_ID = rooms.size();
    rooms.push_back(room);
    add_portals(room, room, portals);
}

int RoomManager::add_room_group(
    Spatial* node,
    LocalVector<RoomGroup*>& room_groups
) {
    auto* room_group = cast_to<RoomGroup>(node);
    if (room_group && room_group->_conversion_tick == conversion_count) {
        return room_group->_roomgroup_ID;
    }
    if (!room_group) {
        convert_log("convert_roomgroup : " + node->get_name(), 1);
        room_group = convert_node_to<RoomGroup>(node, "G");
    }

    room_group->clear();
    room_group->_conversion_tick = conversion_count;
    VisualServer::get_singleton()->roomgroup_prepare(
        room_group->_room_group_rid,
        room_group->get_instance_id()
    );
    room_group->_roomgroup_ID = room_groups.size();
    room_groups.push_back(room_group);
    return room_group->_roomgroup_ID;
}

void RoomManager::add_portals(
    Spatial* node,
    Room* room,
    LocalVector<Portal*>& portals
) {
    auto* mesh_instance = cast_to<MeshInstance>(node);
    if (cast_to<Portal>(node)
        || (mesh_instance && node_name_ends_with(mesh_instance, "-portal"))) {
        add_portal(node, portals, room->_room_ID);
    }
    for (int index = 0; index < node->get_child_count(); index++) {
        auto* child = cast_to<Spatial>(node->get_child(index));
        if (child) {
            add_portals(child, room, portals);
        }
    }
}

void RoomManager::add_portal(
    Spatial* node,
    LocalVector<Portal*>& portals,
    const int room_id
) {
    auto* portal = cast_to<Portal>(node);
    if (portal && portal->_conversion_tick == conversion_count) {
        return;
    }
    bool importing_portal = false;
    if (!portal) {
        importing_portal = true;
        portal           = convert_node_to<Portal>(node, "G", false);
        portal->create_from_mesh_instance(cast_to<MeshInstance>(node));
        node->queue_delete();
    }
    portal->clear();
    portal->_importing_portal = importing_portal;
    portal->_conversion_tick  = conversion_count;
    portal->portal_update();
    portal->_linkedroom_ID[0] = room_id;
    portals.push_back(portal);
}

void RoomManager::add_portal_links(LocalVector<Portal*>& portals) {
    for (unsigned int n = 0; n < portals.size(); n++) {
        Portal* portal = portals[n];

        // we have a choice here.
        // If we are importing, we will try linking using the naming convention
        // method. We do this by setting the assigned nodepath if we find the
        // link room, then the resolving links is done in the usual manner from
        // the nodepath.
        if (portal->_importing_portal) {
            String string_link_room_shortname =
                _find_name_before(portal, "-portal");
            String string_link_room = string_link_room_shortname + "-room";

            if (string_link_room_shortname != "") {
                // try the room name plus the postfix first, this will be the
                // most common case during import
                Room* linked_room = Object::cast_to<Room>(
                    rooms_root_node->find_node(string_link_room, true, false)
                );

                // try the short name as a last ditch attempt
                if (!linked_room) {
                    linked_room =
                        Object::cast_to<Room>(rooms_root_node->find_node(
                            string_link_room_shortname,
                            true,
                            false
                        ));
                }

                if (linked_room) {
                    NodePath path = portal->get_path_to(linked_room);
                    portal->set_linked_room_internal(path);
                } else {
                    WARN_PRINT(
                        "Portal link room : " + string_link_room + " not found."
                    );
                    portal_link_room_not_found = true;
                }
            }
        }

        // get the room we are linking from
        int room_from_id = portal->_linkedroom_ID[0];
        if (room_from_id != -1) {
            Room* room_from = rooms[room_from_id];
            portal->resolve_links(rooms, room_from->_room_rid);

            // add the portal id to the room from and the room to.
            // These are used so we can later add the portal geometry to the
            // room bounds.
            room_from->_portals.push_back(n);

            int room_to_id = portal->_linkedroom_ID[1];
            if (room_to_id != -1) {
                Room* room_to = rooms[room_to_id];
                room_to->_portals.push_back(n);

                // make the portal internal if necessary
                portal->_internal =
                    room_from->_room_priority > room_to->_room_priority;
            }
        }
    }
}

bool RoomManager::_bound_findpoints_geom_instance(
    GeometryInstance* p_gi,
    Vector<Vector3>& r_room_pts,
    AABB& r_aabb
) {
    // max opposite extents .. note AABB storing size is rubbish in this aspect
    // it can fail once mesh min is larger than FLT_MAX / 2.
    r_aabb.position = Vector3(FLT_MAX / 2, FLT_MAX / 2, FLT_MAX / 2);
    r_aabb.size     = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

#ifdef MODULE_CSG_ENABLED
    CSGShape* shape = Object::cast_to<CSGShape>(p_gi);
    if (shape) {
        // Shapes will not be up to date on the first frame due to a quirk
        // of CSG - it defers updates to the next frame. So we need to
        // explicitly force an update to make sure the CSG is correct on level
        // load.
        shape->force_update_shape();

        Array arr = shape->get_meshes();
        if (!arr.size()) {
            return false;
        }

        Ref<ArrayMesh> arr_mesh = arr[1];
        if (!arr_mesh.is_valid()) {
            return false;
        }

        if (arr_mesh->get_surface_count() == 0) {
            return false;
        }

        // for converting meshes to world space
        Transform trans = p_gi->get_global_transform();

        for (int surf = 0; surf < arr_mesh->get_surface_count(); surf++) {
            Array arrays = arr_mesh->surface_get_arrays(surf);

            if (!arrays.size()) {
                continue;
            }

            PoolVector<Vector3> vertices = arrays[VS::ARRAY_VERTEX];

            // convert to world space
            for (int n = 0; n < vertices.size(); n++) {
                Vector3 pt_world = trans.xform(vertices[n]);
                r_room_pts.push_back(pt_world);

                // keep the bound up to date
                r_aabb.expand_to(pt_world);
            }

        } // for through the surfaces

        return true;
    } // if csg shape
#endif

    // multimesh
    MultiMeshInstance* mmi = Object::cast_to<MultiMeshInstance>(p_gi);
    if (mmi) {
        Ref<MultiMesh> rmm = mmi->get_multimesh();
        if (!rmm.is_valid()) {
            return false;
        }

        // first get the mesh verts in local space
        LocalVector<Vector3, int32_t> local_verts;
        Ref<Mesh> rmesh = rmm->get_mesh();

        if (rmesh->get_surface_count() == 0) {
            String string;
            string = "MultiMeshInstance '" + mmi->get_name()
                   + "' has no surfaces, ignoring";
            WARN_PRINT(string);
            return false;
        }

        for (int surf = 0; surf < rmesh->get_surface_count(); surf++) {
            Array arrays = rmesh->surface_get_arrays(surf);

            if (!arrays.size()) {
                WARN_PRINT_ONCE("MultiMesh mesh surface with no mesh, ignoring"
                );
                continue;
            }

            const PoolVector<Vector3>& vertices = arrays[VS::ARRAY_VERTEX];

            int count = local_verts.size();
            local_verts.resize(local_verts.size() + vertices.size());

            for (int n = 0; n < vertices.size(); n++) {
                local_verts[count++] = vertices[n];
            }
        }

        if (!local_verts.size()) {
            return false;
        }

        // now we have the local space verts, add a bunch for each instance, and
        // find the AABB
        for (int i = 0; i < rmm->get_instance_count(); i++) {
            Transform trans = rmm->get_instance_transform(i);
            trans           = mmi->get_global_transform() * trans;

            for (int n = 0; n < local_verts.size(); n++) {
                Vector3 pt_world = trans.xform(local_verts[n]);
                r_room_pts.push_back(pt_world);

                // keep the bound up to date
                r_aabb.expand_to(pt_world);
            }
        }
        return true;
    }

    // Sprite3D
    SpriteBase3D* sprite = Object::cast_to<SpriteBase3D>(p_gi);
    if (sprite) {
        Ref<TriangleMesh> tmesh      = sprite->generate_triangle_mesh();
        PoolVector<Vector3> vertices = tmesh->get_vertices();

        // for converting meshes to world space
        Transform trans = p_gi->get_global_transform();

        // convert to world space
        for (int n = 0; n < vertices.size(); n++) {
            Vector3 pt_world = trans.xform(vertices[n]);
            r_room_pts.push_back(pt_world);

            // keep the bound up to date
            r_aabb.expand_to(pt_world);
        }

        return true;
    }

    return false;
}

void RoomManager::_check_portal_for_warnings(
    Portal* p_portal,
    const AABB& p_room_aabb_without_portals
) {
#ifdef TOOLS_ENABLED
    AABB bb = p_room_aabb_without_portals;
    bb      = bb.grow(bb.get_longest_axis_size() * 0.5);

    bool changed = false;

    // far outside the room?
    const Vector3& pos = p_portal->get_global_transform().origin;

    bool old_outside                     = p_portal->_warning_outside_room_aabb;
    p_portal->_warning_outside_room_aabb = !bb.has_point(pos);

    if (p_portal->_warning_outside_room_aabb != old_outside) {
        changed = true;
    }

    if (p_portal->_warning_outside_room_aabb) {
        WARN_PRINT(
            String(p_portal->get_name()) + " possibly in the wrong room."
        );
    }

    // facing wrong way?
    Vector3 offset = pos - bb.get_center();
    real_t dot     = offset.dot(p_portal->_plane.normal);

    bool old_facing                     = p_portal->_warning_facing_wrong_way;
    p_portal->_warning_facing_wrong_way = dot < 0.0;

    if (p_portal->_warning_facing_wrong_way != old_facing) {
        changed = true;
    }

    if (p_portal->_warning_facing_wrong_way) {
        WARN_PRINT(
            String(p_portal->get_name()) + " possibly facing the wrong way."
        );
    }

    // handled later
    p_portal->_warning_autolink_failed = false;

    if (changed) {
        p_portal->update_gizmo();
    }
#endif
}

bool RoomManager::_convert_manual_bound(
    Room* room,
    Spatial* node,
    const LocalVector<Portal*>& portals
) {
    MeshInstance* mesh_instance = cast_to<MeshInstance>(node);
    if (!mesh_instance) {
        return false;
    }
    Vector<Vector3> points;
    if (!add_mesh_instance_points(mesh_instance, points)) {
        return false;
    }
    mesh_instance->set_portal_mode(CullInstance::PORTAL_MODE_IGNORE);
    mesh_instance->hide();
    return _convert_room_hull_preliminary(room, points, portals);
}

bool RoomManager::_convert_room_hull_preliminary(
    Room* room,
    const Vector<Vector3>& room_points,
    const LocalVector<Portal*>& portals
) {
    if (room_points.size() <= 3) {
        return false;
    }
    Geometry::MeshData mesh_data;
    Error error = OK;
    if (room_points.size() > 100000) {
        // If there are too many room points, quickhull will fail or freeze.
        // Use a bounding rectangle instead.
        WARN_PRINT(
            String(room->get_name()) +
            " contains too many vertices to build a convex hull;"
            " using a bound rectangle instead.");
        error = build_room_aabb_convex_hull(room_points, mesh_data);
    } else {
        error = build_room_convex_hull(room, room_points, mesh_data);
    }
    if (error != OK) {
        return false;
    }

    // Add existing portals planes first, because
    // these will trump any other planes further out.
    for (int index = 0; index < room->_portals.size(); index++) {
        const Portal* portal = portals[room->_portals[index]];
        if (portal->is_portal_internal(room->_room_ID)) {
            continue;
        }
        Plane plane = portal->_plane;
        // Reverse incoming portals.
        if (portal->_linkedroom_ID[1] == room->_room_ID) {
            plane = -plane;
        }
        add_plane_if_unique(room, room->_preliminary_planes, plane);
    }

    // Add the planes from the geometry or manual bound
    for (int index = 0; index < mesh_data.faces.size(); index++) {
        const Plane& plane = mesh_data.faces[index].plane;
        add_plane_if_unique(room, room->_preliminary_planes, plane);
    }

    // temporary copy of mesh data for the boundary points
    // to form a new hull in _convert_room_hull_final
    room->_bound_mesh_data = mesh_data;

    // aabb (should later include portals too, these are added in
    // _convert_room_hull_final)
    room->_aabb.create_from_points(mesh_data.vertices);

    return true;
}

void RoomManager::_find_statics_recursive(
    Room* p_room,
    Spatial* p_node,
    Vector<Vector3>& r_room_pts,
    bool p_add_to_portal_renderer
) {
    // don't process portal MeshInstances that are being deleted
    // (and replaced by proper Portal nodes)
    if (p_node->is_queued_for_deletion()) {
        return;
    }

    _process_static(p_room, p_node, r_room_pts, p_add_to_portal_renderer);

    for (int n = 0; n < p_node->get_child_count(); n++) {
        Spatial* child = Object::cast_to<Spatial>(p_node->get_child(n));

        if (child) {
            _find_statics_recursive(
                p_room,
                child,
                r_room_pts,
                p_add_to_portal_renderer
            );
        }
    }
}

void RoomManager::_process_static(
    Room* p_room,
    Spatial* p_node,
    Vector<Vector3>& r_room_pts,
    bool p_add_to_portal_renderer
) {
    bool ignore        = false;
    VisualInstance* vi = Object::cast_to<VisualInstance>(p_node);

    bool is_dynamic = false;

    // we are only interested in VIs with static or dynamic mode
    if (vi) {
        switch (vi->get_portal_mode()) {
            default: {
                ignore = true;
            } break;
            case CullInstance::PORTAL_MODE_DYNAMIC: {
                is_dynamic = true;
            } break;
            case CullInstance::PORTAL_MODE_STATIC:
                break;
        }
    }

    if (!ignore) {
        // We'll have a done flag. This isn't strictly speaking necessary
        // because the types should be mutually exclusive, but this would
        // break if something changes the inheritance hierarchy of the nodes
        // at a later date, so having a done flag makes it more robust.
        bool done = false;

        Light* light = Object::cast_to<Light>(p_node);

        if (!done && light) {
            done = true;

            // lights (don't affect bound, so aren't added in first pass)
            if (p_add_to_portal_renderer) {
                Vector<Vector3> dummy_pts;
                VisualServer::get_singleton()->room_add_instance(
                    p_room->_room_rid,
                    light->get_instance(),
                    light->get_transformed_aabb(),
                    dummy_pts
                );
                convert_log("\t\t\tLIGT\t" + light->get_name());
            }
        }

        GeometryInstance* gi = Object::cast_to<GeometryInstance>(p_node);

        if (!done && gi) {
            done = true;

            // MeshInstance is the most interesting type for portalling, so we
            // handle this explicitly
            MeshInstance* mi = Object::cast_to<MeshInstance>(p_node);
            if (mi) {
                if (p_add_to_portal_renderer) {
                    convert_log("\t\t\tMESH\t" + mi->get_name());
                }

                Vector<Vector3> object_pts;
                // get the object points and don't immediately add to the room
                // points, as we want to use these points for sprawling
                // algorithm in the visual server.
                if (add_mesh_instance_points(mi, object_pts)) {
                    // need to keep track of room bound
                    // NOTE the is_visible check MAY cause problems if
                    // conversion run on nodes that aren't properly in the tree.
                    // It can optionally be removed. Certainly calling
                    // is_visible_in_tree DID cause problems.
                    if (!is_dynamic && mi->get_include_in_bound()
                        && mi->is_visible()) {
                        r_room_pts.append_array(object_pts);
                    }

                    if (p_add_to_portal_renderer) {
                        VisualServer::get_singleton()->room_add_instance(
                            p_room->_room_rid,
                            mi->get_instance(),
                            mi->get_transformed_aabb(),
                            object_pts
                        );
                    }
                } // if bound found points
            } else {
                // geometry instance but not a mesh instance ..
                if (p_add_to_portal_renderer) {
                    convert_log("\t\t\tGEOM\t" + gi->get_name());
                }

                Vector<Vector3> object_pts;
                AABB aabb;

                // attempt to recognise this GeometryInstance and read back the
                // geometry Note: never attempt to add dynamics to the room aabb
                if (is_dynamic
                    || _bound_findpoints_geom_instance(gi, object_pts, aabb)) {
                    // need to keep track of room bound
                    // NOTE the is_visible check MAY cause problems if
                    // conversion run on nodes that aren't properly in the tree.
                    // It can optionally be removed. Certainly calling
                    // is_visible_in_tree DID cause problems.
                    if (!is_dynamic && gi->get_include_in_bound()
                        && gi->is_visible()) {
                        r_room_pts.append_array(object_pts);
                    }

                    if (p_add_to_portal_renderer) {
                        VisualServer::get_singleton()->room_add_instance(
                            p_room->_room_rid,
                            gi->get_instance(),
                            gi->get_transformed_aabb(),
                            object_pts
                        );
                    }
                } // if bound found points
            }
        } // if gi

        VisibilityNotifier* vn = Object::cast_to<VisibilityNotifier>(p_node);
        if (!done && vn
            && ((vn->get_portal_mode() == CullInstance::PORTAL_MODE_DYNAMIC)
                || (vn->get_portal_mode() == CullInstance::PORTAL_MODE_STATIC)
            )) {
            done = true;

            if (p_add_to_portal_renderer) {
                AABB world_aabb =
                    vn->get_global_transform().xform(vn->get_aabb());
                VisualServer::get_singleton()->room_add_ghost(
                    p_room->_room_rid,
                    vn->get_instance_id(),
                    world_aabb
                );
                convert_log("\t\t\tVIS \t" + vn->get_name());
            }
        }

    } // if not ignore
}

void RoomManager::add_room_bounds(
    Room* room,
    const LocalVector<RoomGroup*>& room_groups,
    const LocalVector<Portal*>& portals
) {
    bool manual_bound_found = false;
    Vector<Vector3> room_points;
    for (int index = 0; index < room->get_child_count(); index++) {
        Spatial* child = cast_to<Spatial>(room->get_child(index));
        if (!child || child->is_queued_for_deletion()) {
            continue;
        }
        if (cast_to<Portal>(child)) {
            // Adding portal points is done later, because
            // We need incoming and outgoing portals.
            continue;
        }
        if (node_name_ends_with(child, "-bound")) {
            manual_bound_found = _convert_manual_bound(room, child, portals);
        } else {
            // don't add the instances to the portal renderer on the first
            // pass of _find_statics, just find the geometry points in order
            // to make sure the bound is correct.
            _find_statics_recursive(room, child, room_points, false);
        }
    }

    // Has the bound been specified using points in the room?
    // in that case, overwrite the room_pts
    if (room->_bound_pts.size() && room->is_inside_tree()) {
        Transform tr = room->get_global_transform();

        room_points.clear();
        room_points.resize(room->_bound_pts.size());
        for (int n = 0; n < room_points.size(); n++) {
            room_points.set(n, tr.xform(room->_bound_pts[n]));
        }

        // we override and manual bound with the room points
        manual_bound_found = false;
    }

    if (!manual_bound_found) {
        // rough aabb for checking portals for warning conditions
        AABB aabb;
        aabb.create_from_points(room_points);

        for (int n = 0; n < room->_portals.size(); n++) {
            int portal_id  = room->_portals[n];
            Portal* portal = portals[portal_id];

            // only checking portals out from source room
            if (portal->_linkedroom_ID[0] != room->_room_ID) {
                continue;
            }

            // don't add portals to the world bound that are internal to this
            // room!
            if (portal->is_portal_internal(room->_room_ID)) {
                continue;
            }

            // check portal for suspect conditions, like a long way from the
            // room AABB, or possibly flipped the wrong way
            _check_portal_for_warnings(portal, aabb);
        }

        // create convex hull
        _convert_room_hull_preliminary(room, room_points, portals);
    }

    // add the room to roomgroups
    for (int index = 0; index < room->_roomgroups.size(); index++) {
        int room_group_id = room->_roomgroups[index];
        room_groups[room_group_id]->add_room(room);
    }
}

void RoomManager::create_room_statics(
    const LocalVector<RoomGroup*>& room_groups,
    const LocalVector<Portal*>& portals
) {
    for (int index = 0; index < rooms.size(); index++) {
        Room* room = rooms[index];
        if (merge_meshes) {
            merge_room_meshes(room);
        }
        add_room_bounds(room, room_groups, portals);
    }
}

void RoomManager::finalize_portals(
    Spatial* p_roomlist,
    LocalVector<Portal*>& r_portals
) {
    for (unsigned int n = 0; n < r_portals.size(); n++) {
        Portal* portal = r_portals[n];

        // all portals should have a source room
        DEV_ASSERT(portal->_linkedroom_ID[0] != -1);
        const Room* source_room = rooms[portal->_linkedroom_ID[0]];

        if (portal->_linkedroom_ID[1] != -1) {
            // already manually linked
            continue;
        }

        bool autolink_found = false;

        // try to autolink
        // try points iteratively out from the portal center and find the first
        // that is in a room that isn't the source room
        for (int attempt = 0; attempt < 4; attempt++) {
            // found
            if (portal->_linkedroom_ID[1] != -1) {
                break;
            }

            // these numbers are arbitrary .. we could alternatively reuse the
            // portal margins for this?
            real_t dist = 0.01f;
            switch (attempt) {
                default: {
                    dist = 0.01f;
                } break;
                case 1: {
                    dist = 0.1f;
                } break;
                case 2: {
                    dist = 1;
                } break;
                case 3: {
                    dist = 2;
                } break;
            }

            Vector3 test_pos =
                portal->_pt_center_world + (dist * portal->_plane.normal);

            int best_priority = -1000;
            int best_room     = -1;

            for (int r = 0; r < rooms.size(); r++) {
                Room* room = rooms[r];
                if (room->_room_ID == portal->_linkedroom_ID[0]) {
                    // can't link back to the source room
                    continue;
                }

                // first do a rough aabb check
                if (!room->_aabb.has_point(test_pos)) {
                    continue;
                }

                bool outside = false;
                for (int p = 0; p < room->_preliminary_planes.size(); p++) {
                    const Plane& plane = room->_preliminary_planes[p];
                    if (plane.distance_to(test_pos) > 0.0) {
                        outside = true;
                        break;
                    }
                } // for through planes

                if (!outside) {
                    // we found a suitable room, but we want the highest
                    // priority in case there are internal rooms...
                    if (room->_room_priority > best_priority) {
                        best_priority = room->_room_priority;
                        best_room     = r;
                    }
                }

            } // for through rooms

            // found a suitable link room
            if (best_room != -1) {
                Room* room = rooms[best_room];

                // great, we found a linked room!
                convert_log(
                    "\t\tAUTOLINK OK from " + source_room->get_name() + " to "
                        + room->get_name(),
                    1
                );
                portal->_linkedroom_ID[1] = best_room;

                // add the portal to the portals list for the receiving room
                room->_portals.push_back(n);

                // send complete link to visual server so the portal will be
                // active in the visual server room system
                VisualServer::get_singleton()->portal_link(
                    portal->_portal_rid,
                    source_room->_room_rid,
                    room->_room_rid,
                    portal->_settings_two_way
                );

                // make the portal internal if necessary
                // (this prevents the portal plane clipping the room bound)
                portal->_internal =
                    source_room->_room_priority > room->_room_priority;

                autolink_found = true;
                break;
            }

        } // for attempt

        // error condition
        if (!autolink_found) {
            WARN_PRINT(
                "Portal AUTOLINK failed for " + portal->get_name() + " from "
                + source_room->get_name()
            );
            portal_autolink_failed = true;

#ifdef TOOLS_ENABLED
            portal->_warning_autolink_failed = true;
            portal->update_gizmo();
#endif
        }
    } // for portal
}

void RoomManager::_build_simplified_bound(
    const Room* p_room,
    Geometry::MeshData& r_md,
    LocalVector<Plane, int32_t>& r_planes,
    int p_num_portal_planes
) {
    if (!r_planes.size()) {
        return;
    }

    Vector<Vector3> pts = Geometry::compute_convex_mesh_points(
        &r_planes[0],
        r_planes.size(),
        0.001f
    );
    Error err = build_room_convex_hull(p_room, pts, r_md);

    if (err != OK) {
        WARN_PRINT("QuickHull failed building simplified bound");
        return;
    }

    // if the number of faces is less than the number of planes, we can use this
    // simplified version to reduce the number of planes
    if (r_md.faces.size() < r_planes.size()) {
        // always include the portal planes
        r_planes.resize(p_num_portal_planes);

        for (int n = 0; n < r_md.faces.size(); n++) {
            add_plane_if_unique(p_room, r_planes, r_md.faces[n].plane);
        }
    }
}

bool RoomManager::_convert_room_hull_final(
    Room* p_room,
    const LocalVector<Portal*>& p_portals
) {
    Vector<Vector3> vertices_including_portals =
        p_room->_bound_mesh_data.vertices;

    // add the portals planes first, as these will trump any other existing
    // planes further out
    int num_portals_added = 0;

    for (int n = 0; n < p_room->_portals.size(); n++) {
        int portal_id  = p_room->_portals[n];
        Portal* portal = p_portals[portal_id];

        // don't add portals to the world bound that are internal to this room!
        if (portal->is_portal_internal(p_room->_room_ID)) {
            continue;
        }

        Plane plane = portal->_plane;

        // does it need to be reversed? (i.e. is the portal incoming rather than
        // outgoing)
        if (portal->_linkedroom_ID[1] == p_room->_room_ID) {
            plane = -plane;
        }

        if (add_plane_if_unique(p_room, p_room->_planes, plane)) {
            num_portals_added++;
        }

        // add any new portals to the aabb of the room
        for (int p = 0; p < portal->_pts_world.size(); p++) {
            const Vector3& pt = portal->_pts_world[p];
            vertices_including_portals.push_back(pt);
            p_room->_aabb.expand_to(pt);
        }
    }

    // create new convex hull
    Geometry::MeshData md;
    Error err = build_room_convex_hull(p_room, vertices_including_portals, md);

    if (err != OK) {
        return false;
    }

    // add the planes from the new hull
    for (int n = 0; n < md.faces.size(); n++) {
        const Plane& p = md.faces[n].plane;
        add_plane_if_unique(p_room, p_room->_planes, p);
    }

    // recreate the points within the new simplified bound, and then recreate
    // the convex hull by running quickhull a second time... (this enables the
    // gizmo to accurately show the simplified hull)
    int num_planes_before_simplification = p_room->_planes.size();
    Geometry::MeshData md_simplified;
    _build_simplified_bound(
        p_room,
        md_simplified,
        p_room->_planes,
        num_portals_added
    );

    if (num_planes_before_simplification != p_room->_planes.size()) {
        convert_log(
            "\t\t\tcontained " + itos(num_planes_before_simplification)
            + " planes before simplification, " + itos(p_room->_planes.size())
            + " planes after."
        );
    }

    // make a copy of the mesh data for debugging
    // note this could be avoided in release builds? NYI
    p_room->_bound_mesh_data = md_simplified;

    // send bound to visual server
    VisualServer::get_singleton()->room_set_bound(
        p_room->_room_rid,
        p_room->get_instance_id(),
        p_room->_planes,
        p_room->_aabb,
        md_simplified.vertices
    );

    return true;
}

void RoomManager::finalize_rooms(const LocalVector<Portal*>& p_portals) {
    bool found_errors = false;

    for (int n = 0; n < rooms.size(); n++) {
        Room* room = rooms[n];

        // no need to do all these string operations if we are not debugging and
        // don't need logs
        if (debug_logging) {
            String room_short_name = _find_name_before(room, "-room", true);
            convert_log("ROOM\t" + room_short_name);

            // log output the portals associated with this room
            for (int p = 0; p < room->_portals.size(); p++) {
                const Portal& portal = *p_portals[room->_portals[p]];

                bool portal_links_out =
                    portal._linkedroom_ID[0] == room->_room_ID;

                int linked_room_id = (portal_links_out)
                                       ? portal._linkedroom_ID[1]
                                       : portal._linkedroom_ID[0];

                // this shouldn't be out of range, but just in case
                if ((linked_room_id >= 0) && (linked_room_id < rooms.size())) {
                    Room* linked_room = rooms[linked_room_id];

                    String portal_link_room_name =
                        _find_name_before(linked_room, "-room", true);
                    String in_or_out = (portal_links_out) ? "POUT" : "PIN ";

                    // display the name of the room linked to
                    convert_log(
                        "\t\t" + in_or_out + "\t" + portal_link_room_name
                    );
                } else {
                    WARN_PRINT_ONCE("linked_room_id is out of range");
                }
            }

        } // if _show_debug

        // do a second pass finding the statics, where they are
        // finally added to the rooms in the portal_renderer.
        Vector<Vector3> room_pts;

        // the true indicates we DO want to add to the portal renderer this
        // second time we call _find_statics_recursive
        _find_statics_recursive(room, room, room_pts, true);

        if (!_convert_room_hull_final(room, p_portals)) {
            found_errors = true;
        }
        room->update_gizmo();
        room->update_configuration_warning();
    }

    if (found_errors) {
        show_alert(
            TTR("Error calculating room bounds.\n"
                "Ensure all rooms contain geometry or manual bounds.")
        );
    }
}

void RoomManager::place_remaining_statics(Spatial* p_node) {
    if (p_node->is_queued_for_deletion()) {
        return;
    }

    // as soon as we hit a room, quit the recursion as the objects
    // will already have been added inside rooms
    if (Object::cast_to<Room>(p_node)) {
        return;
    }

    VisualInstance* vi = Object::cast_to<VisualInstance>(p_node);

    // we are only interested in VIs with static or dynamic mode
    if (vi) {
        switch (vi->get_portal_mode()) {
            default: {
            } break;
            case CullInstance::PORTAL_MODE_DYNAMIC:
            case CullInstance::PORTAL_MODE_STATIC: {
                _autoplace_object(vi);
            } break;
        }
    }

    for (int n = 0; n < p_node->get_child_count(); n++) {
        Spatial* child = Object::cast_to<Spatial>(p_node->get_child(n));

        if (child) {
            place_remaining_statics(child);
        }
    }
}

bool RoomManager::_autoplace_object(VisualInstance* p_vi) {
    // note we could alternatively use the portal_renderer to do this more
    // efficiently (as it has a BSP) but at a cost of returning result from the
    // visual server
    AABB bb        = p_vi->get_transformed_aabb();
    Vector3 centre = bb.get_center();

    // in order to deal with internal rooms, we can't just stop at the first
    // room the point is within, as there could be later rooms with a higher
    // priority
    int best_priority = -INT32_MAX;
    Room* best_room   = nullptr;

    // if not set to zero (no preference) this can override a preference
    // for a certain RoomGroup priority to ensure the instance gets placed in
    // the correct RoomGroup (e.g. outside, for building exteriors)
    int preferred_priority = p_vi->get_portal_autoplace_priority();

    for (int n = 0; n < rooms.size(); n++) {
        Room* room = rooms[n];

        if (room->contains_point(centre)) {
            // the standard routine autoplaces in the highest priority room
            if (room->_room_priority > best_priority) {
                best_priority = room->_room_priority;
                best_room     = room;
            }

            // if we override the preferred priority we always choose this
            if (preferred_priority
                && (room->_room_priority == preferred_priority)) {
                best_room = room;
                break;
            }
        }
    }

    if (best_room) {
        // just dummies, we won't use these this time
        Vector<Vector3> room_pts;

        // we can reuse this function
        _process_static(best_room, p_vi, room_pts, true);
        return true;
    }

    return false;
}

bool RoomManager::add_plane_if_unique(
    const Room* room,
    LocalVector<Plane, int32_t>& planes,
    const Plane& plane
) const {
    if (room->_use_default_simplify) {
        return room_simplify_info.add_plane_if_unique(planes, plane);
    }
    return room->_simplify_info.add_plane_if_unique(planes, plane);
}

Error RoomManager::build_room_convex_hull(
    const Room* room,
    const Vector<Vector3>& points,
    Geometry::MeshData& mesh_data
) const {
    // Calculate epsilon based on the simplify value.
    // A value between 0.3 (accurate) and 10.0 (very rough) * UNIT_EPSILON.
    real_t epsilon = 0;
    if (room->_use_default_simplify) {
        epsilon = room_simplify_info._plane_simplify;
    } else {
        epsilon = room->_simplify_info._plane_simplify;
    }
    epsilon *= epsilon;
    epsilon *= 40;
    epsilon += 0.3f;
    epsilon *= UNIT_EPSILON;
    return build_convex_hull(points, mesh_data, epsilon);
}

void RoomManager::_cleanup_after_conversion() {
    for (int n = 0; n < rooms.size(); n++) {
        Room* room = rooms[n];
        room->_portals.reset();
        room->_preliminary_planes.reset();

        // outside the editor, there's no need to keep the data for the convex
        // hull drawing, as it is only used for gizmos.
        if (!Engine::get_singleton()->is_editor_hint()) {
            room->_bound_mesh_data = Geometry::MeshData();
        }
    }
}

void RoomManager::update_portal_gizmos(Spatial* p_node) {
    Portal* portal = Object::cast_to<Portal>(p_node);

    if (portal) {
        portal->update_gizmo();
    }

    // recurse
    for (int n = 0; n < p_node->get_child_count(); n++) {
        Spatial* child = Object::cast_to<Spatial>(p_node->get_child(n));

        if (child) {
            update_portal_gizmos(child);
        }
    }
}

void RoomManager::_list_mergeable_mesh_instances(
    Spatial* p_node,
    LocalVector<MeshInstance*, int32_t>& r_list
) {
    MeshInstance* mi = Object::cast_to<MeshInstance>(p_node);

    if (mi) {
        // only interested in static portal mode meshes
        VisualInstance* vi = Object::cast_to<VisualInstance>(mi);

        // we are only interested in VIs with static or dynamic mode
        if (vi && vi->get_portal_mode() == CullInstance::PORTAL_MODE_STATIC) {
            // disallow for portals or bounds
            // mesh instance portals should be queued for deletion by this
            // point, we don't want to merge portals!
            if (!cast_to<Portal>(mi) && !node_name_ends_with(mi, "-bound")
                && !mi->is_queued_for_deletion()) {
                // only merge if visible
                if (mi->is_inside_tree() && mi->is_visible()) {
                    r_list.push_back(mi);
                }
            }
        }
    }

    for (int n = 0; n < p_node->get_child_count(); n++) {
        Spatial* child = Object::cast_to<Spatial>(p_node->get_child(n));
        if (child) {
            _list_mergeable_mesh_instances(child, r_list);
        }
    }
}

void RoomManager::_merge_log(String p_string) {
    debug_print_line(p_string);
}

void RoomManager::merge_room_meshes(Room* p_room) {
    // only do in running game so as not to lose data
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }

    _merge_log("merging room " + p_room->get_name());

    // list of meshes suitable
    LocalVector<MeshInstance*, int32_t> source_meshes;
    _list_mergeable_mesh_instances(p_room, source_meshes);

    // none suitable
    if (!source_meshes.size()) {
        return;
    }

    _merge_log("\t" + itos(source_meshes.size()) + " source meshes");

    BitFieldDynamic bf;
    bf.create(source_meshes.size(), true);

    for (int n = 0; n < source_meshes.size(); n++) {
        LocalVector<MeshInstance*, int32_t> merge_list;

        // find similar meshes
        MeshInstance* a = source_meshes[n];
        merge_list.push_back(a);

        // may not be necessary
        bf.set_bit(n, true);

        for (int c = n + 1; c < source_meshes.size(); c++) {
            // if not merged already
            if (!bf.get_bit(c)) {
                MeshInstance* b = source_meshes[c];
                if (a->is_mergeable_with(*b)) {
                    merge_list.push_back(b);
                    bf.set_bit(c, true);
                }
            }
        }

        // only merge if more than 1
        if (merge_list.size() > 1) {
            // we can merge!
            // create a new holder mesh

            MeshInstance* merged = memnew(MeshInstance);
            merged->set_name("MergedMesh");

            _merge_log("\t\t" + merged->get_name());

            if (merged->create_by_merging(merge_list)) {
                // set all the source meshes to portal mode ignore so not shown
                for (int i = 0; i < merge_list.size(); i++) {
                    merge_list[i]->set_portal_mode(
                        CullInstance::PORTAL_MODE_IGNORE
                    );
                }

                // and set the new merged mesh to static
                merged->set_portal_mode(CullInstance::PORTAL_MODE_STATIC);

                // attach to scene tree
                p_room->add_child(merged);
                merged->set_owner(p_room->get_owner());

                // compensate for room transform, as the verts are now in world
                // space
                Transform tr = p_room->get_global_transform();
                tr.affine_invert();
                merged->set_transform(tr);

                // delete originals?
                // note this isn't perfect, it may still end up with dangling
                // spatials, but they can be deleted later.
                for (int i = 0; i < merge_list.size(); i++) {
                    MeshInstance* mi = merge_list[i];
                    if (!mi->get_child_count()) {
                        mi->queue_delete();
                    } else {
                        Node* parent = mi->get_parent();
                        if (parent) {
                            // if there are children, we don't want to delete
                            // it, but we do want to remove the mesh drawing,
                            // e.g. by replacing it with a spatial
                            String name = mi->get_name();
                            mi->set_name("DeleteMe"
                            ); // can be anything, just to avoid name conflict
                               // with replacement node
                            Spatial* replacement = memnew(Spatial);
                            replacement->set_name(name);

                            parent->add_child(replacement);

                            // make the transform and owner match
                            replacement->set_owner(mi->get_owner());
                            replacement->set_transform(mi->get_transform());

                            // move all children from the mesh instance to the
                            // replacement
                            while (mi->get_child_count()) {
                                Node* child = mi->get_child(0);
                                mi->remove_child(child);
                                replacement->add_child(child);
                            }

                        } // if the mesh instance has a parent (should hopefully
                          // be always the case?)
                    }
                }

            } else {
                // no success
                memdelete(merged);
            }
        }

    } // for n through primary mesh

    if (remove_danglers) {
        _remove_redundant_dangling_nodes(p_room);
    }
}

bool RoomManager::_remove_redundant_dangling_nodes(Spatial* p_node) {
    int non_queue_delete_children = 0;

    // do the children first
    for (int n = 0; n < p_node->get_child_count(); n++) {
        Node* node_child = p_node->get_child(n);

        Spatial* child = Object::cast_to<Spatial>(node_child);
        if (child) {
            _remove_redundant_dangling_nodes(child);
        }

        if (node_child && !node_child->is_queued_for_deletion()) {
            non_queue_delete_children++;
        }
    }

    if (!non_queue_delete_children) {
        // only remove true spatials, not derived classes
        if (p_node->get_class_name() == "Spatial") {
            p_node->queue_delete();
            return true;
        }
    }

    return false;
}

void RoomManager::_flip_portals_recursive(Spatial* p_node) {
    Portal* portal = Object::cast_to<Portal>(p_node);

    if (portal) {
        portal->flip();
    }

    for (int n = 0; n < p_node->get_child_count(); n++) {
        Spatial* child = Object::cast_to<Spatial>(p_node->get_child(n));
        if (child) {
            _flip_portals_recursive(child);
        }
    }
}

void RoomManager::_update_gizmos_recursive(Node* p_node) {
    Portal* portal = Object::cast_to<Portal>(p_node);

    if (portal) {
        portal->update_gizmo();
    }

    for (int n = 0; n < p_node->get_child_count(); n++) {
        _update_gizmos_recursive(p_node->get_child(n));
    }
}

template <class T>
T* RoomManager::convert_node_to(
    Spatial* original_node,
    const String& prefix_original,
    const bool delete_original
) {
    Node* parent = original_node->get_parent();
    if (!parent) {
        return nullptr;
    }
    String node_name = original_node->get_name();
    Node* owner      = original_node->get_owner();
    // Change the name of the original node.
    original_node->set_name(prefix_original + node_name);
    T* new_node = memnew(T);
    new_node->set_name(node_name);
    // Add the child at the same position as the old node.
    parent->add_child_below_node(original_node, new_node);
    new_node->set_transform(original_node->get_transform());
    // Move original node's children to the new node.
    while (original_node->get_child_count()) {
        Node* child = original_node->get_child(0);
        original_node->remove_child(child);
        new_node->add_child(child);
    }
    // Set the owner of the new node and all it's descendents.
    set_node_and_descendents_owner(new_node, owner);
    if (delete_original) {
        original_node->queue_delete();
    }
    return new_node;
}

void RoomManager::convert_log(String p_string, int p_priority) {
    debug_print_line(p_string, 1);
}

void RoomManager::debug_print_line(String p_string, int p_priority) {
    if (debug_logging) {
        if (!p_priority) {
            print_verbose(p_string);
        } else {
            print_line(p_string);
        }
    }
}

#ifdef TOOLS_ENABLED
void RoomManager::_generate_room_overlap_zones() {
    for (int n = 0; n < rooms.size(); n++) {
        Room* room = rooms[n];

        // no planes .. no overlap
        if (!room->_planes.size()) {
            continue;
        }

        for (int c = n + 1; c < rooms.size(); c++) {
            if (c == n) {
                continue;
            }
            Room* other = rooms[c];

            // do a quick reject AABB
            if (!room->_aabb.intersects(other->_aabb)
                || (!other->_planes.size())) {
                continue;
            }

            // if the room priorities are different (i.e. an internal room),
            // they are allowed to overlap
            if (room->_room_priority != other->_room_priority) {
                continue;
            }

            // get all the planes of both rooms in a contiguous list
            LocalVector<Plane, int32_t> planes;
            planes.resize(room->_planes.size() + other->_planes.size());
            Plane* dest = planes.ptr();
            memcpy(
                dest,
                &room->_planes[0],
                room->_planes.size() * sizeof(Plane)
            );
            dest += room->_planes.size();

            memcpy(
                dest,
                &other->_planes[0],
                other->_planes.size() * sizeof(Plane)
            );

            Vector<Vector3> overlap_pts = Geometry::compute_convex_mesh_points(
                planes.ptr(),
                planes.size()
            );

            if (overlap_pts.size() < 4) {
                continue;
            }

            // there is an overlap, create a mesh from the points
            Geometry::MeshData md;
            Error err = build_convex_hull(overlap_pts, md);

            if (err != OK) {
                WARN_PRINT("QuickHull failed building room overlap hull");
                continue;
            }

            // only if the volume is more than some threshold
            real_t volume = Geometry::calculate_convex_hull_volume(md);
            if (volume > overlap_warning_threshold) {
                WARN_PRINT(
                    "Room overlap of " + String(Variant(volume))
                    + " detected between " + room->get_name() + " and "
                    + other->get_name()
                );
                room->_gizmo_overlap_zones.push_back(md);
                room_overlap_detected = true;
            }
        }
    }
}
#endif
