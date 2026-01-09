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

// Static function declarations.
// Build convex hulls.
static Error build_best_room_convex_hull(
    const Room* room,
    const Vector<Vector3>& points,
    Geometry::MeshData& mesh_data,
    real_t default_simplify
);
static Error build_aabb_convex_hull(
    const Vector<Vector3>& room_points,
    Geometry::MeshData& mesh_data
);
static Error build_room_convex_hull(
    const Room* room,
    const Vector<Vector3>& room_points,
    Geometry::MeshData& mesh_data,
    real_t default_simplify
);
static Error build_quick_hull(
    const Vector<Vector3>& points,
    Geometry::MeshData& mesh,
    real_t epsilon = 3.0 * UNIT_EPSILON
);

// Create statics.
static void add_mergeable_mesh_instances(
    Spatial* p_node,
    LocalVector<MeshInstance*, int32_t>& mergeable_mesh_instances
);
static void add_mesh_instance_to_room(MeshInstance* mesh_instance, Room* room);
static LocalVector<MeshInstance*, int32_t> get_merging_instances(
    LocalVector<MeshInstance*, int32_t>& mergeable_mesh_instances,
    BitFieldDynamic& bit_field_dynamic,
    int start_index
);
static void merge_room_meshes(
    Room* room,
    bool debug_logging,
    bool remove_danglers
);
static void remove_meshes(LocalVector<MeshInstance*, int32_t>& mesh_instances);
static void remove_redundant_dangling_nodes(Spatial* node);

// Get vertices.
static void add_visual_instance_points(
    VisualInstance* visual_instance,
    Vector<Vector3>& points
);
static void add_visual_instances_points(
    Spatial* spatial,
    Vector<Vector3>& points
);
#ifdef MODULE_CSG_ENABLED
static Vector<Vector3> get_csg_shape_points(CSGShape* shape);
#endif // MODULE_CSG_ENABLED
static Vector<Vector3> get_mesh_instance_points(
    const MeshInstance* mesh_instance
);
static Vector<Vector3> get_multi_mesh_instance_points(
    const MultiMeshInstance* multi_mesh_instance
);
static Vector<Vector3> get_sprite_points(const SpriteBase3D* sprite3d);
static Vector<Vector3> get_geometry_instance_points(
    GeometryInstance* geometry_instance
);

// Process Visual Instances.
static void process_geometry_instance(
    GeometryInstance* geometry_instance,
    RID room_rid
);
static void process_mesh_instance(
    const MeshInstance* mesh_instance,
    RID room_id
);
static void process_room_light_node(const Light* light, RID room_rid);
static void process_visibility_notifier(
    const VisibilityNotifier* visibility_notifier,
    RID room_rid
);
static void process_visual_instance(
    VisualInstance* visual_instance,
    RID room_rid,
    bool debug_logging
);
static void process_visual_instances(
    Spatial* spatial,
    RID room_rid,
    bool debug_logging
);

static bool node_name_ends_with(const Node* node, const String& suffix);
static String remove_suffix(
    const String& name,
    const String& suffix,
    bool allow_empty_suffix = false
);
static void set_node_and_descendents_owner(Node* node, Node* owner);
static void update_gizmos(Node* node);

// Static function definitions.
// Build convex hulls.
static Error build_best_room_convex_hull(
    const Room* room,
    const Vector<Vector3>& points,
    Geometry::MeshData& mesh_data,
    const real_t default_simplify
) {
    if (points.size() > 100000) {
        // If there are too many points, quickhull will fail or freeze.
        WARN_PRINT(
            String(room->get_name()) +
            " contains too many points to build a convex hull;"
            " using an AABB instead.");
        return build_aabb_convex_hull(points, mesh_data);
    }
    return build_room_convex_hull(room, points, mesh_data, default_simplify);
}

static Error build_aabb_convex_hull(
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
    return build_quick_hull(points, mesh_data);
}

static Error build_room_convex_hull(
    const Room* room,
    const Vector<Vector3>& room_points,
    Geometry::MeshData& mesh_data,
    const real_t default_simplify
) {
    // Calculate epsilon based on the simplify value.
    // A value between 0.3 (accurate) and 10.0 (very rough) * UNIT_EPSILON.
    real_t epsilon = 0;
    if (room->get_use_default_simplify()) {
        epsilon = default_simplify;
    } else {
        epsilon = room->get_room_simplify();
    }
    epsilon *= epsilon;
    epsilon *= 40;
    epsilon += 0.3f;
    epsilon *= UNIT_EPSILON;
    return build_quick_hull(room_points, mesh_data, epsilon);
}

static Error build_quick_hull(
    const Vector<Vector3>& points,
    Geometry::MeshData& mesh,
    const real_t epsilon
) {
    QuickHull::_flag_warnings = false;
    const Error error         = QuickHull::build(points, mesh, epsilon);
    QuickHull::_flag_warnings = true;
    return error;
}

// Create statics.
static void add_mergeable_mesh_instances(
    Spatial* p_node,
    LocalVector<MeshInstance*, int32_t>& mergeable_mesh_instances
) {
    for (int index = 0; index < p_node->get_child_count(); index++) {
        auto* child = Object::cast_to<Spatial>(p_node->get_child(index));
        if (child) {
            add_mergeable_mesh_instances(child, mergeable_mesh_instances);
        }
    }

    auto* mesh_instance = Object::cast_to<MeshInstance>(p_node);
    if (!mesh_instance || !mesh_instance->is_inside_tree()
        || !mesh_instance->is_visible()
        || mesh_instance->is_queued_for_deletion()) {
        return;
    }
    if (mesh_instance->get_portal_mode() != CullInstance::PORTAL_MODE_STATIC) {
        return;
    }
    if (Object::cast_to<Portal>(mesh_instance)) {
        return;
    }
    if (node_name_ends_with(mesh_instance, "-bound")) {
        return;
    }
    mergeable_mesh_instances.push_back(mesh_instance);
}

static void add_mesh_instance_to_room(MeshInstance* mesh_instance, Room* room) {
    // Make mesh static.
    mesh_instance->set_portal_mode(CullInstance::PORTAL_MODE_STATIC);
    // Attach mesh to room.
    room->add_child(mesh_instance);
    mesh_instance->set_owner(room->get_owner());
    // Set mesh transform to room transform.
    Transform room_global_transform = room->get_global_transform();
    room_global_transform.affine_invert();
    mesh_instance->set_transform(room_global_transform);
}

static LocalVector<MeshInstance*, int32_t> get_merging_instances(
    LocalVector<MeshInstance*, int32_t>& mergeable_mesh_instances,
    BitFieldDynamic& bit_field_dynamic,
    const int start_index
) {
    LocalVector<MeshInstance*, int32_t> merging_instances;
    MeshInstance* first_instance = mergeable_mesh_instances[start_index];
    merging_instances.push_back(first_instance);
    bit_field_dynamic.set_bit(start_index, true);
    for (int second_index = start_index + 1;
         second_index < mergeable_mesh_instances.size();
         second_index++) {
        if (!bit_field_dynamic.get_bit(second_index)) {
            MeshInstance* second_instance =
                mergeable_mesh_instances[second_index];
            if (first_instance->is_mergeable_with(*second_instance)) {
                merging_instances.push_back(second_instance);
                bit_field_dynamic.set_bit(second_index, true);
            }
        }
    }
    return merging_instances;
}

static void remove_meshes(LocalVector<MeshInstance*, int32_t>& mesh_instances) {
    for (int index = 0; index < mesh_instances.size(); index++) {
        MeshInstance* mesh_instance = mesh_instances[index];
        // Hide mesh.
        mesh_instance->set_portal_mode(CullInstance::PORTAL_MODE_IGNORE);
        if (!mesh_instance->get_child_count()) {
            mesh_instance->queue_delete();
            continue;
        }
        // Mesh instance has children, so don't delete it.
        // Replace the mesh instance with a spatial.
        Node* parent = mesh_instance->get_parent();
        ERR_CONTINUE_MSG(!parent, "Room mesh instance without a parent!");
        String name = mesh_instance->get_name();
        mesh_instance->set_name("Can be deleted");
        Spatial* replacement = memnew(Spatial);
        replacement->set_name(name);
        parent->add_child(replacement);
        replacement->set_owner(mesh_instance->get_owner());
        replacement->set_transform(mesh_instance->get_transform());
        while (mesh_instance->get_child_count()) {
            Node* child = mesh_instance->get_child(0);
            mesh_instance->remove_child(child);
            replacement->add_child(child);
        }
    }
}

static void remove_redundant_dangling_nodes(Spatial* node) {
    bool child_queued_for_deletion = false;
    for (int child_index = 0; child_index < node->get_child_count();
         child_index++) {
        Node* node_child    = node->get_child(child_index);
        auto* spatial_child = Object::cast_to<Spatial>(node_child);
        if (spatial_child) {
            remove_redundant_dangling_nodes(spatial_child);
        }
        if (node_child && !node_child->is_queued_for_deletion()) {
            child_queued_for_deletion = true;
        }
    }
    if (!child_queued_for_deletion) {
        // Only remove true Spatial, not derived classes.
        if (node->get_class_name() == "Spatial") {
            node->queue_delete();
        }
    }
}

static void merge_room_meshes(
    Room* room,
    const bool debug_logging,
    const bool remove_danglers
) {
    // Only do in a running game so as not to lose data.
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    if (debug_logging) {
        print_verbose("merging room " + room->get_name());
    }
    LocalVector<MeshInstance*, int32_t> mergeable_mesh_instances;
    add_mergeable_mesh_instances(room, mergeable_mesh_instances);
    if (mergeable_mesh_instances.empty()) {
        return;
    }
    if (debug_logging) {
        print_verbose(
            "\t" + itos(mergeable_mesh_instances.size()) + " source meshes"
        );
    }

    BitFieldDynamic bit_field_dynamic;
    bit_field_dynamic.create(mergeable_mesh_instances.size(), true);
    for (int index = 0; index < mergeable_mesh_instances.size(); index++) {
        LocalVector<MeshInstance*, int32_t> merging_instances =
            get_merging_instances(
                mergeable_mesh_instances,
                bit_field_dynamic,
                index
            );
        if (merging_instances.size() < 2) {
            continue;
        }
        MeshInstance* merged_instance = memnew(MeshInstance);
        merged_instance->set_name("MergedMesh");
        if (debug_logging) {
            print_verbose("\t\t" + merged_instance->get_name());
        }
        if (merged_instance->create_by_merging(merging_instances)) {
            add_mesh_instance_to_room(merged_instance, room);
            remove_meshes(merging_instances);
        } else {
            // Merging failed.
            memdelete(merged_instance);
        }
    }
    if (remove_danglers) {
        remove_redundant_dangling_nodes(room);
    }
}

// Get vertices.
static void add_visual_instance_points(
    VisualInstance* visual_instance,
    Vector<Vector3>& points
) {
    // Note: Calling is_visible_in_tree caused problems.
    // Calling is_visible may cause problems if nodes aren't in the tree.
    if (!visual_instance->get_include_in_bound()
        || !visual_instance->is_visible()) {
        return;
    }
    const CullInstance::PortalMode mode = visual_instance->get_portal_mode();
    // We only process visual instances in static or dynamic mode.
    if (mode != CullInstance::PORTAL_MODE_STATIC) {
        return;
    }
    auto* geometry_instance =
        Object::cast_to<GeometryInstance>(visual_instance);
    const auto* mesh_instance = Object::cast_to<MeshInstance>(visual_instance);
    if (mesh_instance) {
        points.append_array(get_mesh_instance_points(mesh_instance));
    } else if (geometry_instance) {
        points.append_array(get_geometry_instance_points(geometry_instance));
    }
}

static void add_visual_instances_points(
    Spatial* spatial,
    Vector<Vector3>& points
) {
    if (spatial->is_queued_for_deletion()) {
        return;
    }
    auto* visual_instance = Object::cast_to<VisualInstance>(spatial);
    if (visual_instance) {
        add_visual_instance_points(visual_instance, points);
    }
    for (int index = 0; index < spatial->get_child_count(); index++) {
        auto* child = Object::cast_to<Spatial>(spatial->get_child(index));
        if (child) {
            add_visual_instances_points(child, points);
        }
    }
}

#ifdef MODULE_CSG_ENABLED
static Vector<Vector3> get_csg_shape_points(CSGShape* shape) {
    // CSG shapes only update on the next frame; so, we force an update.
    shape->force_update_shape();
    Array shape_meshes = shape->get_meshes();
    if (shape_meshes.empty()) {
        return {};
    }
    Ref<ArrayMesh> array_mesh = shape_meshes[1];
    if (!array_mesh.is_valid()) {
        return {};
    }
    if (array_mesh->get_surface_count() == 0) {
        return {};
    }

    Vector<Vector3> points;
    const Transform global_transform = shape->get_global_transform();
    for (int surface_index = 0; surface_index < array_mesh->get_surface_count();
         surface_index++) {
        Array surface_arrays = array_mesh->surface_get_arrays(surface_index);
        if (surface_arrays.empty()) {
            continue;
        }
        PoolVector<Vector3> vertices = surface_arrays[VS::ARRAY_VERTEX];
        for (int vertex_index = 0; vertex_index < vertices.size();
             vertex_index++) {
            Vector3 point = global_transform.xform(vertices[vertex_index]);
            points.push_back(point);
        }
    }
    return points;
}
#endif // MODULE_CSG_ENABLED

static Vector<Vector3> get_geometry_instance_points(
    GeometryInstance* geometry_instance
) {
#ifdef MODULE_CSG_ENABLED
    auto* shape = Object::cast_to<CSGShape>(geometry_instance);
    if (shape) {
        return get_csg_shape_points(shape);
    }
#endif // MODULE_CSG_ENABLED
    const auto* multi_mesh_instance =
        Object::cast_to<MultiMeshInstance>(geometry_instance);
    if (multi_mesh_instance) {
        return get_multi_mesh_instance_points(multi_mesh_instance);
    }
    const auto* sprite3d = Object::cast_to<SpriteBase3D>(geometry_instance);
    if (sprite3d) {
        return get_sprite_points(sprite3d);
    }
    return {};
}

static Vector<Vector3> get_mesh_instance_points(
    const MeshInstance* mesh_instance
) {
    Ref<Mesh> mesh = mesh_instance->get_mesh();
    ERR_FAIL_COND_V(!mesh.is_valid(), {});
    ERR_FAIL_COND_V_MSG(
        mesh->get_surface_count() == 0,
        {},
        "MeshInstance '" + mesh_instance->get_name()
            + "' has no surfaces, ignoring"
    );
    Vector<Vector3> points;
    const Transform transform = mesh_instance->get_global_transform();
    for (int surface = 0; surface < mesh->get_surface_count(); surface++) {
        Array arrays = mesh->surface_get_arrays(surface);
        ERR_CONTINUE_MSG(arrays.empty(), "Ignoring mesh surface with no mesh.");
        PoolVector<Vector3> vertices = arrays[VS::ARRAY_VERTEX];
        for (int vertex = 0; vertex < vertices.size(); vertex++) {
            points.push_back(transform.xform(vertices[vertex]));
        }
    }
    return points;
}

static Vector<Vector3> get_multi_mesh_instance_points(
    const MultiMeshInstance* multi_mesh_instance
) {
    Ref<MultiMesh> multi_mesh = multi_mesh_instance->get_multimesh();
    if (!multi_mesh.is_valid()) {
        return {};
    }
    Ref<Mesh> mesh = multi_mesh->get_mesh();
    if (mesh->get_surface_count() == 0) {
        WARN_PRINT(
            "MultiMeshInstance '" + multi_mesh_instance->get_name()
            + "' has no surfaces, ignoring"
        );
        return {};
    }

    LocalVector<Vector3, int32_t> vertices;
    for (int surface_index = 0; surface_index < mesh->get_surface_count();
         surface_index++) {
        Array surface_arrays = mesh->surface_get_arrays(surface_index);
        if (surface_arrays.empty()) {
            WARN_PRINT_ONCE("MultiMesh mesh surface with no mesh, ignoring");
            continue;
        }
        const PoolVector<Vector3>& surface_vertices =
            surface_arrays[VS::ARRAY_VERTEX];
        int count = vertices.size();
        vertices.resize(vertices.size() + surface_vertices.size());
        for (int vertex_index = 0; vertex_index < surface_vertices.size();
             vertex_index++) {
            vertices[count++] = surface_vertices[vertex_index];
        }
    }
    if (vertices.empty()) {
        return {};
    }

    Vector<Vector3> points;
    // For each instance we apply its global transform and add the vertices.
    for (int i = 0; i < multi_mesh->get_instance_count(); i++) {
        Transform instance_transform = multi_mesh->get_instance_transform(i);
        Transform global_transform =
            multi_mesh_instance->get_global_transform() * instance_transform;
        for (int index = 0; index < vertices.size(); index++) {
            const Vector3 point = global_transform.xform(vertices[index]);
            points.push_back(point);
        }
    }
    return points;
}

static Vector<Vector3> get_sprite_points(const SpriteBase3D* sprite3d) {
    Vector<Vector3> points;
    Ref<TriangleMesh> triangle_mesh    = sprite3d->generate_triangle_mesh();
    const PoolVector<Vector3> vertices = triangle_mesh->get_vertices();
    const Transform global_transform   = sprite3d->get_global_transform();
    for (int index = 0; index < vertices.size(); index++) {
        const Vector3 point = global_transform.xform(vertices[index]);
        points.push_back(point);
    }
    return points;
}

// Process Visual Instances.
static void process_geometry_instance(
    GeometryInstance* geometry_instance,
    const RID room_rid
) {
    const Vector<Vector3> geometry_instance_points =
        get_geometry_instance_points(geometry_instance);
    if (geometry_instance_points.empty()) {
        return;
    }
    VisualServer::get_singleton()->room_add_instance(
        room_rid,
        geometry_instance->get_instance(),
        geometry_instance->get_transformed_aabb(),
        geometry_instance_points
    );
}

static void process_mesh_instance(
    const MeshInstance* mesh_instance,
    const RID room_id
) {
    const Vector<Vector3> mesh_instance_points =
        get_mesh_instance_points(mesh_instance);
    if (mesh_instance_points.empty()) {
        return;
    }
    VisualServer::get_singleton()->room_add_instance(
        room_id,
        mesh_instance->get_instance(),
        mesh_instance->get_transformed_aabb(),
        mesh_instance_points
    );
}

static void process_room_light_node(const Light* light, const RID room_rid) {
    const Vector<Vector3> light_has_no_points;
    VisualServer::get_singleton()->room_add_instance(
        room_rid,
        light->get_instance(),
        light->get_transformed_aabb(),
        light_has_no_points
    );
}

static void process_visibility_notifier(
    const VisibilityNotifier* visibility_notifier,
    const RID room_rid
) {
    const AABB aabb = visibility_notifier->get_global_transform().xform(
        visibility_notifier->get_aabb()
    );
    VisualServer::get_singleton()->room_add_ghost(
        room_rid,
        visibility_notifier->get_instance_id(),
        aabb
    );
}

static void process_visual_instance(
    VisualInstance* visual_instance,
    const RID room_rid,
    const bool debug_logging
) {
    const CullInstance::PortalMode mode = visual_instance->get_portal_mode();
    // We only process visual instances in static or dynamic mode.
    if (mode != CullInstance::PORTAL_MODE_STATIC
        && mode != CullInstance::PORTAL_MODE_DYNAMIC) {
        return;
    }
    const auto* light = Object::cast_to<Light>(visual_instance);
    auto* geometry_instance =
        Object::cast_to<GeometryInstance>(visual_instance);
    const auto* mesh_instance = Object::cast_to<MeshInstance>(visual_instance);
    const auto* visibility_notifier =
        Object::cast_to<VisibilityNotifier>(visual_instance);
    if (light) {
        process_room_light_node(light, room_rid);
        if (debug_logging) {
            print_line("\t\t\tLIGT\t" + light->get_name());
        }
    } else if (mesh_instance) {
        if (debug_logging) {
            print_line("\t\t\tMESH\t" + mesh_instance->get_name());
        }
        process_mesh_instance(mesh_instance, room_rid);
    } else if (geometry_instance) {
        if (debug_logging) {
            print_line("\t\t\tGEOM\t" + geometry_instance->get_name());
        }
        process_geometry_instance(geometry_instance, room_rid);
    } else if (visibility_notifier) {
        process_visibility_notifier(visibility_notifier, room_rid);
        if (debug_logging) {
            print_line("\t\t\tVIS \t" + visibility_notifier->get_name());
        }
    }
}

static void process_visual_instances(
    Spatial* spatial,
    RID room_rid,
    const bool debug_logging
) {
    if (spatial->is_queued_for_deletion()) {
        return;
    }
    auto* visual_instance = Object::cast_to<VisualInstance>(spatial);
    if (visual_instance) {
        process_visual_instance(visual_instance, room_rid, debug_logging);
    }
    for (int index = 0; index < spatial->get_child_count(); index++) {
        auto* child = Object::cast_to<Spatial>(spatial->get_child(index));
        if (child) {
            process_visual_instances(child, room_rid, debug_logging);
        }
    }
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

static String remove_suffix(
    const String& name,
    const String& suffix,
    const bool allow_empty_suffix
) {
    const int name_length   = name.length();
    const int suffix_length = suffix.length();

    String result = name;
    if (suffix_length > name_length) {
        if (!allow_empty_suffix) {
            return {};
        }
    } else {
        if (result.substr(name_length - suffix_length, suffix_length)
            == suffix) {
            result = result.substr(0, name_length - suffix_length);
        } else {
            if (!allow_empty_suffix) {
                return {};
            }
        }
    }

    // Rebel Engine doesn't support multiple nodes with the same name.
    // Therefore, we strip everything after a '*'.
    // e.g. kitchen*2-portal -> kitchen*
    for (int index = 0; index < result.length(); index++) {
        if (result[index] == '*') {
            result = result.substr(0, index);
            break;
        }
    }
    return result;
}

static void set_node_and_descendents_owner(Node* node, Node* owner) {
    if (!node->get_owner() && node != owner) {
        node->set_owner(owner);
    }
    for (int child = 0; child < node->get_child_count(); child++) {
        set_node_and_descendents_owner(node->get_child(child), owner);
    }
}

static void update_gizmos(Node* node) {
    auto* portal = Object::cast_to<Portal>(node);
    if (portal) {
        portal->update_gizmo();
    }
    for (int index = 0; index < node->get_child_count(); index++) {
        auto* child = Object::cast_to<Spatial>(node->get_child(index));
        if (child) {
            update_gizmos(child);
        }
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
    auto* room_list       = cast_to<Spatial>(get_node(room_list_node_path));
    if (!room_list) {
        return;
    }
    update_gizmos(room_list);
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
    return default_simplify_info._plane_simplify;
}

void RoomManager::set_room_simplify(real_t new_value) {
    default_simplify_info.set_simplify(new_value);
}

bool RoomManager::get_show_margins() const {
    return Portal::_settings_gizmo_show_margins;
}

void RoomManager::set_show_margins(const bool show) {
    Portal::_settings_gizmo_show_margins = show;
    auto* room_list = cast_to<Spatial>(get_node(room_list_node_path));
    if (!room_list) {
        return;
    }
    update_gizmos(room_list);
}

bool RoomManager::get_use_secondary_pvs() const {
    return use_secondary_pvs;
}

void RoomManager::set_use_secondary_pvs(const bool enabled) {
    use_secondary_pvs = enabled;
}

void RoomManager::rooms_clear() const {
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
    LocalVector<Room*, int32_t> rooms;
    LocalVector<RoomGroup*> room_groups;
    LocalVector<Portal*> portals;
    add_nodes(rooms_root_node, rooms, portals, room_groups);
    if (rooms.empty()) {
        rooms_clear();
        show_alert(TTR("No Rooms found!"));
        return;
    }
    add_portal_links(rooms, portals);
    create_room_statics(
        rooms,
        room_groups,
        portals,
        default_simplify_info,
        merge_meshes,
        debug_logging,
        remove_danglers
    );
    finalize_portals(rooms_root_node, rooms, portals);
    finalize_rooms(rooms, portals);
    place_remaining_statics(rooms_root_node, rooms);

    const bool generate_pvs =
        pvs_mode == PVS_MODE_PARTIAL || pvs_mode == PVS_MODE_FULL;
    const bool cull_pvs = pvs_mode == PVS_MODE_FULL;
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
    VisualServer::get_singleton()->rooms_set_params(
        get_world()->get_scenario(),
        portal_depth_limit,
        roaming_expansion_margin
    );

#ifdef TOOLS_ENABLED
    _generate_room_overlap_zones(rooms);
#endif

    // just delete any intermediate data
    clean_up_rooms(rooms);

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
    Error err = build_room_convex_hull(
        p_room,
        pts,
        md,
        default_simplify_info._plane_simplify
    );

    if (err != OK) {
        return false;
    }

    p_room->_bound_mesh_data = md;
    p_room->update_gizmo();

    return true;
}

void RoomManager::_rooms_changed(String p_reason) {
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
    LocalVector<Room*, int32_t>& rooms,
    LocalVector<Portal*>& portals,
    LocalVector<RoomGroup*>& room_groups,
    int room_group
) {
    if (cast_to<Room>(node) || node_name_ends_with(node, "-room")) {
        add_room(node, rooms, portals, room_groups, room_group);
    }
    if (cast_to<RoomGroup>(node) || node_name_ends_with(node, "-roomgroup")) {
        room_group = add_room_group(node, room_groups);
    }
    for (int index = 0; index < node->get_child_count(); index++) {
        auto* child = cast_to<Spatial>(node->get_child(index));
        if (child) {
            add_nodes(child, rooms, portals, room_groups, room_group);
        }
    }
}

void RoomManager::add_room(
    Spatial* node,
    LocalVector<Room*, int32_t>& rooms,
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
    add_portals(room, room->_room_ID, portals);
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
        if (debug_logging) {
            print_line("convert_roomgroup : " + node->get_name());
        }
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
    const int room_id,
    LocalVector<Portal*>& portals
) {
    auto* mesh_instance = cast_to<MeshInstance>(node);
    if (cast_to<Portal>(node)
        || (mesh_instance && node_name_ends_with(mesh_instance, "-portal"))) {
        add_portal(node, room_id, portals);
    }
    for (int index = 0; index < node->get_child_count(); index++) {
        auto* child = cast_to<Spatial>(node->get_child(index));
        if (child) {
            add_portals(child, room_id, portals);
        }
    }
}

void RoomManager::add_portal(
    Spatial* node,
    const int room_id,
    LocalVector<Portal*>& portals
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

void RoomManager::add_portal_links(
    LocalVector<Room*, int32_t> rooms,
    LocalVector<Portal*>& portals
) {
    for (unsigned int portal_index = 0; portal_index < portals.size();
         portal_index++) {
        Portal* portal = portals[portal_index];
        if (portal->_importing_portal) {
            add_imported_portal_portal_link(portal);
        }
        const int from_room_id = portal->_linkedroom_ID[0];
        const int to_room_id   = portal->_linkedroom_ID[1];

        if (from_room_id == -1) {
            continue;
        }
        Room* from_room = rooms[from_room_id];
        portal->resolve_links(rooms, from_room->_room_rid);
        from_room->_portals.push_back(portal_index);

        if (to_room_id == -1) {
            continue;
        }
        Room* to_room = rooms[to_room_id];
        to_room->_portals.push_back(portal_index);

        portal->_internal = from_room->_room_priority > to_room->_room_priority;
    }
}

void RoomManager::add_imported_portal_portal_link(Portal* portal) {
    const String name = remove_suffix(portal->get_name(), "-portal");
    if (name.empty()) {
        return;
    }
    const String name_room = name + "-room";
    // Try room with same name as portal, but different suffix.
    const auto* linked_room =
        cast_to<Room>(rooms_root_node->find_node(name_room, true, false));
    // Try room with same name as portal without suffix.
    if (!linked_room) {
        linked_room =
            cast_to<Room>(rooms_root_node->find_node(name, true, false));
    }
    if (!linked_room) {
        WARN_PRINT("Portal link room : " + name_room + " not found.");
        portal_link_room_not_found = true;
        return;
    }

    portal->set_linked_room_internal(portal->get_path_to(linked_room));
}

void RoomManager::create_room_statics(
    LocalVector<Room*, int32_t>& rooms,
    const LocalVector<RoomGroup*>& room_groups,
    const LocalVector<Portal*>& portals,
    const Room::SimplifyInfo& default_simplify_info,
    const bool merge_meshes,
    const bool debug_logging,
    const bool remove_danglers
) {
    for (int index = 0; index < rooms.size(); index++) {
        Room* room = rooms[index];
        if (merge_meshes) {
            merge_room_meshes(room, debug_logging, remove_danglers);
        }
        add_room_bounds(room, portals, default_simplify_info);
        add_room_to_room_groups(room, room_groups);
    }
}

void RoomManager::add_room_bounds(
    Room* room,
    const LocalVector<Portal*>& portals,
    const Room::SimplifyInfo& default_simplify_info
) {
    Vector<Vector3> room_points;
    bool manual_bound_found =
        get_room_points(room, room_points, portals, default_simplify_info);

    // Has the bound been specified using points in the room?
    // in that case, overwrite the room_pts
    if (!room->_bound_pts.empty() && room->is_inside_tree()) {
        update_room_points(room, room_points);
        // we override and manual bound with the room points
        manual_bound_found = false;
    }

    if (!manual_bound_found) {
        // rough aabb for checking portals for warning conditions
        AABB aabb;
        aabb.create_from_points(room_points);

        for (int index = 0; index < room->_portals.size(); index++) {
            const int portal_id = room->_portals[index];
            Portal* portal      = portals[portal_id];
            // Only checking portals out from source room
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
            check_portal_for_warnings(portal, aabb);
        }

        // create convex hull
        convert_room_hull_preliminary(
            room,
            room_points,
            portals,
            default_simplify_info
        );
    }
}

void RoomManager::check_portal_for_warnings(
    Portal* portal,
    const AABB& room_aabb
) {
#ifdef TOOLS_ENABLED
    const AABB enlarged_aabb =
        room_aabb.grow(room_aabb.get_longest_axis_size() * 0.5);
    bool changed = false;

    // Is portal far outside the room?
    const Vector3& portal_position = portal->get_global_transform().origin;
    bool was_outside               = portal->_warning_outside_room_aabb;
    portal->_warning_outside_room_aabb =
        !enlarged_aabb.has_point(portal_position);
    if (portal->_warning_outside_room_aabb != was_outside) {
        changed = true;
    }
    if (portal->_warning_outside_room_aabb) {
        WARN_PRINT(
            String(portal->get_name()) + " is possibly in the wrong room."
        );
    }

    // Is portal facing the wrong way?
    const Vector3 offset      = portal_position - enlarged_aabb.get_center();
    const real_t dot          = offset.dot(portal->_plane.normal);
    bool was_facing_wrong_way = portal->_warning_facing_wrong_way;
    portal->_warning_facing_wrong_way = dot < 0.0;
    if (portal->_warning_facing_wrong_way != was_facing_wrong_way) {
        changed = true;
    }
    if (portal->_warning_facing_wrong_way) {
        WARN_PRINT(
            String(portal->get_name()) + " is possibly facing the wrong way."
        );
    }

    // Handled later.
    portal->_warning_autolink_failed = false;

    if (changed) {
        portal->update_gizmo();
    }
#endif
}

void RoomManager::finalize_portals(
    Spatial* p_roomlist,
    LocalVector<Room*, int32_t>& rooms,
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
                if (debug_logging) {
                    print_line(
                        "\t\tAUTOLINK OK from " + source_room->get_name()
                        + " to " + room->get_name()
                    );
                }
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
) const {
    if (r_planes.empty()) {
        return;
    }

    Vector<Vector3> pts = Geometry::compute_convex_mesh_points(
        &r_planes[0],
        r_planes.size(),
        0.001f
    );
    Error err = build_room_convex_hull(
        p_room,
        pts,
        r_md,
        default_simplify_info._plane_simplify
    );

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
            add_plane_if_unique(
                r_md.faces[n].plane,
                r_planes,
                p_room,
                default_simplify_info
            );
        }
    }
}

bool RoomManager::_convert_room_hull_final(
    Room* p_room,
    const LocalVector<Portal*>& p_portals
) const {
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

        if (add_plane_if_unique(
                plane,
                p_room->_planes,
                p_room,
                default_simplify_info
            )) {
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
    Error err = build_room_convex_hull(
        p_room,
        vertices_including_portals,
        md,
        default_simplify_info._plane_simplify
    );

    if (err != OK) {
        return false;
    }

    // add the planes from the new hull
    for (int n = 0; n < md.faces.size(); n++) {
        const Plane& p = md.faces[n].plane;
        add_plane_if_unique(p, p_room->_planes, p_room, default_simplify_info);
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
        if (debug_logging) {
            print_line(
                "\t\t\tcontained " + itos(num_planes_before_simplification)
                + " planes before simplification, "
                + itos(p_room->_planes.size()) + " planes after."
            );
        }
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

void RoomManager::finalize_rooms(
    LocalVector<Room*, int32_t>& rooms,
    const LocalVector<Portal*>& p_portals
) const {
    bool found_errors = false;

    for (int n = 0; n < rooms.size(); n++) {
        Room* room = rooms[n];

        // no need to do all these string operations if we are not debugging and
        // don't need logs
        if (debug_logging) {
            String room_short_name =
                remove_suffix(room->get_name(), "-room", true);
            print_line("ROOM\t" + room_short_name);

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
                        remove_suffix(linked_room->get_name(), "-room", true);
                    String in_or_out = (portal_links_out) ? "POUT" : "PIN ";

                    // display the name of the room linked to
                    print_line(
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
        process_visual_instances(room, room->_room_rid, debug_logging);

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

void RoomManager::place_remaining_statics(
    Spatial* p_node,
    LocalVector<Room*, int32_t>& rooms
) {
    if (p_node->is_queued_for_deletion()) {
        return;
    }

    // as soon as we hit a room, quit the recursion as the objects
    // will already have been added inside rooms
    if (Object::cast_to<Room>(p_node)) {
        return;
    }

    auto* visual_instance = cast_to<VisualInstance>(p_node);

    // we are only interested in VIs with static or dynamic mode
    if (visual_instance) {
        switch (visual_instance->get_portal_mode()) {
            default: {
            } break;
            case CullInstance::PORTAL_MODE_DYNAMIC:
            case CullInstance::PORTAL_MODE_STATIC: {
                _autoplace_object(visual_instance, rooms);
            } break;
        }
    }

    for (int n = 0; n < p_node->get_child_count(); n++) {
        auto* child = cast_to<Spatial>(p_node->get_child(n));
        if (child) {
            place_remaining_statics(child, rooms);
        }
    }
}

bool RoomManager::_autoplace_object(
    VisualInstance* p_vi,
    LocalVector<Room*, int32_t>& rooms
) const {
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
        process_visual_instance(p_vi, best_room->_room_rid, debug_logging);
        return true;
    }

    return false;
}

void RoomManager::_flip_portals_recursive(Spatial* p_node) {
    auto* portal = cast_to<Portal>(p_node);
    if (portal) {
        portal->flip();
    }

    for (int n = 0; n < p_node->get_child_count(); n++) {
        auto* child = cast_to<Spatial>(p_node->get_child(n));
        if (child) {
            _flip_portals_recursive(child);
        }
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

#ifdef TOOLS_ENABLED
void RoomManager::_generate_room_overlap_zones(
    LocalVector<Room*, int32_t>& rooms
) {
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
            Error err = build_quick_hull(overlap_pts, md);

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

void RoomManager::add_room_to_room_groups(
    Room* room,
    const LocalVector<RoomGroup*>& room_groups
) {
    for (int index = 0; index < room->_roomgroups.size(); index++) {
        const int room_group_id = room->_roomgroups[index];
        room_groups[room_group_id]->add_room(room);
    }
}

bool RoomManager::add_plane_if_unique(
    const Plane& plane,
    LocalVector<Plane, int32_t>& planes,
    const Room* room,
    const Room::SimplifyInfo& default_simplify_info
) {
    if (room->_use_default_simplify) {
        return default_simplify_info.add_plane_if_unique(planes, plane);
    }
    return room->_simplify_info.add_plane_if_unique(planes, plane);
}

void RoomManager::add_mesh_planes(
    Room* room,
    const Geometry::MeshData& mesh_data,
    const Room::SimplifyInfo& default_simplify_info
) {
    for (int index = 0; index < mesh_data.faces.size(); index++) {
        const Plane& plane = mesh_data.faces[index].plane;
        add_plane_if_unique(
            plane,
            room->_preliminary_planes,
            room,
            default_simplify_info
        );
    }
}

void RoomManager::add_portal_planes(
    Room* room,
    const LocalVector<Portal*>& portals,
    const Room::SimplifyInfo& default_simplify_info
) {
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
        add_plane_if_unique(
            plane,
            room->_preliminary_planes,
            room,
            default_simplify_info
        );
    }
}

Vector<Vector3> RoomManager::convert_manual_bound(MeshInstance* mesh_instance) {
    const Vector<Vector3> mesh_instance_points =
        get_mesh_instance_points(mesh_instance);
    if (mesh_instance_points.empty()) {
        return {};
    }
    mesh_instance->set_portal_mode(CullInstance::PORTAL_MODE_IGNORE);
    mesh_instance->hide();
    return mesh_instance_points;
}

bool RoomManager::convert_room_hull_preliminary(
    Room* room,
    const Vector<Vector3>& room_points,
    const LocalVector<Portal*>& portals,
    const Room::SimplifyInfo& default_simplify_info
) {
    Geometry::MeshData room_convex_mesh_data;
    const Error error = build_best_room_convex_hull(
        room,
        room_points,
        room_convex_mesh_data,
        default_simplify_info._plane_simplify
    );
    if (error != OK) {
        return false;
    }
    // Add existing portal planes first.
    add_portal_planes(room, portals, default_simplify_info);
    add_mesh_planes(room, room_convex_mesh_data, default_simplify_info);
    room->_bound_mesh_data = room_convex_mesh_data;
    room->_aabb.create_from_points(room_convex_mesh_data.vertices);
    return true;
}

bool RoomManager::get_room_points(
    Room* room,
    Vector<Vector3>& room_points,
    const LocalVector<Portal*>& portals,
    const Room::SimplifyInfo& default_simplify_info
) {
    bool manual_bound_found = false;
    for (int index = 0; index < room->get_child_count(); index++) {
        auto* child = cast_to<Spatial>(room->get_child(index));
        if (!child || child->is_queued_for_deletion()) {
            continue;
        }
        if (cast_to<Portal>(child)) {
            continue;
        }
        auto* mesh_instance = cast_to<MeshInstance>(child);
        if (mesh_instance && node_name_ends_with(child, "-bound")) {
            const Vector<Vector3> mesh_instance_points =
                convert_manual_bound(mesh_instance);
            if (mesh_instance_points.size() <= 3) {
                return false;
            }
            return convert_room_hull_preliminary(
                room,
                mesh_instance_points,
                portals,
                default_simplify_info
            );
        }
        add_visual_instances_points(child, room_points);
    }
    return manual_bound_found;
}

void RoomManager::update_room_points(
    const Room* room,
    Vector<Vector3>& room_points
) {
    const Transform global_transform = room->get_global_transform();
    room_points.clear();
    room_points.resize(room->_bound_pts.size());
    for (int n = 0; n < room_points.size(); n++) {
        room_points.set(n, global_transform.xform(room->_bound_pts[n]));
    }
}

void RoomManager::clean_up_rooms(LocalVector<Room*, int32_t>& rooms) {
    for (int index = 0; index < rooms.size(); index++) {
        Room* room = rooms[index];
        room->_portals.reset();
        room->_preliminary_planes.reset();
        // Data for convex hull drawing, is only used for gizmos in the editor.
        if (!Engine::get_singleton()->is_editor_hint()) {
            room->_bound_mesh_data = Geometry::MeshData();
        }
    }
}
