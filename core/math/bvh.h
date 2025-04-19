// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef BVH_H
#define BVH_H

#include "bvh_tree.h"
#include "core/os/mutex.h"

#define BVHTREE_CLASS Tree<T, 2, MAX_ITEMS, BoundingBox, Point>
#define BVH_LOCKED_FUNCTION                                                    \
    BVHLockedFunction(&_mutex, thread_safe&& _thread_safe);

namespace BVH {

// BVH Manager provides a wrapper around BVH tree, which contains most of the
// functionality for a dynamic BVH with templated leaf size.
// BVH Manager also adds facilities for pairing. Pairing is a collision pairing
// system, on top of the basic BVH.

// The rendering tree mask and types that are sent to the BVH are NOT layer
// masks. They are INSTANCE_TYPES (defined in visual_server.h), e.g. MESH,
// MULTIMESH, PARTICLES etc. Thus the lights do no cull by layer mask in the
// BVH.

// Layer masks are implemented in the renderers as a later step, and
// light_cull_mask appears to be implemented in GLES3 but not GLES2. Layer masks
// are not yet implemented for directional lights.

template <
    class T,
    int MAX_ITEMS     = 32,
    class BoundingBox = ::AABB,
    class Point       = Vector3>
class Manager {
public:
    // Use uint32_t instead of Handle to maintain compatibility with octree.
    typedef void* (*PairCallback)(void*, uint32_t, T*, int, uint32_t, T*, int);
    typedef void (*UnpairCallback)(void*, uint32_t, T*, int, uint32_t, T*, int, void*);
    typedef void* (*CheckPairCallback)(void*, uint32_t, T*, int, uint32_t, T*, int, void*);

    // Toggles thread safety if thread_safe = true.
    void params_set_thread_safe(bool p_enable) {
        _thread_safe = p_enable;
    }

    // Used for for fine tuning.
    void params_set_node_expansion(real_t p_value) {
        BVH_LOCKED_FUNCTION
        if (p_value >= 0.0) {
            tree._node_expansion      = p_value;
            tree._auto_node_expansion = false;
        } else {
            tree._auto_node_expansion = true;
        }
    }

    void params_set_pairing_expansion(real_t p_value) {
        BVH_LOCKED_FUNCTION
        tree.params_set_pairing_expansion(p_value);
    }

    void set_pair_callback(PairCallback p_callback, void* p_userdata) {
        BVH_LOCKED_FUNCTION
        pair_callback          = p_callback;
        pair_callback_userdata = p_userdata;
    }

    void set_unpair_callback(UnpairCallback p_callback, void* p_userdata) {
        BVH_LOCKED_FUNCTION
        unpair_callback          = p_callback;
        unpair_callback_userdata = p_userdata;
    }

    void set_check_pair_callback(
        CheckPairCallback p_callback,
        void* p_userdata
    ) {
        BVH_LOCKED_FUNCTION
        check_pair_callback          = p_callback;
        check_pair_callback_userdata = p_userdata;
    }

    Handle create(
        T* p_userdata,
        bool p_active,
        const BoundingBox& p_aabb = BoundingBox(),
        int p_subindex            = 0,
        bool p_pairable           = false,
        uint32_t p_pairable_type  = 0,
        uint32_t p_pairable_mask  = 1
    ) {
        BVH_LOCKED_FUNCTION

        if (use_pairs) {
            // Uncomment if there are bugs.
            //_check_for_collisions();
        }

#ifdef TOOLS_ENABLED
        if (!use_pairs) {
            if (p_pairable) {
                WARN_PRINT_ONCE(
                    "creating pairable item in BVH with use_pairs set to false"
                );
            }
        }
#endif

        Handle h = tree.item_add(
            p_userdata,
            p_active,
            p_aabb,
            p_subindex,
            p_pairable,
            p_pairable_type,
            p_pairable_mask
        );

        if (use_pairs) {
            // For safety, initialize the expanded AABB.
            BoundingBox& expanded_aabb = tree._pairs[h.id()].expanded_aabb;
            expanded_aabb              = p_aabb;
            expanded_aabb.grow_by(tree._pairing_expansion);

            // Force a collision check no matter the AABB.
            if (p_active) {
                _add_changed_item(h, p_aabb, false);
                _check_for_collisions(true);
            }
        }

        return h;
    }

    // Wrapper versions that use uint32_t instead of Handle.
    void move(uint32_t p_handle, const BoundingBox& p_aabb) {
        Handle h;
        h.set(p_handle);
        move(h, p_aabb);
    }

    void recheck_pairs(uint32_t p_handle) {
        Handle h;
        h.set(p_handle);
        recheck_pairs(h);
    }

    void erase(uint32_t p_handle) {
        Handle h;
        h.set(p_handle);
        erase(h);
    }

    void force_collision_check(uint32_t p_handle) {
        Handle h;
        h.set(p_handle);
        force_collision_check(h);
    }

    bool activate(
        uint32_t p_handle,
        const BoundingBox& p_aabb,
        bool p_delay_collision_check = false
    ) {
        Handle h;
        h.set(p_handle);
        return activate(h, p_aabb, p_delay_collision_check);
    }

    bool deactivate(uint32_t p_handle) {
        Handle h;
        h.set(p_handle);
        return deactivate(h);
    }

    void set_pairable(
        uint32_t p_handle,
        bool p_pairable,
        uint32_t p_pairable_type,
        uint32_t p_pairable_mask,
        bool p_force_collision_check = true
    ) {
        Handle h;
        h.set(p_handle);
        set_pairable(
            h,
            p_pairable,
            p_pairable_type,
            p_pairable_mask,
            p_force_collision_check
        );
    }

    bool is_pairable(uint32_t p_handle) const {
        Handle h;
        h.set(p_handle);
        return item_is_pairable(h);
    }

    int get_subindex(uint32_t p_handle) const {
        Handle h;
        h.set(p_handle);
        return item_get_subindex(h);
    }

    T* get(uint32_t p_handle) const {
        Handle h;
        h.set(p_handle);
        return item_get_userdata(h);
    }

    ////////////////////////////////////////////////////

    void move(Handle p_handle, const BoundingBox& p_aabb) {
        BVH_LOCKED_FUNCTION
        if (tree.item_move(p_handle, p_aabb)) {
            if (use_pairs) {
                _add_changed_item(p_handle, p_aabb);
            }
        }
    }

    void recheck_pairs(Handle p_handle) {
        BVH_LOCKED_FUNCTION
        if (use_pairs) {
            _recheck_pairs(p_handle);
        }
    }

    void erase(Handle p_handle) {
        BVH_LOCKED_FUNCTION
        // Call unpair and remove all references before deleting from the tree.
        if (use_pairs) {
            _remove_changed_item(p_handle);
        }

        tree.item_remove(p_handle);

        _check_for_collisions(true);
    }

    // Use in conjunction with activate if collision checks were deferred,
    // and set pairable was never called.
    // Deferred collision checks are a workaround for visual server.
    void force_collision_check(Handle p_handle) {
        BVH_LOCKED_FUNCTION
        if (use_pairs) {
            BoundingBox aabb;
            item_get_AABB(p_handle, aabb);
            _add_changed_item(p_handle, aabb, false);
            _check_for_collisions(true);
        }
    }

    // Should be read as set_visible for render trees.
    bool activate(
        Handle p_handle,
        const BoundingBox& p_aabb,
        bool p_delay_collision_check = false
    ) {
        BVH_LOCKED_FUNCTION
        // Sending the AABB here prevents the need for the BVH to maintain a
        // redundant copy of the aabb.
        if (tree.item_activate(p_handle, p_aabb)) {
            if (use_pairs) {
                // In the special case of the render tree, when setting
                // visibility, we are using the combination of activate then
                // set_pairable. This causes 2 sets of collision checks.
                // For efficiency we allow deferring to have a single collision
                // check at the set_pairable call.
                // May cause bugs if set_pairable is not called.
                if (!p_delay_collision_check) {
                    _add_changed_item(p_handle, p_aabb, false);
                    _check_for_collisions(true);
                }
            }
            return true;
        }
        return false;
    }

    bool deactivate(Handle p_handle) {
        BVH_LOCKED_FUNCTION
        if (tree.item_deactivate(p_handle)) {
            // Call unpair and remove all references before deleting.
            if (use_pairs) {
                _remove_changed_item(p_handle);
                _check_for_collisions(true);
            }
            return true;
        }
        return false;
    }

    bool get_active(Handle p_handle) {
        BVH_LOCKED_FUNCTION
        return tree.item_get_active(p_handle);
    }

    // Called once per frame.
    void update() {
        BVH_LOCKED_FUNCTION
        tree.update();
        _check_for_collisions();
#ifdef BVH_INTEGRITY_CHECKS
        tree._integrity_check_all();
#endif
    }

    // Can be called more frequently than once per frame if necessary.
    void update_collisions() {
        BVH_LOCKED_FUNCTION
        _check_for_collisions();
    }

    void set_pairable(
        const Handle& p_handle,
        bool p_pairable,
        uint32_t p_pairable_type,
        uint32_t p_pairable_mask,
        bool p_force_collision_check = true
    ) {
        BVH_LOCKED_FUNCTION
        // Returns true if the pairing state has changed.
        bool state_changed = tree.item_set_pairable(
            p_handle,
            p_pairable,
            p_pairable_type,
            p_pairable_mask
        );

        if (use_pairs) {
            // Uncomment if there are bugs.
            //_check_for_collisions();

            if ((p_force_collision_check || state_changed)
                && tree.item_get_active(p_handle)) {
                // We force a collision check when the pairable state changes,
                // because newly pairable items may be in collision, and
                // unpairable items might move out of collision.
                BoundingBox aabb;
                item_get_AABB(p_handle, aabb);

                // Passing false disables the optimization that prevents
                // collision checks if the AABB hasn't changed.
                _add_changed_item(p_handle, aabb, false);
                _check_for_collisions(true);
            }
        }
    }

    int cull_aabb(
        const BoundingBox& p_aabb,
        T** p_result_array,
        int p_result_max,
        int* p_subindex_array = nullptr,
        uint32_t p_mask       = 0xFFFFFFFF
    ) {
        BVH_LOCKED_FUNCTION
        typename BVHTREE_CLASS::CullParams params;

        params.result_count_overall = 0;
        params.result_max           = p_result_max;
        params.result_array         = p_result_array;
        params.subindex_array       = p_subindex_array;
        params.mask                 = p_mask;
        params.pairable_type        = 0;
        params.test_pairable_only   = false;
        params.bvh_aabb.from(p_aabb);

        tree.cull_aabb(params);

        return params.result_count_overall;
    }

    int cull_segment(
        const Point& p_from,
        const Point& p_to,
        T** p_result_array,
        int p_result_max,
        int* p_subindex_array = nullptr,
        uint32_t p_mask       = 0xFFFFFFFF
    ) {
        BVH_LOCKED_FUNCTION
        typename BVHTREE_CLASS::CullParams params;

        params.result_count_overall = 0;
        params.result_max           = p_result_max;
        params.result_array         = p_result_array;
        params.subindex_array       = p_subindex_array;
        params.mask                 = p_mask;
        params.pairable_type        = 0;

        params.segment.from = p_from;
        params.segment.to   = p_to;

        tree.cull_segment(params);

        return params.result_count_overall;
    }

    int cull_point(
        const Point& p_point,
        T** p_result_array,
        int p_result_max,
        int* p_subindex_array = nullptr,
        uint32_t p_mask       = 0xFFFFFFFF
    ) {
        BVH_LOCKED_FUNCTION
        typename BVHTREE_CLASS::CullParams params;

        params.result_count_overall = 0;
        params.result_max           = p_result_max;
        params.result_array         = p_result_array;
        params.subindex_array       = p_subindex_array;
        params.mask                 = p_mask;
        params.pairable_type        = 0;

        params.point = p_point;

        tree.cull_point(params);
        return params.result_count_overall;
    }

    int cull_convex(
        const Vector<Plane>& p_convex,
        T** p_result_array,
        int p_result_max,
        uint32_t p_mask = 0xFFFFFFFF
    ) {
        BVH_LOCKED_FUNCTION
        if (!p_convex.size()) {
            return 0;
        }

        Vector<Vector3> convex_points =
            Geometry::compute_convex_mesh_points(&p_convex[0], p_convex.size());
        if (convex_points.size() == 0) {
            return 0;
        }

        typename BVHTREE_CLASS::CullParams params;
        params.result_count_overall = 0;
        params.result_max           = p_result_max;
        params.result_array         = p_result_array;
        params.subindex_array       = nullptr;
        params.mask                 = p_mask;
        params.pairable_type        = 0;

        params.hull.planes     = &p_convex[0];
        params.hull.num_planes = p_convex.size();
        params.hull.points     = &convex_points[0];
        params.hull.num_points = convex_points.size();

        tree.cull_convex(params);

        return params.result_count_overall;
    }

private:
    void _check_for_collisions(bool p_full_check = false) {
        if (!changed_items.size()) {
            // noop
            return;
        }

        BoundingBox bb;

        typename BVHTREE_CLASS::CullParams params;

        params.result_count_overall = 0;
        params.result_max           = INT_MAX;
        params.result_array         = nullptr;
        params.subindex_array       = nullptr;
        params.mask                 = 0xFFFFFFFF;
        params.pairable_type        = 0;

        for (unsigned int n = 0; n < changed_items.size(); n++) {
            const Handle& h = changed_items[n];

            // Use the expanded aabb for pairing.
            const BoundingBox& expanded_aabb =
                tree._pairs[h.id()].expanded_aabb;
            BVHAABB_CLASS bvh_aabb;
            bvh_aabb.from(expanded_aabb);

            // Send callbacks to pairs that are no longer paired.
            _find_leavers(h, bvh_aabb, p_full_check);

            uint32_t changed_item_ref_id = h.id();

            // Use this item for mask and test for non-pairable tree.
            tree.item_fill_cullparams(h, params);

            params.bvh_aabb = bvh_aabb;

            // TODO: Is this needed?
            params.result_count_overall = 0;

            tree.cull_aabb(params, false);
            for (unsigned int i = 0; i < tree._cull_hits.size(); i++) {
                uint32_t ref_id = tree._cull_hits[i];

                if (ref_id == changed_item_ref_id) {
                    continue;
                }

                Handle h_collidee;
                h_collidee.set_id(ref_id);
                _collide(h, h_collidee);
            }
        }
        _reset();
    }

public:
    void item_get_AABB(Handle p_handle, BoundingBox& r_aabb) {
        BVHAABB_CLASS bvh_aabb;
        tree.item_get_bvh_aabb(p_handle, bvh_aabb);
        bvh_aabb.to(r_aabb);
    }

private:
    bool item_is_pairable(Handle p_handle) const {
        return _get_extra(p_handle).pairable;
    }

    T* item_get_userdata(Handle p_handle) const {
        return _get_extra(p_handle).userdata;
    }

    int item_get_subindex(Handle p_handle) const {
        return _get_extra(p_handle).subindex;
    }

    void _unpair(Handle p_from, Handle p_to) {
        tree._handle_sort(p_from, p_to);

        typename BVHTREE_CLASS::ItemExtra& exa = tree._extra[p_from.id()];
        typename BVHTREE_CLASS::ItemExtra& exb = tree._extra[p_to.id()];

        // If the userdata is the same, no collisions should occur.
        if ((exa.userdata == exb.userdata) && exa.userdata) {
            return;
        }

        typename BVHTREE_CLASS::ItemPairs& pairs_from =
            tree._pairs[p_from.id()];
        typename BVHTREE_CLASS::ItemPairs& pairs_to = tree._pairs[p_to.id()];

        void* ud_from = pairs_from.remove_pair_to(p_to);
        pairs_to.remove_pair_to(p_from);

#ifdef BVH_VERBOSE_PAIRING
        print_line("_unpair " + itos(p_from.id()) + " from " + itos(p_to.id()));
#endif

        // callback
        if (unpair_callback) {
            unpair_callback(
                pair_callback_userdata,
                p_from,
                exa.userdata,
                exa.subindex,
                p_to,
                exb.userdata,
                exb.subindex,
                ud_from
            );
        }
    }

    void* _recheck_pair(Handle p_from, Handle p_to, void* p_pair_data) {
        tree._handle_sort(p_from, p_to);

        typename BVHTREE_CLASS::ItemExtra& exa = tree._extra[p_from.id()];
        typename BVHTREE_CLASS::ItemExtra& exb = tree._extra[p_to.id()];

        // If the userdata is the same, no collisions should occur.
        if ((exa.userdata == exb.userdata) && exa.userdata) {
            return p_pair_data;
        }

        if (check_pair_callback) {
            return check_pair_callback(
                check_pair_callback_userdata,
                p_from,
                exa.userdata,
                exa.subindex,
                p_to,
                exb.userdata,
                exb.subindex,
                p_pair_data
            );
        }

        return p_pair_data;
    }

    // Returns true if unpaired.
    bool _find_leavers_process_pair(
        typename BVHTREE_CLASS::ItemPairs& p_pairs_from,
        const BVHAABB_CLASS& p_abb_from,
        Handle p_from,
        Handle p_to,
        bool p_full_check
    ) {
        BVHAABB_CLASS bvh_aabb_to;
        tree.item_get_bvh_aabb(p_to, bvh_aabb_to);

        // Test for overlap.
        if (p_abb_from.intersects(bvh_aabb_to)) {
            if (!p_full_check) {
                return false;
            }

            const typename BVHTREE_CLASS::ItemExtra& exa = _get_extra(p_from);
            const typename BVHTREE_CLASS::ItemExtra& exb = _get_extra(p_to);

            // To pair, one of the two must be pairable.
            if (exa.pairable || exb.pairable) {
                // To pair, the masks must be compatible.
                if (tree._cull_pairing_mask_test_hit(
                        exa.pairable_mask,
                        exa.pairable_type,
                        exb.pairable_mask,
                        exb.pairable_type
                    )) {
                    return false;
                }
            }
        }

        _unpair(p_from, p_to);
        return true;
    }

    // Find all paired aabbs that are no longer paired, and send callbacks.
    void _find_leavers(
        Handle p_handle,
        const BVHAABB_CLASS& expanded_abb_from,
        bool p_full_check
    ) {
        typename BVHTREE_CLASS::ItemPairs& p_from = tree._pairs[p_handle.id()];

        BVHAABB_CLASS bvh_aabb_from = expanded_abb_from;

        // Remove every partner from pairing list.
        for (unsigned int n = 0; n < p_from.extended_pairs.size(); n++) {
            Handle h_to = p_from.extended_pairs[n].handle;
            if (_find_leavers_process_pair(
                    p_from,
                    bvh_aabb_from,
                    p_handle,
                    h_to,
                    p_full_check
                )) {
                // TODO: Check if this is necessaryj.
                n--;
            }
        }
    }

    void _collide(Handle p_ha, Handle p_hb) {
        // We only need to do this oneway: lower ID then higher ID.
        tree._handle_sort(p_ha, p_hb);

        const typename BVHTREE_CLASS::ItemExtra& exa = _get_extra(p_ha);
        const typename BVHTREE_CLASS::ItemExtra& exb = _get_extra(p_hb);

        // If the userdata is the same, no collisions should occur.
        if ((exa.userdata == exb.userdata) && exa.userdata) {
            return;
        }

        typename BVHTREE_CLASS::ItemPairs& p_from = tree._pairs[p_ha.id()];
        typename BVHTREE_CLASS::ItemPairs& p_to   = tree._pairs[p_hb.id()];

        // Only check the one with lower number of pairs for greater speed.
        if (p_from.num_pairs <= p_to.num_pairs) {
            if (p_from.contains_pair_to(p_hb)) {
                return;
            }
        } else {
            if (p_to.contains_pair_to(p_ha)) {
                return;
            }
        }

        void* callback_userdata = nullptr;

#ifdef BVH_VERBOSE_PAIRING
        print_line("_pair " + itos(p_ha.id()) + " to " + itos(p_hb.id()));
#endif

        if (pair_callback) {
            callback_userdata = pair_callback(
                pair_callback_userdata,
                p_ha,
                exa.userdata,
                exa.subindex,
                p_hb,
                exb.userdata,
                exb.subindex
            );
        }

        // We actually only need to store the userdata on the lower handle.
        p_from.add_pair_to(p_hb, callback_userdata);
        p_to.add_pair_to(p_ha, callback_userdata);
    }

    // If we remove an item, remove the pairs.
    void _remove_pairs_containing(Handle p_handle) {
        typename BVHTREE_CLASS::ItemPairs& p_from = tree._pairs[p_handle.id()];

        // Remove from pairing list for every partner.
        while (p_from.extended_pairs.size()) {
            Handle h_to = p_from.extended_pairs[0].handle;
            _unpair(p_handle, h_to);
        }
    }

    // Send pair callbacks again for all existing pairs for the given handle.
    void _recheck_pairs(Handle p_handle) {
        typename BVHTREE_CLASS::ItemPairs& from = tree._pairs[p_handle.id()];

        // Check pair for every partner.
        for (unsigned int n = 0; n < from.extended_pairs.size(); n++) {
            typename BVHTREE_CLASS::ItemPairs::Link& pair =
                from.extended_pairs[n];
            Handle h_to         = pair.handle;
            void* new_pair_data = _recheck_pair(p_handle, h_to, pair.userdata);

            if (new_pair_data != pair.userdata) {
                pair.userdata = new_pair_data;

                // Update pair data for the second item.
                typename BVHTREE_CLASS::ItemPairs& to = tree._pairs[h_to.id()];
                for (unsigned int to_index = 0;
                     to_index < to.extended_pairs.size();
                     to_index++) {
                    typename BVHTREE_CLASS::ItemPairs::Link& to_pair =
                        to.extended_pairs[to_index];
                    if (to_pair.handle == p_handle) {
                        to_pair.userdata = new_pair_data;
                        break;
                    }
                }
            }
        }
    }

private:
    const typename BVHTREE_CLASS::ItemExtra& _get_extra(Handle p_handle) const {
        return tree._extra[p_handle.id()];
    }

    const typename BVHTREE_CLASS::ItemRef& _get_ref(Handle p_handle) const {
        return tree._refs[p_handle.id()];
    }

    void _reset() {
        changed_items.clear();
        _tick++;
    }

    void _add_changed_item(
        Handle p_handle,
        const BoundingBox& aabb,
        bool p_check_aabb = true
    ) {
        // Pairable items can pair with non-pairable items.
        // So, all types must be added to the list.

#ifdef BVH_EXPAND_LEAF_AABBS
        // Redundancy check already done, if using expanded AABB in the leaf.
        BoundingBox& expanded_aabb = tree._pairs[p_handle.id()].expanded_aabb;
        item_get_AABB(p_handle, expanded_aabb);
#else
        // AABB check with expanded AABB.
        // Greatly decreases processing using less accurate pairing checks.
        // Note: This pairing AABB is separate from the AABB in the actual tree.
        BoundingBox& expanded_aabb = tree._pairs[p_handle.id()].expanded_aabb;

        // If p_check_aabb is true, we check collisions if the AABB has changed.
        // Used when calling set_pairable has been called, but the position has
        // not changed.
        if (p_check_aabb
            && tree.expanded_aabb_encloses_not_shrink(expanded_aabb, aabb)) {
            return;
        }

        // Always update the new expanded AABB.
        expanded_aabb = aabb;
        expanded_aabb.grow_by(tree._pairing_expansion);
#endif

        // Ensure changed items only appear once on the updated list.
        uint32_t& last_updated_tick =
            tree._extra[p_handle.id()].last_updated_tick;
        if (last_updated_tick == _tick) {
            return;
        }

        last_updated_tick = _tick;
        changed_items.push_back(p_handle);
    }

    void _remove_changed_item(Handle p_handle) {
        // Care must be taken for items that are deleted.
        // The ref ID could be reused on the same tick for new items.
        // This is probably rare but should be taken into consideration.

        _remove_pairs_containing(p_handle);

        // Remove from changed items.
        // TODO: Make more efficient yet.
        for (int n = 0; n < (int)changed_items.size(); n++) {
            if (changed_items[n] == p_handle) {
                changed_items.remove_unordered(n);

                // TODO: Check if this is needed.
                n--;
            }
        }

        // TODO: Check if this is needed.
        tree._extra[p_handle.id()].last_updated_tick = 0;
    }

    PairCallback pair_callback;
    UnpairCallback unpair_callback;
    CheckPairCallback check_pair_callback;
    void* pair_callback_userdata;
    void* unpair_callback_userdata;
    void* check_pair_callback_userdata;

    BVHTREE_CLASS tree;

    // Maintain a list of all items moved to test for collision pairing.
    LocalVector<Handle, uint32_t, true> changed_items;
    uint32_t _tick;

    class BVHLockedFunction {
    public:
        BVHLockedFunction(Mutex* p_mutex, bool p_thread_safe) {
            // Should be compiled out if not set in template.
            if (p_thread_safe) {
                _mutex = p_mutex;

                if (_mutex->try_lock() != OK) {
                    WARN_PRINT("Info : multithread BVH access detected (benign)"
                    );
                    _mutex->lock();
                }

            } else {
                _mutex = nullptr;
            }
        }

        ~BVHLockedFunction() {
            // Should be compiled out if not set in template.
            if (_mutex) {
                _mutex->unlock();
            }
        }

    private:
        Mutex* _mutex;
    };

    Mutex _mutex;

    // Toggle for turning thread safety on and off in project settings.
    bool _thread_safe;
    const bool use_pairs;
    const bool thread_safe;

public:
    Manager(bool use_pairs = true, bool thread_safe = true) :
        use_pairs(use_pairs),
        thread_safe(thread_safe) {
        // Start from 1. so items with 0 indicate never updated.
        _tick                    = 1;
        pair_callback            = nullptr;
        unpair_callback          = nullptr;
        pair_callback_userdata   = nullptr;
        unpair_callback_userdata = nullptr;
        _thread_safe             = thread_safe;
    }
};

#undef BVHTREE_CLASS

} // namespace BVH

#endif // BVH_H
