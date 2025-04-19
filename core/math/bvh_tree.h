// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef BVH_TREE_H
#define BVH_TREE_H

#include "core/local_vector.h"
#include "core/math/aabb.h"
#include "core/math/bvh_aabb.h"
#include "core/math/geometry.h"
#include "core/math/vector3.h"
#include "core/pooled_list.h"
#include "core/print_string.h"

#include <limits.h>

#define BVHAABB_CLASS AABB<BoundingBox, Point>

// TODO: Check if this is better.
#define BVH_EXPAND_LEAF_AABBS

// For debugging only.
#if defined(TOOLS_ENABLED) && defined(DEBUG_ENABLED)
// #define BVH_VERBOSE
// #define BVH_VERBOSE_TREE
// #define BVH_VERBOSE_PAIRING
// #define BVH_VERBOSE_MOVES

// #define BVH_VERBOSE_FRAME
// #define BVH_CHECKS
// #define BVH_INTEGRITY_CHECKS
#endif

// Debug only assert.
#ifdef BVH_CHECKS
#define BVH_ASSERT(a) CRASH_COND((a) == false)
#else
#define BVH_ASSERT(a)
#endif

#ifdef BVH_VERBOSE
#define VERBOSE_PRINT print_line
#else
#define VERBOSE_PRINT(a)
#endif

namespace BVH {

// These could possibly also be the same constant,
// although this may be useful for debugging.
// Could use zero for invalid and +1 based indices.
static const uint32_t INVALID  = (0xffffffff);
static const uint32_t INACTIVE = (0xfffffffe);

// Note: Zero is a valid reference for the BVH.
// May need a plus one based ID for clients that expect 0 to be invalid.
struct Handle {
    // Conversion operator.
    operator uint32_t() const {
        return _data;
    }

    void set(uint32_t p_value) {
        _data = p_value;
    }

    uint32_t _data;

    void set_invalid() {
        _data = INVALID;
    }

    bool is_invalid() const {
        return _data == INVALID;
    }

    uint32_t id() const {
        return _data;
    }

    void set_id(uint32_t p_id) {
        _data = p_id;
    }

    bool operator==(const Handle& p_h) const {
        return _data == p_h._data;
    }

    bool operator!=(const Handle& p_h) const {
        return (*this == p_h) == false;
    }
};

// Helper class to make iterative versions of recursive functions.
template <class T>
class IterativeInfo {
public:
    enum {
        ALLOCA_STACK_SIZE = 128
    };

    int32_t depth     = 1;
    int32_t threshold = ALLOCA_STACK_SIZE - 2;
    T* stack;
    // Only used in rare occasions when you run out of alloca memory,
    // because the tree is too unbalanced.
    LocalVector<T> aux_stack;

    int32_t get_alloca_stacksize() const {
        return ALLOCA_STACK_SIZE * sizeof(T);
    }

    T* get_first() const {
        return &stack[0];
    }

    // Pop the last member of the stack, or return false.
    bool pop(T& r_value) {
        if (!depth) {
            return false;
        }

        depth--;
        r_value = stack[depth];
        return true;
    }

    // Request an addition to stack.
    T* request() {
        if (depth > threshold) {
            if (aux_stack.empty()) {
                aux_stack.resize(ALLOCA_STACK_SIZE * 2);
                memcpy(aux_stack.ptr(), stack, get_alloca_stacksize());
            } else {
                aux_stack.resize(aux_stack.size() * 2);
            }
            stack     = aux_stack.ptr();
            threshold = aux_stack.size() - 2;
        }
        return &stack[depth++];
    }
};

// BVH Tree is an implementation of a dynamic BVH with templated leaf size.
// This differs from most dynamic BVH in that it can handle more than 1 object
// in leaf nodes. This can make it far more efficient in certain circumstances.
// It also means that the splitting logic etc have to be completely different
// to a simpler tree.
// Note: MAX_CHILDREN should be fixed at 2 for now.

template <
    class T,
    int MAX_CHILDREN,
    int MAX_ITEMS,
    bool use_pairs    = false,
    class BoundingBox = ::AABB,
    class Point       = Vector3>
class Tree {
public:
    // TODO: Check if this should be attached to another node structure.
    struct ItemPairs {
        struct Link {
            void set(Handle h, void* ud) {
                handle   = h;
                userdata = ud;
            }

            Handle handle;
            void* userdata;
        };

        void clear() {
            num_pairs = 0;
            extended_pairs.reset();
            expanded_aabb = BoundingBox();
        }

        BoundingBox expanded_aabb;

        // TODO: Could use Vector size.
        int32_t num_pairs;
        LocalVector<Link> extended_pairs;

        void add_pair_to(Handle h, void* p_userdata) {
            Link temp;
            temp.set(h, p_userdata);

            extended_pairs.push_back(temp);
            num_pairs++;
        }

        uint32_t find_pair_to(Handle h) const {
            for (int n = 0; n < num_pairs; n++) {
                if (extended_pairs[n].handle == h) {
                    return n;
                }
            }
            return -1;
        }

        bool contains_pair_to(Handle h) const {
            return find_pair_to(h) != INVALID;
        }

        // Return success
        void* remove_pair_to(Handle h) {
            void* userdata = nullptr;

            for (int n = 0; n < num_pairs; n++) {
                if (extended_pairs[n].handle == h) {
                    userdata = extended_pairs[n].userdata;
                    extended_pairs.remove_unordered(n);
                    num_pairs--;
                    break;
                }
            }

            return userdata;
        }

        // Experiment: Scale the pairing expansion by the number of pairs.
        // When the number of pairs is high, the density is high and a lower
        // collision margin is better.
        // When there are few local pairs, a larger margin is more optimal.
        real_t scale_expansion_margin(real_t p_margin) const {
            real_t x = real_t(num_pairs) * (1.0 / 9.0);
            x        = MIN(x, 1.0);
            x        = 1.0 - x;
            return p_margin * x;
        }
    };

public:
    struct ItemRef {
        uint32_t tnode_id;
        uint32_t item_id;

        bool is_active() const {
            return tnode_id != INACTIVE;
        }

        void set_inactive() {
            tnode_id = INACTIVE;
            item_id  = INACTIVE;
        }
    };

    // Extra info is less used. So, for better caching, it is kept separate.
    struct ItemExtra {
        uint32_t last_updated_tick;
        uint32_t pairable;
        uint32_t pairable_mask;
        uint32_t pairable_type;

        int32_t subindex;

        // The active reference allows iterating over many frames.
        uint32_t active_ref_id;

        T* userdata;
    };

    struct TLeaf {
        uint16_t num_items;

    private:
        uint16_t dirty;
        // separate data orientated lists for faster SIMD traversal
        uint32_t item_ref_ids[MAX_ITEMS];
        BVHAABB_CLASS aabbs[MAX_ITEMS];

    public:
        BVHAABB_CLASS& get_aabb(uint32_t p_id) {
            return aabbs[p_id];
        }

        const BVHAABB_CLASS& get_aabb(uint32_t p_id) const {
            return aabbs[p_id];
        }

        uint32_t& get_item_ref_id(uint32_t p_id) {
            return item_ref_ids[p_id];
        }

        const uint32_t& get_item_ref_id(uint32_t p_id) const {
            return item_ref_ids[p_id];
        }

        bool is_dirty() const {
            return dirty;
        }

        void set_dirty(bool p) {
            dirty = p;
        }

        void clear() {
            num_items = 0;
            set_dirty(true);
        }

        bool is_full() const {
            return num_items >= MAX_ITEMS;
        }

        void remove_item_unordered(uint32_t p_id) {
            BVH_ASSERT(p_id < num_items);
            num_items--;
            aabbs[p_id]        = aabbs[num_items];
            item_ref_ids[p_id] = item_ref_ids[num_items];
        }

        uint32_t request_item() {
            CRASH_COND_MSG(num_items >= MAX_ITEMS, "BVH leaf is full");
            uint32_t id = num_items;
            num_items++;
            return id;
        }
    };

    struct TNode {
        BVHAABB_CLASS aabb;

        // Either number of children is positive or leaf id if negative.
        // Leaf id = 0 is disallowed.
        union {
            int32_t num_children;
            int32_t neg_leaf_id;
        };

        // Set to -1 if there is no parent.
        uint32_t parent_id;
        uint16_t children[MAX_CHILDREN];

        // Height in the tree, where leaves are 0, and all above are +1.
        int32_t height;

        bool is_leaf() const {
            return num_children < 0;
        }

        void set_leaf_id(int id) {
            neg_leaf_id = -id;
        }

        int get_leaf_id() const {
            return -neg_leaf_id;
        }

        void clear() {
            num_children = 0;
            parent_id    = INVALID;
            // Set to -1 for testing.
            height       = 0;

            // For safety, set to improbable value.
            aabb.set_to_max_opposite_extents();
        }

        bool is_full_of_children() const {
            return num_children >= MAX_CHILDREN;
        }

        void remove_child_internal(uint32_t child_num) {
            children[child_num] = children[num_children - 1];
            num_children--;
        }

        int find_child(uint32_t p_child_node_id) {
            BVH_ASSERT(!is_leaf());

            for (int n = 0; n < num_children; n++) {
                if (children[n] == p_child_node_id) {
                    return n;
                }
            }

            // Not found.
            return -1;
        }
    };

    // Instead of using a linked list, we use item references for quick lookups.
    PooledList<ItemRef, true> _refs;
    PooledList<ItemExtra, true> _extra;
    PooledList<ItemPairs> _pairs;

    PooledList<TNode, true> _nodes;
    PooledList<TLeaf, true> _leaves;

    // We can maintain an un-ordered list of which references are active.
    // Enables a slow incremental optimize of the tree over each frame.
    // Work best if dynamic objects and static objects are in different trees.
    LocalVector<uint32_t, uint32_t, true> _active_refs;
    uint32_t _current_active_ref = 0;

    // We use an intermediate list of reference IDs for hits.
    // These can be used for pairing collision detection.
    LocalVector<uint32_t, uint32_t, true> _cull_hits;

    // Multiple root nodes allow us to store more than 1 tree.
    // This enables sharing the same common lists for efficiency.
    enum {
        NUM_TREES = 2,
    };

    // Tree 0 - Non pairable
    // Tree 1 - Pairable
    // In physics we only need check non pairable against the pairable tree.
    uint32_t _root_node_id[NUM_TREES];

    // These values may need tweaking according to the project the bound of the
    // world, and the average velocities of the objects.
    //
    // Node expansion is important in the rendering tree.
    // Larger values result in less re-insertion as items move,
    // but over estimates the bounding box of nodes.
    // Use auto mode, to allow the expansion to depend on the root node size.
    real_t _node_expansion    = 0.5;
    bool _auto_node_expansion = true;

    // Pairing expansion is important for physics pairing.
    // Larger values result in more 'sticky' pairing, and
    // it iss less likely to exhibit tunneling.
    // Use auto mode, to allow the expansion to depend on the root node size.
    real_t _pairing_expansion = 0.1;

#ifdef BVH_ALLOW_AUTO_EXPANSION
    bool _auto_pairing_expansion = true;
#endif

    // When using an expanded bound, we must detect the condition where a new
    // AABB is significantly smaller than the expanded bound.
    // This indicates a special case where we should override the optimization
    // and create a new expanded bound.
    // This threshold is derived from the _pairing_expansion, and
    // should be recalculated if _pairing_expansion is changed.
    real_t _aabb_shrinkage_threshold = 0.0;

public:
    Tree() {
        for (int n = 0; n < NUM_TREES; n++) {
            _root_node_id[n] = INVALID;
        }

        // Leaf ids are stored as negative numbers in the node.
        uint32_t dummy_leaf_id;
        _leaves.request(dummy_leaf_id);

        // In many cases you may want to change this default in the client code,
        // or expose this value to the user.
        // This default may make sense for a typically scaled 3d game, but maybe
        // not for 2d on a pixel scale.
        params_set_pairing_expansion(0.1);
    }

private:
    bool node_add_child(uint32_t p_node_id, uint32_t p_child_node_id) {
        TNode& tnode = _nodes[p_node_id];
        if (tnode.is_full_of_children()) {
            return false;
        }

        tnode.children[tnode.num_children]  = p_child_node_id;
        tnode.num_children                 += 1;

        // Back link from the child to the parent.
        TNode& tnode_child    = _nodes[p_child_node_id];
        tnode_child.parent_id = p_node_id;

        return true;
    }

    void node_replace_child(
        uint32_t p_parent_id,
        uint32_t p_old_child_id,
        uint32_t p_new_child_id
    ) {
        TNode& parent = _nodes[p_parent_id];
        BVH_ASSERT(!parent.is_leaf());

        int child_num = parent.find_child(p_old_child_id);
        BVH_ASSERT(child_num != INVALID);
        parent.children[child_num] = p_new_child_id;

        TNode& new_child    = _nodes[p_new_child_id];
        new_child.parent_id = p_parent_id;
    }

    void node_remove_child(
        uint32_t p_parent_id,
        uint32_t p_child_id,
        uint32_t p_tree_id,
        bool p_prevent_sibling = false
    ) {
        TNode& parent = _nodes[p_parent_id];
        BVH_ASSERT(!parent.is_leaf());

        int child_num = parent.find_child(p_child_id);
        BVH_ASSERT(child_num != INVALID);

        parent.remove_child_internal(child_num);

        // Currently, there is no need to keep back references for children.

        // Always a node id, as tnode is never a leaf.
        uint32_t sibling_id;
        bool sibling_present = false;

        // Don't delete if there are more children, or it is the root node.
        if (parent.num_children > 1) {
            return;
        }

        if (parent.num_children == 1) {
            // A redundant node with one child can be removed.
            sibling_id      = parent.children[0];
            sibling_present = true;
        }

        // If there are no children in this node, it can be removed.
        uint32_t grandparent_id = parent.parent_id;

        // Special case for a root node.
        if (grandparent_id == INVALID) {
            if (sibling_present) {
                change_root_node(sibling_id, p_tree_id);

                // Delete the old root node, because it is no longer needed.
                node_free_node_and_leaf(p_parent_id);
            }

            return;
        }

        if (sibling_present) {
            node_replace_child(grandparent_id, p_parent_id, sibling_id);
        } else {
            node_remove_child(grandparent_id, p_parent_id, p_tree_id, true);
        }

        // Put the node on the free list to recycle.
        node_free_node_and_leaf(p_parent_id);
    }

    // A node can either be a node, or a node AND a leaf combo.
    // Both must be deleted to prevent a leak.
    void node_free_node_and_leaf(uint32_t p_node_id) {
        TNode& node = _nodes[p_node_id];
        if (node.is_leaf()) {
            int leaf_id = node.get_leaf_id();
            _leaves.free(leaf_id);
        }

        _nodes.free(p_node_id);
    }

    void change_root_node(uint32_t p_new_root_id, uint32_t p_tree_id) {
        _root_node_id[p_tree_id] = p_new_root_id;
        TNode& root              = _nodes[p_new_root_id];

        // A root node has no parent.
        root.parent_id = INVALID;
    }

    void node_make_leaf(uint32_t p_node_id) {
        uint32_t child_leaf_id;
        TLeaf* child_leaf = _leaves.request(child_leaf_id);
        child_leaf->clear();

        BVH_ASSERT(child_leaf_id != 0);

        TNode& node      = _nodes[p_node_id];
        node.neg_leaf_id = -(int)child_leaf_id;
    }

    void node_remove_item(
        uint32_t p_ref_id,
        uint32_t p_tree_id,
        BVHAABB_CLASS* r_old_aabb = nullptr
    ) {
        ItemRef& ref           = _refs[p_ref_id];
        uint32_t owner_node_id = ref.tnode_id;

        // TODO: Check if this is needed.
        if (owner_node_id == INVALID) {
            return;
        }

        TNode& tnode = _nodes[owner_node_id];
        CRASH_COND(!tnode.is_leaf());

        TLeaf& leaf = _node_get_leaf(tnode);

        // If the AABB is not determining the corner size, don't refit,
        // because merging AABBs takes a lot of time.
        const BVHAABB_CLASS& old_aabb = leaf.get_aabb(ref.item_id);

        // Shrink a little to prevent using corner AABBs.
        // To miss the corners, first we shrink by node_expansion, which is
        // added to the overall bound of the leaf.
        // Second, we shrink by an epsilon, to miss the very corner AABBs,
        // which are important in determining the bound.
        // AABBs within this can be removed and not affect the overall bound.
        BVHAABB_CLASS node_bound = tnode.aabb;
        node_bound.expand(-_node_expansion - 0.001f);
        bool refit = true;

        if (node_bound.is_other_within(old_aabb)) {
            refit = false;
        }

        // The old AABB is used for incremental remove and reinsert.
        if (r_old_aabb) {
            *r_old_aabb = old_aabb;
        }

        leaf.remove_item_unordered(ref.item_id);

        if (leaf.num_items) {
            // The swapped item reference is changed to the new item id.
            uint32_t swapped_ref_id = leaf.get_item_ref_id(ref.item_id);

            ItemRef& swapped_ref = _refs[swapped_ref_id];

            swapped_ref.item_id = ref.item_id;

            // Only refit if it is an edge item.
            // We defer the refit updates until the update function is called
            // once per frame, because it is expensive.
            if (refit) {
                leaf.set_dirty(true);
            }
        } else {
            // Remove node if it is empty, and remove the link from parent.
            if (tnode.parent_id != INVALID) {
                // Check if the root noode only has one child.
                uint32_t parent_id = tnode.parent_id;

                node_remove_child(parent_id, owner_node_id, p_tree_id);
                refit_upward(parent_id);

                // Recycle the node.
                node_free_node_and_leaf(owner_node_id);
            }
        }

        ref.tnode_id = INVALID;
        ref.item_id  = INVALID;
    }

    // Return true if parent tree needs a refit.
    // The node AABB is calculated within this routine.
    bool _node_add_item(
        uint32_t p_node_id,
        uint32_t p_ref_id,
        const BVHAABB_CLASS& p_aabb
    ) {
        ItemRef& ref = _refs[p_ref_id];
        ref.tnode_id = p_node_id;

        TNode& node = _nodes[p_node_id];
        BVH_ASSERT(node.is_leaf());
        TLeaf& leaf = _node_get_leaf(node);

        // We only need to refit if the added item is changing the node's AABB.
        bool needs_refit = true;

        BVHAABB_CLASS expanded = p_aabb;
        expanded.expand(_node_expansion);

        // The bound will only be valid if it contains an item already.
        if (leaf.num_items) {
            if (node.aabb.is_other_within(expanded)) {
                // no change to node AABBs
                needs_refit = false;
            } else {
                node.aabb.merge(expanded);
            }
        } else {
            node.aabb = expanded;
        }

        ref.item_id = leaf.request_item();

        // Set the aabb of the new item.
        leaf.get_aabb(ref.item_id) = p_aabb;

        // Back reference of the item back to the item reference.
        leaf.get_item_ref_id(ref.item_id) = p_ref_id;

        return needs_refit;
    }

    uint32_t _node_create_another_child(
        uint32_t p_node_id,
        const BVHAABB_CLASS& p_aabb
    ) {
        uint32_t child_node_id;
        TNode* child_node = _nodes.request(child_node_id);
        child_node->clear();

        // TODO: Check if necessary.
        child_node->aabb = p_aabb;

        node_add_child(p_node_id, child_node_id);

        return child_node_id;
    }

public:
    struct CullParams {
        int result_count_overall; // both trees
        int result_count;         // this tree only
        int result_max;
        T** result_array;
        int* subindex_array;

        // TODO: Understand how masks are intended to work.
        uint32_t mask;
        uint32_t pairable_type;

        // Optional components for different tests.
        Point point;
        BVHAABB_CLASS bvh_aabb;
        typename BVHAABB_CLASS::ConvexHull hull;
        typename BVHAABB_CLASS::Segment segment;

        // When collision testing, non pairable moving items only need to be
        // tested against the pairable tree.
        // Collisions with other non pairable items are irrelevant.
        bool test_pairable_only;
    };

private:
    void _cull_translate_hits(CullParams& p) {
        int num_hits = _cull_hits.size();
        int left     = p.result_max - p.result_count_overall;

        if (num_hits > left) {
            num_hits = left;
        }

        int out_n = p.result_count_overall;

        for (int n = 0; n < num_hits; n++) {
            uint32_t ref_id = _cull_hits[n];

            const ItemExtra& ex   = _extra[ref_id];
            p.result_array[out_n] = ex.userdata;

            if (p.subindex_array) {
                p.subindex_array[out_n] = ex.subindex;
            }

            out_n++;
        }

        p.result_count          = num_hits;
        p.result_count_overall += num_hits;
    }

public:
    int cull_convex(CullParams& r_params, bool p_translate_hits = true) {
        _cull_hits.clear();
        r_params.result_count = 0;

        for (int n = 0; n < NUM_TREES; n++) {
            if (_root_node_id[n] == INVALID) {
                continue;
            }

            _cull_convex_iterative(_root_node_id[n], r_params);
        }

        if (p_translate_hits) {
            _cull_translate_hits(r_params);
        }

        return r_params.result_count;
    }

    int cull_segment(CullParams& r_params, bool p_translate_hits = true) {
        _cull_hits.clear();
        r_params.result_count = 0;

        for (int n = 0; n < NUM_TREES; n++) {
            if (_root_node_id[n] == INVALID) {
                continue;
            }

            _cull_segment_iterative(_root_node_id[n], r_params);
        }

        if (p_translate_hits) {
            _cull_translate_hits(r_params);
        }

        return r_params.result_count;
    }

    int cull_point(CullParams& r_params, bool p_translate_hits = true) {
        _cull_hits.clear();
        r_params.result_count = 0;

        for (int n = 0; n < NUM_TREES; n++) {
            if (_root_node_id[n] == INVALID) {
                continue;
            }

            _cull_point_iterative(_root_node_id[n], r_params);
        }

        if (p_translate_hits) {
            _cull_translate_hits(r_params);
        }

        return r_params.result_count;
    }

    int cull_aabb(CullParams& r_params, bool p_translate_hits = true) {
        _cull_hits.clear();
        r_params.result_count = 0;

        for (int n = 0; n < NUM_TREES; n++) {
            if (_root_node_id[n] == INVALID) {
                continue;
            }

            if ((n == 0) && r_params.test_pairable_only) {
                continue;
            }

            _cull_aabb_iterative(_root_node_id[n], r_params);
        }

        if (p_translate_hits) {
            _cull_translate_hits(r_params);
        }

        return r_params.result_count;
    }

    bool _cull_hits_full(const CullParams& p) {
        // Instead of checking every hit, do a lazy check for this condition.
        // It isn't a problem if we write too many _cull_hits, because
        // we only the result_max amount will be translated and outputted.
        // We stop the cull checks after the maximum has been reached.
        return (int)_cull_hits.size() >= p.result_max;
    }

    bool _cull_pairing_mask_test_hit(
        uint32_t p_maskA,
        uint32_t p_typeA,
        uint32_t p_maskB,
        uint32_t p_typeB
    ) const {
        // TODO: Check for a possible source of bugs.
        bool A_match_B = p_maskA & p_typeB;

        if (!A_match_B) {
            bool B_match_A = p_maskB & p_typeA;
            if (!B_match_A) {
                return false;
            }
        }

        return true;
    }

    void _cull_hit(uint32_t p_ref_id, CullParams& p) {
        // It would be more efficient to do before plane checks,
        // but done here to ease gettting started.
        if (use_pairs) {
            const ItemExtra& ex = _extra[p_ref_id];

            if (!_cull_pairing_mask_test_hit(
                    p.mask,
                    p.pairable_type,
                    ex.pairable_mask,
                    ex.pairable_type
                )) {
                return;
            }
        }

        _cull_hits.push_back(p_ref_id);
    }

    bool _cull_segment_iterative(uint32_t p_node_id, CullParams& r_params) {
        // Our function parameters to keep on a stack.
        struct CullSegParams {
            uint32_t node_id;
        };

        // Most of the iterative functionality is contained in the helper class.
        IterativeInfo<CullSegParams> ii;

        // Allocate the stack from this function, because it cannot be allocated
        // in the helper class.
        ii.stack = (CullSegParams*)alloca(ii.get_alloca_stacksize());

        // Seed the stack.
        ii.get_first()->node_id = p_node_id;

        CullSegParams csp;

        // Loop while there are still more nodes on the stack.
        while (ii.pop(csp)) {
            TNode& tnode = _nodes[csp.node_id];

            if (tnode.is_leaf()) {
                // Lazy check for hits full condition.
                if (_cull_hits_full(r_params)) {
                    return false;
                }

                TLeaf& leaf = _node_get_leaf(tnode);

                // Test the children individually.
                for (int n = 0; n < leaf.num_items; n++) {
                    const BVHAABB_CLASS& aabb = leaf.get_aabb(n);

                    if (aabb.intersects_segment(r_params.segment)) {
                        uint32_t child_id = leaf.get_item_ref_id(n);

                        // Register a hit.
                        _cull_hit(child_id, r_params);
                    }
                }
            } else {
                // Test the children individually.
                for (int n = 0; n < tnode.num_children; n++) {
                    uint32_t child_id                   = tnode.children[n];
                    const BVHAABB_CLASS& child_bvh_aabb = _nodes[child_id].aabb;

                    if (child_bvh_aabb.intersects_segment(r_params.segment)) {
                        // Add to the stack.
                        CullSegParams* child = ii.request();
                        child->node_id       = child_id;
                    }
                }
            }

        } // Loop while more nodes to pop.

        // Return true if results are not full.
        return true;
    }

    bool _cull_point_iterative(uint32_t p_node_id, CullParams& r_params) {
        // Function parameters are kept on the stack.
        struct CullPointParams {
            uint32_t node_id;
        };

        IterativeInfo<CullPointParams> ii;

        // Allocate the stack from this function, because it cannot be allocated
        // in the helper class.
        ii.stack = (CullPointParams*)alloca(ii.get_alloca_stacksize());

        // Seed the stack.
        ii.get_first()->node_id = p_node_id;

        CullPointParams cpp;

        // Loop while there are still more nodes on the stack.
        while (ii.pop(cpp)) {
            TNode& tnode = _nodes[cpp.node_id];
            // Check for hit.
            if (!tnode.aabb.intersects_point(r_params.point)) {
                continue;
            }

            if (tnode.is_leaf()) {
                // Lazy check for hits full condition.
                if (_cull_hits_full(r_params)) {
                    return false;
                }

                TLeaf& leaf = _node_get_leaf(tnode);

                // Test the children individually.
                for (int n = 0; n < leaf.num_items; n++) {
                    if (leaf.get_aabb(n).intersects_point(r_params.point)) {
                        uint32_t child_id = leaf.get_item_ref_id(n);

                        // Register a hit.
                        _cull_hit(child_id, r_params);
                    }
                }
            } else {
                // Test the children individually.
                for (int n = 0; n < tnode.num_children; n++) {
                    uint32_t child_id = tnode.children[n];

                    // Add to the stack.
                    CullPointParams* child = ii.request();
                    child->node_id         = child_id;
                }
            }

        } // Loop while more nodes to pop.

        // Return true if the results are not full.
        return true;
    }

    bool _cull_aabb_iterative(
        uint32_t p_node_id,
        CullParams& r_params,
        bool p_fully_within = false
    ) {
        struct CullAABBParams {
            uint32_t node_id;
            bool fully_within;
        };

        IterativeInfo<CullAABBParams> ii;

        // Allocate the stack from this function, because it cannot be allocated
        // in the helper class.
        ii.stack = (CullAABBParams*)alloca(ii.get_alloca_stacksize());

        // Seed the stack.
        ii.get_first()->node_id      = p_node_id;
        ii.get_first()->fully_within = p_fully_within;

        CullAABBParams cap;

        // Loop while there are still more nodes on the stack.
        while (ii.pop(cap)) {
            TNode& tnode = _nodes[cap.node_id];

            if (tnode.is_leaf()) {
                // Lazy check for hits full condition.
                if (_cull_hits_full(r_params)) {
                    return false;
                }

                TLeaf& leaf = _node_get_leaf(tnode);

                // If fully within, we add all items that pass mask checks.
                if (cap.fully_within) {
                    for (int n = 0; n < leaf.num_items; n++) {
                        uint32_t child_id = leaf.get_item_ref_id(n);

                        // Register a hit.
                        _cull_hit(child_id, r_params);
                    }
                } else {
                    for (int n = 0; n < leaf.num_items; n++) {
                        const BVHAABB_CLASS& aabb = leaf.get_aabb(n);

                        if (aabb.intersects(r_params.bvh_aabb)) {
                            uint32_t child_id = leaf.get_item_ref_id(n);

                            // Register a hit.
                            _cull_hit(child_id, r_params);
                        }
                    }
                }
            } else {
                if (!cap.fully_within) {
                    // Test children individually.
                    for (int n = 0; n < tnode.num_children; n++) {
                        uint32_t child_id = tnode.children[n];
                        const BVHAABB_CLASS& child_bvh_aabb =
                            _nodes[child_id].aabb;

                        if (child_bvh_aabb.intersects(r_params.bvh_aabb)) {
                            // Is the node totally within the aabb?
                            bool fully_within =
                                r_params.bvh_aabb.is_other_within(child_bvh_aabb
                                );

                            // Add to the stack.
                            CullAABBParams* child = ii.request();

                            // Should always return a valid child.
                            child->node_id      = child_id;
                            child->fully_within = fully_within;
                        }
                    }
                } else {
                    for (int n = 0; n < tnode.num_children; n++) {
                        uint32_t child_id = tnode.children[n];

                        // Add to the stack.
                        CullAABBParams* child = ii.request();

                        // Should always return a valid child.
                        child->node_id      = child_id;
                        child->fully_within = true;
                    }
                }
            }

        } // Loop while more nodes to pop.

        // Return true if results are not full.
        return true;
    }

    // Returns full with results.
    bool _cull_convex_iterative(
        uint32_t p_node_id,
        CullParams& r_params,
        bool p_fully_within = false
    ) {
        // Our function parameters are kept on the stack.
        struct CullConvexParams {
            uint32_t node_id;
            bool fully_within;
        };

        IterativeInfo<CullConvexParams> ii;

        // Allocate the stack from this function, because it cannot be allocated
        // in the helper class.
        ii.stack = (CullConvexParams*)alloca(ii.get_alloca_stacksize());

        // Seed the stack.
        ii.get_first()->node_id      = p_node_id;
        ii.get_first()->fully_within = p_fully_within;

        // Pre-allocate these as a once off to be reused.
        uint32_t max_planes = r_params.hull.num_planes;
        uint32_t* plane_ids = (uint32_t*)alloca(sizeof(uint32_t) * max_planes);

        CullConvexParams ccp;

        // Loop while there are still more nodes on the stack.
        while (ii.pop(ccp)) {
            const TNode& tnode = _nodes[ccp.node_id];

            if (!ccp.fully_within) {
                IntersectResult result =
                    tnode.aabb.intersects_convex(r_params.hull);

                switch (result) {
                    case IntersectResult::MISS: {
                        continue;
                    } break;
                    case IntersectResult::PARTIAL: {
                    } break;
                    case IntersectResult::FULL: {
                        ccp.fully_within = true;
                    } break;
                }
            }

            if (tnode.is_leaf()) {
                // Lazy check for hits full condition.
                if (_cull_hits_full(r_params)) {
                    return false;
                }

                const TLeaf& leaf = _node_get_leaf(tnode);

                // If fully within, add all items taking masks into account.
                if (ccp.fully_within) {
                    for (int n = 0; n < leaf.num_items; n++) {
                        uint32_t child_id = leaf.get_item_ref_id(n);

                        // Register hit.
                        _cull_hit(child_id, r_params);
                    }

                } else {
                    // We can either use a naive check of all the planes against
                    // the AABB, or an optimized check, which finds in advance
                    // which of the planes can possibly cut the AABB, and only
                    // test those. This can be much faster.
#define BVH_CONVEX_CULL_OPTIMIZED
#ifdef BVH_CONVEX_CULL_OPTIMIZED
                    // First find which planes cut the AABB.
                    uint32_t num_planes = tnode.aabb.find_cutting_planes(
                        r_params.hull,
                        plane_ids
                    );
                    BVH_ASSERT(num_planes <= max_planes);

// #define BVH_CONVEX_CULL_OPTIMIZED_RIGOR_CHECK
#ifdef BVH_CONVEX_CULL_OPTIMIZED_RIGOR_CHECK
                    uint32_t results[MAX_ITEMS];
                    uint32_t num_results = 0;
#endif

                    // Test children individually.
                    for (int n = 0; n < leaf.num_items; n++) {
                        const BVHAABB_CLASS& aabb = leaf.get_aabb(n);

                        if (aabb.intersects_convex_optimized(
                                r_params.hull,
                                plane_ids,
                                num_planes
                            )) {
                            uint32_t child_id = leaf.get_item_ref_id(n);

#ifdef BVH_CONVEX_CULL_OPTIMIZED_RIGOR_CHECK
                            results[num_results++] = child_id;
#endif

                            // Register a hit.
                            _cull_hit(child_id, r_params);
                        }
                    }

#ifdef BVH_CONVEX_CULL_OPTIMIZED_RIGOR_CHECK
                    uint32_t test_count = 0;

                    for (int n = 0; n < leaf.num_items; n++) {
                        const BVHAABB_CLASS& aabb = leaf.get_aabb(n);

                        if (aabb.intersects_convex_partial(r_params.hull)) {
                            uint32_t child_id = leaf.get_item_ref_id(n);

                            CRASH_COND(child_id != results[test_count++]);
                            CRASH_COND(test_count > num_results);
                        }
                    }
#endif

#else  // ! BVH_CONVEX_CULL_OPTIMIZED

                    // Test children individually.
                    for (int n = 0; n < leaf.num_items; n++) {
                        const BVHAABB_CLASS& aabb = leaf.get_aabb(n);

                        if (aabb.intersects_convex_partial(r_params.hull)) {
                            uint32_t child_id = leaf.get_item_ref_id(n);

                            // If results full return false and exit.
                            if (!_cull_hit(child_id, r_params)) {
                                return false;
                            }
                        }
                    }
#endif // BVH_CONVEX_CULL_OPTIMIZED
                }
            } else {
                // Not fully within.
                for (int n = 0; n < tnode.num_children; n++) {
                    uint32_t child_id = tnode.children[n];

                    // Add to the stack.
                    CullConvexParams* child = ii.request();

                    // Should always return a valid child.
                    child->node_id      = child_id;
                    child->fully_within = ccp.fully_within;
                }
            }

        } // Loop while there are more nodes to pop.

        // Return true if results are not full.
        return true;
    }

public:
#ifdef BVH_VERBOSE
    void _debug_recursive_print_tree(int p_tree_id) const {
        if (_root_node_id[p_tree_id] != INVALID) {
            _debug_recursive_print_tree_node(_root_node_id[p_tree_id]);
        }
    }

    String _debug_aabb_to_string(const BVHAABB_CLASS& aabb) const {
        Point size = aabb.calculate_size();

        String sz;
        float vol = 0.0;

        for (int i = 0; i < Point::AXIS_COUNT; ++i) {
            sz += "(";
            sz += itos(aabb.min[i]);
            sz += " ~ ";
            sz += itos(-aabb.neg_max[i]);
            sz += ") ";

            vol += size[i];
        }

        sz += "vol " + itos(vol);

        return sz;
    }

    void _debug_recursive_print_tree_node(uint32_t p_node_id, int depth = 0)
        const {
        const TNode& tnode = _nodes[p_node_id];

        String sz = "";
        for (int n = 0; n < depth; n++) {
            sz += "\t";
        }
        sz += itos(p_node_id);

        if (tnode.is_leaf()) {
            sz                += " L";
            sz                += itos(tnode.height) + " ";
            const TLeaf& leaf  = _node_get_leaf(tnode);

            sz += "[";
            for (int n = 0; n < leaf.num_items; n++) {
                if (n) {
                    sz += ", ";
                }
                sz += "r";
                sz += itos(leaf.get_item_ref_id(n));
            }
            sz += "]  ";
        } else {
            sz += " N";
            sz += itos(tnode.height) + " ";
        }

        sz += _debug_aabb_to_string(tnode.aabb);
        print_line(sz);

        if (!tnode.is_leaf()) {
            for (int n = 0; n < tnode.num_children; n++) {
                _debug_recursive_print_tree_node(tnode.children[n], depth + 1);
            }
        }
    }
#endif

    void _integrity_check_all() {
#ifdef BVH_INTEGRITY_CHECKS
        for (int n = 0; n < NUM_TREES; n++) {
            uint32_t root = _root_node_id[n];
            if (root != INVALID) {
                _integrity_check_down(root);
            }
        }
#endif
    }

    void _integrity_check_up(uint32_t p_node_id) {
        TNode& node = _nodes[p_node_id];

        BVHAABB_CLASS bvh_aabb = node.aabb;
        node_update_aabb(node);

        BVHAABB_CLASS bvh_aabb2 = node.aabb;
        bvh_aabb2.expand(-_node_expansion);

        CRASH_COND(!bvh_aabb.is_other_within(bvh_aabb2));
    }

    void _integrity_check_down(uint32_t p_node_id) {
        const TNode& node = _nodes[p_node_id];

        if (node.is_leaf()) {
            _integrity_check_up(p_node_id);
        } else {
            CRASH_COND(node.num_children != 2);

            for (int n = 0; n < node.num_children; n++) {
                uint32_t child_id = node.children[n];

                // Check that the children's parent pointers are correct.
                TNode& child = _nodes[child_id];
                CRASH_COND(child.parent_id != p_node_id);

                _integrity_check_down(child_id);
            }
        }
    }

    // For slow incremental optimization, we periodically remove each item from
    // the tree and reinsert it to give it a chance to find a better position.
    void _logic_item_remove_and_reinsert(uint32_t p_ref_id) {
        ItemRef& ref = _refs[p_ref_id];

        if (!ref.is_active()) {
            return;
        }

        if (ref.item_id == INVALID) {
            return;
        }

        BVH_ASSERT(ref.tnode_id != INVALID);

        Handle temp_handle;
        temp_handle.set_id(p_ref_id);
        uint32_t tree_id = _handle_get_tree_id(temp_handle);

        // Remove and reinsert.
        BVHAABB_CLASS bvh_aabb;
        node_remove_item(p_ref_id, tree_id, &bvh_aabb);

        // We must choose where to add it to the tree.
        ref.tnode_id =
            _logic_choose_item_add_node(_root_node_id[tree_id], bvh_aabb);
        _node_add_item(ref.tnode_id, p_ref_id, bvh_aabb);

        refit_upward_and_balance(ref.tnode_id, tree_id);
    }

    //--------------------------------------------------------------------------------------------------
    /**
     * @file    q3DynamicAABBTree.h
     * @author  Randy Gaul
     * @date    10/10/2014
     *  Copyright (c) 2014 Randy Gaul http://www.randygaul.net
     *  This software is provided 'as-is', without any express or implied
     *  warranty. In no event will the authors be held liable for any damages
     *  arising from the use of this software.
     *  Permission is granted to anyone to use this software for any purpose,
     *  including commercial applications, and to alter it and redistribute it
     *  freely, subject to the following restrictions:
     *    1. The origin of this software must not be misrepresented; you must
     * not claim that you wrote the original software. If you use this software
     *       in a product, an acknowledgment in the product documentation would
     * be appreciated but is not required.
     *    2. Altered source versions must be plainly marked as such, and must
     * not be misrepresented as being the original software.
     *    3. This notice may not be removed or altered from any source
     * distribution.
     */
    //--------------------------------------------------------------------------------------------------

    // _logic_bvh_aabb_merge and _logic balance are based on the 'Balance'
    // function from Randy Gaul's qu3e: https://github.com/RandyGaul/qu3e

    BVHAABB_CLASS _logic_bvh_aabb_merge(
        const BVHAABB_CLASS& a,
        const BVHAABB_CLASS& b
    ) {
        BVHAABB_CLASS c = a;
        c.merge(b);
        return c;
    }

    int32_t _logic_balance(int32_t iA, uint32_t p_tree_id) {
        // Uncomment to bypass balance.
        // return iA;

        TNode* A = &_nodes[iA];

        if (A->is_leaf() || A->height == 1) {
            return iA;
        }

        /*        A
         *      /   \
         *     B     C
         *    / \   / \
         *   D   E F   G
         */

        CRASH_COND(A->num_children != 2);
        int32_t iB = A->children[0];
        int32_t iC = A->children[1];
        TNode* B   = &_nodes[iB];
        TNode* C   = &_nodes[iC];

        int32_t balance = C->height - B->height;

        // If C is higher, promote C.
        if (balance > 1) {
            int32_t iF = C->children[0];
            int32_t iG = C->children[1];
            TNode* F   = &_nodes[iF];
            TNode* G   = &_nodes[iG];

            // Grandparent point to C.
            if (A->parent_id != INVALID) {
                if (_nodes[A->parent_id].children[0] == iA) {
                    _nodes[A->parent_id].children[0] = iC;

                } else {
                    _nodes[A->parent_id].children[1] = iC;
                }
            } else {
                // TODO: Check this.
                change_root_node(iC, p_tree_id);
            }

            // Swap A and C.
            C->children[0] = iA;
            C->parent_id   = A->parent_id;
            A->parent_id   = iC;

            // Finish rotation.
            if (F->height > G->height) {
                C->children[1] = iF;
                A->children[1] = iG;
                G->parent_id   = iA;
                A->aabb        = _logic_bvh_aabb_merge(B->aabb, G->aabb);
                C->aabb        = _logic_bvh_aabb_merge(A->aabb, F->aabb);

                A->height = 1 + MAX(B->height, G->height);
                C->height = 1 + MAX(A->height, F->height);
            }

            else {
                C->children[1] = iG;
                A->children[1] = iF;
                F->parent_id   = iA;
                A->aabb        = _logic_bvh_aabb_merge(B->aabb, F->aabb);
                C->aabb        = _logic_bvh_aabb_merge(A->aabb, G->aabb);

                A->height = 1 + MAX(B->height, F->height);
                C->height = 1 + MAX(A->height, G->height);
            }

            return iC;
        }

        // If B is higher, promote B.
        else if (balance < -1) {
            int32_t iD = B->children[0];
            int32_t iE = B->children[1];
            TNode* D   = &_nodes[iD];
            TNode* E   = &_nodes[iE];

            // Grandparent point to B.
            if (A->parent_id != INVALID) {
                if (_nodes[A->parent_id].children[0] == iA) {
                    _nodes[A->parent_id].children[0] = iB;
                } else {
                    _nodes[A->parent_id].children[1] = iB;
                }
            }

            else {
                // TODO: Check this.
                change_root_node(iB, p_tree_id);
            }

            // Swap A and B.
            B->children[1] = iA;
            B->parent_id   = A->parent_id;
            A->parent_id   = iB;

            // Finish rotation.
            if (D->height > E->height) {
                B->children[0] = iD;
                A->children[0] = iE;
                E->parent_id   = iA;
                A->aabb        = _logic_bvh_aabb_merge(C->aabb, E->aabb);
                B->aabb        = _logic_bvh_aabb_merge(A->aabb, D->aabb);

                A->height = 1 + MAX(C->height, E->height);
                B->height = 1 + MAX(A->height, D->height);
            }

            else {
                B->children[0] = iE;
                A->children[0] = iD;
                D->parent_id   = iA;
                A->aabb        = _logic_bvh_aabb_merge(C->aabb, D->aabb);
                B->aabb        = _logic_bvh_aabb_merge(A->aabb, E->aabb);

                A->height = 1 + MAX(C->height, D->height);
                B->height = 1 + MAX(A->height, E->height);
            }

            return iB;
        }

        return iA;
    }

    // Either choose an existing node to add item to, or create a new node.
    uint32_t _logic_choose_item_add_node(
        uint32_t p_node_id,
        const BVHAABB_CLASS& p_aabb
    ) {
        while (true) {
            BVH_ASSERT(p_node_id != INVALID);
            TNode& tnode = _nodes[p_node_id];

            if (tnode.is_leaf()) {
                // If a leaf, and not full, use it.
                if (!node_is_leaf_full(tnode)) {
                    return p_node_id;
                }
                // Else split the leaf, and use one of the children.
                return split_leaf(p_node_id, p_aabb);
            }

            // TODO: This should not happen.
            // It is not serious, but it would be nice to prevent.
            // May only happen with the root node.
            if (tnode.num_children == 1) {
                WARN_PRINT_ONCE(
                    "BVH::recursive_choose_item_add_node, node with 1 child, "
                    "recovering"
                );
                p_node_id = tnode.children[0];
            } else {
                BVH_ASSERT(tnode.num_children == 2);
                TNode& childA = _nodes[tnode.children[0]];
                TNode& childB = _nodes[tnode.children[1]];
                int which =
                    p_aabb.select_by_proximity(childA.aabb, childB.aabb);

                p_node_id = tnode.children[which];
            }
        }
    }

    int _handle_get_tree_id(Handle p_handle) const {
        if (use_pairs) {
            int tree = 0;
            if (_extra[p_handle.id()].pairable) {
                tree = 1;
            }
            return tree;
        }
        return 0;
    }

public:
    void _handle_sort(Handle& p_ha, Handle& p_hb) const {
        if (p_ha.id() > p_hb.id()) {
            Handle temp = p_hb;
            p_hb        = p_ha;
            p_ha        = temp;
        }
    }

private:
    void create_root_node(int p_tree) {
        // If there is no root node, create one.
        if (_root_node_id[p_tree] == INVALID) {
            uint32_t root_node_id;
            TNode* node = _nodes.request(root_node_id);
            node->clear();
            _root_node_id[p_tree] = root_node_id;

            // Make the root node a leaf.
            uint32_t leaf_id;
            TLeaf* leaf = _leaves.request(leaf_id);
            leaf->clear();
            node->neg_leaf_id = -(int)leaf_id;
        }
    }

    bool node_is_leaf_full(TNode& tnode) const {
        const TLeaf& leaf = _node_get_leaf(tnode);
        return leaf.is_full();
    }

public:
    TLeaf& _node_get_leaf(TNode& tnode) {
        BVH_ASSERT(tnode.is_leaf());
        return _leaves[tnode.get_leaf_id()];
    }

    const TLeaf& _node_get_leaf(const TNode& tnode) const {
        BVH_ASSERT(tnode.is_leaf());
        return _leaves[tnode.get_leaf_id()];
    }

private:

public:
    Handle item_add(
        T* p_userdata,
        bool p_active,
        const BoundingBox& p_aabb,
        int32_t p_subindex,
        bool p_pairable,
        uint32_t p_pairable_type,
        uint32_t p_pairable_mask,
        bool p_invisible = false
    ) {
#ifdef BVH_VERBOSE_TREE
        VERBOSE_PRINT("\nitem_add BEFORE");
        _debug_recursive_print_tree(p_tree_id);
        VERBOSE_PRINT("\n");
#endif

        BVHAABB_CLASS bvh_aabb;
        bvh_aabb.from(p_aabb);

        // Note: We do not expand the AABB for the first create even if
        // leaf expansion is switched on, because:
        // 1. We don't know if this object will move in future.
        //    If it does, a non-expanded bound would be better.
        // 2. We don't know how many objects will be paired.
        //    This is used to modify the expansion margin.

        Handle handle;
        // Ref id easier to pass around than handle.
        uint32_t ref_id;

        // This should never fail.
        ItemRef* ref = _refs.request(ref_id);

        // The extra data should be a parallel list to the references.
        uint32_t extra_id;
        ItemExtra* extra = _extra.request(extra_id);
        BVH_ASSERT(extra_id == ref_id);

        // Pairs info.
        if (use_pairs) {
            uint32_t pairs_id;
            ItemPairs* pairs = _pairs.request(pairs_id);
            pairs->clear();
            BVH_ASSERT(pairs_id == ref_id);
        }

        extra->subindex          = p_subindex;
        extra->userdata          = p_userdata;
        extra->last_updated_tick = 0;

        // Add an active reference to the list for slow incremental optimize.
        // This list must be kept in sync with the references as they are added
        // or removed.
        extra->active_ref_id = _active_refs.size();
        _active_refs.push_back(ref_id);

        if (use_pairs) {
            extra->pairable_mask = p_pairable_mask;
            extra->pairable_type = p_pairable_type;
            extra->pairable      = p_pairable;
        } else {
            // For safety, in case it gets queried.
            extra->pairable = 0;
            p_pairable      = false;
        }

        // Assign to handle to return.
        handle.set_id(ref_id);

        uint32_t tree_id = 0;
        if (p_pairable) {
            tree_id = 1;
        }

        create_root_node(tree_id);

        // We must choose where to add it to the tree.
        if (p_active) {
            ref->tnode_id =
                _logic_choose_item_add_node(_root_node_id[tree_id], bvh_aabb);

            bool refit = _node_add_item(ref->tnode_id, ref_id, bvh_aabb);

            if (refit) {
                // Only need to refit from the parent.
                const TNode& add_node = _nodes[ref->tnode_id];
                if (add_node.parent_id != INVALID) {
                    refit_upward_and_balance(add_node.parent_id, tree_id);
                }
            }
        } else {
            ref->set_inactive();
        }

#ifdef BVH_VERBOSE
        int mem  = _refs.estimate_memory_use();
        mem     += _nodes.estimate_memory_use();

        String sz = _debug_aabb_to_string(abb);
        VERBOSE_PRINT(
            "\titem_add [" + itos(ref_id) + "] " + itos(_refs.used_size())
            + " refs,\t" + itos(_nodes.used_size()) + " nodes " + sz
        );
        VERBOSE_PRINT(
            "mem use : " + itos(mem)
            + ", num nodes reserved : " + itos(_nodes.reserved_size())
        );

#endif

        return handle;
    }

    void _debug_print_refs() {
#ifdef BVH_VERBOSE_TREE
        print_line("refs.....");
        for (int n = 0; n < _refs.size(); n++) {
            const ItemRef& ref = _refs[n];
            print_line(
                "tnode_id " + itos(ref.tnode_id) + ", item_id "
                + itos(ref.item_id)
            );
        }

#endif
    }

    // Return false if noop.
    bool item_move(Handle p_handle, const BoundingBox& p_aabb) {
        uint32_t ref_id = p_handle.id();

        ItemRef& ref = _refs[ref_id];
        if (!ref.is_active()) {
            return false;
        }

        BVHAABB_CLASS bvh_aabb;
        bvh_aabb.from(p_aabb);

#ifdef BVH_EXPAND_LEAF_AABBS
        if (use_pairs) {
            // Scale the pairing expansion by the number of pairs.
            bvh_aabb.expand(
                _pairs[ref_id].scale_expansion_margin(_pairing_expansion)
            );
        } else {
            bvh_aabb.expand(_pairing_expansion);
        }
#endif

        BVH_ASSERT(ref.tnode_id != INVALID);
        TNode& tnode = _nodes[ref.tnode_id];

        // Does it fit within the current leaf AABB?
        if (tnode.aabb.is_other_within(bvh_aabb)) {
            // It has not moved enough to require a refit: Do nothing.
            // However, we update the exact AABB in the leaf, as this will be
            // needed for accurate collision detection.
            TLeaf& leaf = _node_get_leaf(tnode);

            BVHAABB_CLASS& leaf_bvh_aabb = leaf.get_aabb(ref.item_id);

            // No change?
#ifdef BVH_EXPAND_LEAF_AABBS
            BoundingBox leaf_aabb;
            leaf_bvh_aabb.to(leaf_aabb);

            // This test should pass in a lot of cases, and by returning false
            // we can avoid collision pairing checks later, which greatly
            // reduces processing.
            if (expanded_aabb_encloses_not_shrink(leaf_aabb, p_aabb)) {
                return false;
            }
#else
            if (leaf_bvh_aabb == bvh_aabb) {
                return false;
            }
#endif

#ifdef BVH_VERBOSE_MOVES
            print_line(
                "item_move " + itos(p_handle.id())
                + "(within tnode aabb) : " + _debug_aabb_to_string(abb)
            );
#endif

            leaf_bvh_aabb = bvh_aabb;
            _integrity_check_all();

            return true;
        }

#ifdef BVH_VERBOSE_MOVES
        print_line(
            "item_move " + itos(p_handle.id())
            + "(outside tnode aabb) : " + _debug_aabb_to_string(abb)
        );
#endif

        uint32_t tree_id = _handle_get_tree_id(p_handle);

        // Remove and reinsert.
        node_remove_item(ref_id, tree_id);

        // We must choose where to add it to the tree.
        ref.tnode_id =
            _logic_choose_item_add_node(_root_node_id[tree_id], bvh_aabb);

        // Add it to the tree.
        bool needs_refit = _node_add_item(ref.tnode_id, ref_id, bvh_aabb);

        // Only need to refit from the parent.
        if (needs_refit) {
            const TNode& add_node = _nodes[ref.tnode_id];
            if (add_node.parent_id != INVALID) {
                // TODO: We don't need to rebalance all the time.
                refit_upward(add_node.parent_id);
            }
        }

        return true;
    }

    void item_remove(Handle p_handle) {
        uint32_t ref_id = p_handle.id();

        uint32_t tree_id = _handle_get_tree_id(p_handle);

        VERBOSE_PRINT("item_remove [" + itos(ref_id) + "] ");

        // Remove the active reference from the list for incremental optimize.
        // This list must be kept in sync with the references as they are added
        // or removed.
        uint32_t active_ref_id     = _extra[ref_id].active_ref_id;
        uint32_t ref_id_moved_back = _active_refs[_active_refs.size() - 1];

        // Swap back and decrement for fast, unordered remove.
        _active_refs[active_ref_id] = ref_id_moved_back;
        _active_refs.resize(_active_refs.size() - 1);

        // Keep the moved active reference up to date.
        _extra[ref_id_moved_back].active_ref_id = active_ref_id;

        // If active, remove the item from the node.
        if (_refs[ref_id].is_active()) {
            node_remove_item(ref_id, tree_id);
        }

        // Remove the item reference.
        _refs.free(ref_id);
        _extra.free(ref_id);
        if (use_pairs) {
            _pairs.free(ref_id);
        }

#ifdef BVH_VERBOSE_TREE
        _debug_recursive_print_tree(tree_id);
#endif
    }

    bool item_activate(Handle p_handle, const BoundingBox& p_aabb) {
        uint32_t ref_id = p_handle.id();
        ItemRef& ref    = _refs[ref_id];
        if (ref.is_active()) {
            return false;
        }

        // Add to the tree.
        BVHAABB_CLASS bvh_aabb;
        bvh_aabb.from(p_aabb);

        uint32_t tree_id = _handle_get_tree_id(p_handle);

        // We must choose where to add it to the tree.
        ref.tnode_id =
            _logic_choose_item_add_node(_root_node_id[tree_id], bvh_aabb);
        _node_add_item(ref.tnode_id, ref_id, bvh_aabb);

        refit_upward_and_balance(ref.tnode_id, tree_id);

        return true;
    }

    bool item_deactivate(Handle p_handle) {
        uint32_t ref_id = p_handle.id();
        ItemRef& ref    = _refs[ref_id];
        if (!ref.is_active()) {
            return false;
        }

        uint32_t tree_id = _handle_get_tree_id(p_handle);

        // Remove it from the tree.
        BVHAABB_CLASS bvh_aabb;
        node_remove_item(ref_id, tree_id, &bvh_aabb);

        // Mark as inactive.
        ref.set_inactive();
        return true;
    }

    bool item_get_active(Handle p_handle) const {
        uint32_t ref_id    = p_handle.id();
        const ItemRef& ref = _refs[ref_id];
        return ref.is_active();
    }

    // During collision testing, we set the from item's mask and pairable.
    void item_fill_cullparams(Handle p_handle, CullParams& r_params) const {
        uint32_t ref_id        = p_handle.id();
        const ItemExtra& extra = _extra[ref_id];

        // Only test from pairable items.
        r_params.test_pairable_only = extra.pairable == 0;

        // Take the mask of the from item into account.
        r_params.mask          = extra.pairable_mask;
        r_params.pairable_type = extra.pairable_type;
    }

    bool item_is_pairable(const Handle& p_handle) {
        uint32_t ref_id        = p_handle.id();
        const ItemExtra& extra = _extra[ref_id];
        return extra.pairable != 0;
    }

    void item_get_bvh_aabb(const Handle& p_handle, BVHAABB_CLASS& r_bvh_aabb) {
        uint32_t ref_id    = p_handle.id();
        const ItemRef& ref = _refs[ref_id];

        TNode& tnode = _nodes[ref.tnode_id];
        TLeaf& leaf  = _node_get_leaf(tnode);

        r_bvh_aabb = leaf.get_aabb(ref.item_id);
    }

    bool item_set_pairable(
        const Handle& p_handle,
        bool p_pairable,
        uint32_t p_pairable_type,
        uint32_t p_pairable_mask
    ) {
        uint32_t ref_id = p_handle.id();

        ItemExtra& ex = _extra[ref_id];
        ItemRef& ref  = _refs[ref_id];

        bool active           = ref.is_active();
        bool pairable_changed = (ex.pairable != 0) != p_pairable;
        bool state_changed    = pairable_changed
                          || (ex.pairable_type != p_pairable_type)
                          || (ex.pairable_mask != p_pairable_mask);

        ex.pairable_type = p_pairable_type;
        ex.pairable_mask = p_pairable_mask;

        if (active && pairable_changed) {
            // Record AABB
            TNode& tnode           = _nodes[ref.tnode_id];
            TLeaf& leaf            = _node_get_leaf(tnode);
            BVHAABB_CLASS bvh_aabb = leaf.get_aabb(ref.item_id);

            // Make sure the current tree is correct prior to changing.
            uint32_t tree_id = _handle_get_tree_id(p_handle);

            // Remove from the old tree.
            node_remove_item(ref_id, tree_id);

            // Set the pairable after getting the current tree,
            // because the pairable status determines which tree.
            ex.pairable = p_pairable;

            // Add to new tree.
            tree_id = _handle_get_tree_id(p_handle);
            create_root_node(tree_id);

            // We must choose where to add it to the tree.
            ref.tnode_id =
                _logic_choose_item_add_node(_root_node_id[tree_id], bvh_aabb);
            bool needs_refit = _node_add_item(ref.tnode_id, ref_id, bvh_aabb);

            // Only need to refit from the parent.
            if (needs_refit) {
                const TNode& add_node = _nodes[ref.tnode_id];
                if (add_node.parent_id != INVALID) {
                    refit_upward_and_balance(add_node.parent_id, tree_id);
                }
            }
        } else {
            ex.pairable = p_pairable;
        }

        return state_changed;
    }

    void incremental_optimize() {
        // First update all AABBs as a one off step.
        // Cheaper than doing it on each move as each leaf may get touched
        // multiple times in a frame.
        for (int n = 0; n < NUM_TREES; n++) {
            if (_root_node_id[n] != INVALID) {
                refit_branch(_root_node_id[n]);
            }
        }

        // Do a small section, reinserting to get things moving gradually, and
        // keep items in the right leaf.
        if (_current_active_ref >= _active_refs.size()) {
            _current_active_ref = 0;
        }

        if (!_active_refs.size()) {
            return;
        }

        uint32_t ref_id = _active_refs[_current_active_ref++];

        _logic_item_remove_and_reinsert(ref_id);
    }

    void update() {
        incremental_optimize();

        // Keep the expansion values up to date with the world bound.
// #define BVH_ALLOW_AUTO_EXPANSION
#ifdef BVH_ALLOW_AUTO_EXPANSION
        if (_auto_node_expansion || _auto_pairing_expansion) {
            BVHAABB_CLASS world_bound;
            world_bound.set_to_max_opposite_extents();

            bool bound_valid = false;

            for (int n = 0; n < NUM_TREES; n++) {
                uint32_t node_id = _root_node_id[n];
                if (node_id != INVALID) {
                    world_bound.merge(_nodes[node_id].aabb);
                    bound_valid = true;
                }
            }

            if (bound_valid) {
                BoundingBox bb;
                world_bound.to(bb);
                real_t size = bb.get_longest_axis_size();

                // Automatic AI decision for best parameters.
                // These can be overridden in project settings.

                // These magic numbers are determined by experiment.
                if (_auto_node_expansion) {
                    _node_expansion = size * 0.025;
                }
                if (_auto_pairing_expansion) {
                    _pairing_expansion = size * 0.009;
                }
            }
        }
#endif
    }

    void params_set_pairing_expansion(real_t p_value) {
        if (p_value < 0.0) {
#ifdef BVH_ALLOW_AUTO_EXPANSION
            _auto_pairing_expansion = true;
#endif
            return;
        }
#ifdef BVH_ALLOW_AUTO_EXPANSION
        _auto_pairing_expansion = false;
#endif

        _pairing_expansion = p_value;

        // Calculate shrinking threshold.
        const real_t fudge_factor = 1.1;
        _aabb_shrinkage_threshold =
            _pairing_expansion * Point::AXIS_COUNT * 2.0 * fudge_factor;
    }

    // Ålso checks for special case of shrinkage.
    bool expanded_aabb_encloses_not_shrink(
        const BoundingBox& p_expanded_aabb,
        const BoundingBox& p_aabb
    ) const {
        if (!p_expanded_aabb.encloses(p_aabb)) {
            return false;
        }

        // Check for special case of shrinkage. If the AABB has shrunk
        // significantly, we want to create a new expanded bound, because
        // the previous expanded bound will have diverged significantly.
        const Point& exp_size = p_expanded_aabb.size;
        const Point& new_size = p_aabb.size;

        real_t exp_l = 0.0;
        real_t new_l = 0.0;

        for (int i = 0; i < Point::AXIS_COUNT; ++i) {
            exp_l += exp_size[i];
            new_l += new_size[i];
        }

        // Is the difference above some metric.
        real_t diff = exp_l - new_l;
        if (diff < _aabb_shrinkage_threshold) {
            return true;
        }

        return false;
    }

    void _debug_node_verify_bound(uint32_t p_node_id) {
        TNode& node                   = _nodes[p_node_id];
        BVHAABB_CLASS bvh_aabb_before = node.aabb;

        node_update_aabb(node);

        BVHAABB_CLASS bvh_aabb_after = node.aabb;
        CRASH_COND(bvh_aabb_before != bvh_aabb_after);
    }

    void node_update_aabb(TNode& tnode) {
        tnode.aabb.set_to_max_opposite_extents();
        tnode.height = 0;

        if (!tnode.is_leaf()) {
            for (int n = 0; n < tnode.num_children; n++) {
                uint32_t child_node_id = tnode.children[n];

                // Merge with child AABB.
                const TNode& tchild = _nodes[child_node_id];
                tnode.aabb.merge(tchild.aabb);

                // Do heights at the same time.
                if (tchild.height > tnode.height) {
                    tnode.height = tchild.height;
                }
            }

            // The height of a non leaf is always +1 the biggest child.
            tnode.height++;

#ifdef BVH_CHECKS
            if (!tnode.num_children) {
                // An empty AABB will break the parent AABBs.
                WARN_PRINT("Tree::TNode no children, AABB is undefined");
            }
#endif
        } else {
            const TLeaf& leaf = _node_get_leaf(tnode);

            for (int n = 0; n < leaf.num_items; n++) {
                tnode.aabb.merge(leaf.get_aabb(n));
            }

            // The leaf items are unexpanded. We expand only in the node AABBs.
            tnode.aabb.expand(_node_expansion);
#ifdef BVH_CHECKS
            if (!leaf.num_items) {
                // An empty AABB will break the parent AABBs.
                WARN_PRINT("Tree::TLeaf no items, AABB is undefined");
            }
#endif
        }
    }

    void refit_all(int p_tree_id) {
        refit_downward(_root_node_id[p_tree_id]);
    }

    void refit_upward(uint32_t p_node_id) {
        while (p_node_id != INVALID) {
            TNode& tnode = _nodes[p_node_id];
            node_update_aabb(tnode);
            p_node_id = tnode.parent_id;
        }
    }

    void refit_upward_and_balance(uint32_t p_node_id, uint32_t p_tree_id) {
        while (p_node_id != INVALID) {
            uint32_t before = p_node_id;
            p_node_id       = _logic_balance(p_node_id, p_tree_id);

            if (before != p_node_id) {
                VERBOSE_PRINT("REBALANCED!");
            }

            TNode& tnode = _nodes[p_node_id];

            // Update overall AABB from the children.
            node_update_aabb(tnode);

            p_node_id = tnode.parent_id;
        }
    }

    void refit_downward(uint32_t p_node_id) {
        TNode& tnode = _nodes[p_node_id];

        // Do children first.
        if (!tnode.is_leaf()) {
            for (int n = 0; n < tnode.num_children; n++) {
                refit_downward(tnode.children[n]);
            }
        }

        node_update_aabb(tnode);
    }

    // Go down to the leaves, then refit upward.
    void refit_branch(uint32_t p_node_id) {
        struct RefitParams {
            uint32_t node_id;
        };

        IterativeInfo<RefitParams> ii;

        // Allocate the stack from this function, because it cannot be allocated
        // in the helper class.
        ii.stack = (RefitParams*)alloca(ii.get_alloca_stacksize());

        // Seed the stack.
        ii.get_first()->node_id = p_node_id;

        RefitParams rp;

        // Loop while there are still more nodes on the stack.
        while (ii.pop(rp)) {
            TNode& tnode = _nodes[rp.node_id];

            // Do children first.
            if (!tnode.is_leaf()) {
                for (int n = 0; n < tnode.num_children; n++) {
                    uint32_t child_id = tnode.children[n];

                    // Add to the stack.
                    RefitParams* child = ii.request();
                    child->node_id     = child_id;
                }
            } else {
                // Only refit upward if leaf is dirty.
                TLeaf& leaf = _node_get_leaf(tnode);
                if (leaf.is_dirty()) {
                    leaf.set_dirty(false);
                    refit_upward(p_node_id);
                }
            }
        } // Loop while more nodes to pop.
    }

    void _split_inform_references(uint32_t p_node_id) {
        TNode& node = _nodes[p_node_id];
        TLeaf& leaf = _node_get_leaf(node);

        for (int n = 0; n < leaf.num_items; n++) {
            uint32_t ref_id = leaf.get_item_ref_id(n);

            ItemRef& ref = _refs[ref_id];
            ref.tnode_id = p_node_id;
            ref.item_id  = n;
        }
    }

    void _split_leaf_sort_groups_simple(
        int& num_a,
        int& num_b,
        uint16_t* group_a,
        uint16_t* group_b,
        const BVHAABB_CLASS* temp_bounds,
        const BVHAABB_CLASS full_bound
    ) {
        // Special case for low leaf sizes.
        if (MAX_ITEMS < 4) {
            uint32_t ind = group_a[0];

            // Add to b.
            group_b[num_b++] = ind;

            // Remove from a.
            group_a[0] = group_a[num_a - 1];
            num_a--;
            return;
        }

        Point centre = full_bound.calculate_centre();
        Point size   = full_bound.calculate_size();

        int order[Point::AXIS_COUNT];

        order[0]                     = size.min_axis();
        order[Point::AXIS_COUNT - 1] = size.max_axis();

        static_assert(
            Point::AXIS_COUNT <= 3,
            "BVH Point::AXIS_COUNT has unexpected size"
        );
        if (Point::AXIS_COUNT == 3) {
            order[1] = 3 - (order[0] + order[2]);
        }

        // Simplest case, split on the longest axis.
        int split_axis = order[0];
        for (int a = 0; a < num_a; a++) {
            uint32_t ind = group_a[a];

            if (temp_bounds[ind].min.coord[split_axis]
                > centre.coord[split_axis]) {
                // Add to b.
                group_b[num_b++] = ind;

                // Remove from a.
                group_a[a] = group_a[num_a - 1];
                num_a--;

                // Do this one again, because it has been replaced.
                a--;
            }
        }

        // Detect when split on longest axis failed.
        int min_threshold = MAX_ITEMS / 4;
        int min_group_size[Point::AXIS_COUNT];
        min_group_size[0] = MIN(num_a, num_b);
        if (min_group_size[0] < min_threshold) {
            // Slow but secure. First move everything back into a.
            for (int b = 0; b < num_b; b++) {
                group_a[num_a++] = group_b[b];
            }
            num_b = 0;

            // Calculate the best split.
            for (int axis = 1; axis < Point::AXIS_COUNT; axis++) {
                split_axis = order[axis];
                int count  = 0;

                for (int a = 0; a < num_a; a++) {
                    uint32_t ind = group_a[a];

                    if (temp_bounds[ind].min.coord[split_axis]
                        > centre.coord[split_axis]) {
                        count++;
                    }
                }

                min_group_size[axis] = MIN(count, num_a - count);
            }

            // Best axis.
            int best_axis = 0;
            int best_min  = min_group_size[0];
            for (int axis = 1; axis < Point::AXIS_COUNT; axis++) {
                if (min_group_size[axis] > best_min) {
                    best_min  = min_group_size[axis];
                    best_axis = axis;
                }
            }

            // Do the split.
            if (best_min > 0) {
                split_axis = order[best_axis];

                for (int a = 0; a < num_a; a++) {
                    uint32_t ind = group_a[a];

                    if (temp_bounds[ind].min.coord[split_axis]
                        > centre.coord[split_axis]) {
                        // Add to b.
                        group_b[num_b++] = ind;

                        // Remove from a.
                        group_a[a] = group_a[num_a - 1];
                        num_a--;

                        // Do this one again, because it has been replaced.
                        a--;
                    }
                }
            }
        }

        // Special cases, none crossed threshold.
        if (!num_b) {
            uint32_t ind = group_a[0];

            // Add to b.
            group_b[num_b++] = ind;

            // Remove from a.
            group_a[0] = group_a[num_a - 1];
            num_a--;
        }
        if (!num_a) {
            uint32_t ind = group_b[0];

            // Add to a.
            group_a[num_a++] = ind;

            // Remove from b.
            group_b[0] = group_b[num_b - 1];
            num_b--;
        }
    }

    void _split_leaf_sort_groups(
        int& num_a,
        int& num_b,
        uint16_t* group_a,
        uint16_t* group_b,
        const BVHAABB_CLASS* temp_bounds
    ) {
        BVHAABB_CLASS groupb_aabb;
        groupb_aabb.set_to_max_opposite_extents();
        for (int n = 0; n < num_b; n++) {
            int which = group_b[n];
            groupb_aabb.merge(temp_bounds[which]);
        }
        BVHAABB_CLASS groupb_aabb_new;

        BVHAABB_CLASS rest_aabb;

        float best_size    = FLT_MAX;
        int best_candidate = -1;

        // Find the most likely from a to move into b.
        for (int check = 0; check < num_a; check++) {
            rest_aabb.set_to_max_opposite_extents();
            groupb_aabb_new = groupb_aabb;

            // Find AABB of all the rest.
            for (int rest = 0; rest < num_a; rest++) {
                if (rest == check) {
                    continue;
                }

                int which = group_a[rest];
                rest_aabb.merge(temp_bounds[which]);
            }

            groupb_aabb_new.merge(temp_bounds[group_a[check]]);

            // Compare the sizes.
            float size = groupb_aabb_new.get_area() + rest_aabb.get_area();
            if (size < best_size) {
                best_size      = size;
                best_candidate = check;
            }
        }

        // Move the best from group a to group b.
        group_b[num_b++] = group_a[best_candidate];
        num_a--;
        group_a[best_candidate] = group_a[num_a];
    }

    uint32_t split_leaf(
        uint32_t p_node_id,
        const BVHAABB_CLASS& p_added_item_aabb
    ) {
        return split_leaf_complex(p_node_id, p_added_item_aabb);
    }

    // AABB is the new inserted node.
    uint32_t split_leaf_complex(
        uint32_t p_node_id,
        const BVHAABB_CLASS& p_added_item_aabb
    ) {
        VERBOSE_PRINT("split_leaf");

        // Note: The tnode before and after splitting may be a different address
        // in memory, because the vector could get relocated.
        // We need to reget the tnode after the split.
        BVH_ASSERT(_nodes[p_node_id].is_leaf());

        // Create child leaf nodes.
        uint32_t* child_ids =
            (uint32_t*)alloca(sizeof(uint32_t) * MAX_CHILDREN);

        for (int n = 0; n < MAX_CHILDREN; n++) {
            // Create node children.
            TNode* child_node = _nodes.request(child_ids[n]);

            child_node->clear();

            // Back link to parent.
            child_node->parent_id = p_node_id;

            // Make each child a leaf node.
            node_make_leaf(child_ids[n]);
        }

        // Don't get any leaves or nodes untill after the split.
        TNode& tnode           = _nodes[p_node_id];
        uint32_t orig_leaf_id  = tnode.get_leaf_id();
        const TLeaf& orig_leaf = _node_get_leaf(tnode);

        // Store the final child ids.
        for (int n = 0; n < MAX_CHILDREN; n++) {
            tnode.children[n] = child_ids[n];
        }

        // Mark as no longer a leaf node.
        tnode.num_children = MAX_CHILDREN;

        // Assign childeren to each group equally.
        // Plus 1 for the wildcard. The item being added.
        int max_children = orig_leaf.num_items + 1;

        uint16_t* group_a = (uint16_t*)alloca(sizeof(uint16_t) * max_children);
        uint16_t* group_b = (uint16_t*)alloca(sizeof(uint16_t) * max_children);

        // We are copying the AABBs, because we need one for the inserted item.
        BVHAABB_CLASS* temp_bounds =
            (BVHAABB_CLASS*)alloca(sizeof(BVHAABB_CLASS) * max_children);

        int num_a = max_children;
        int num_b = 0;

        // Setup: Start with all in group a.
        for (int n = 0; n < orig_leaf.num_items; n++) {
            group_a[n]     = n;
            temp_bounds[n] = orig_leaf.get_aabb(n);
        }
        int wildcard = orig_leaf.num_items;

        group_a[wildcard]     = wildcard;
        temp_bounds[wildcard] = p_added_item_aabb;

        // We can choose to either split equally, or just 1 in the new leaf.
        _split_leaf_sort_groups_simple(
            num_a,
            num_b,
            group_a,
            group_b,
            temp_bounds,
            tnode.aabb
        );

        uint32_t wildcard_node = INVALID;

        // Now there should be equal numbers in both groups.
        for (int n = 0; n < num_a; n++) {
            int which = group_a[n];

            if (which != wildcard) {
                const BVHAABB_CLASS& source_item_aabb =
                    orig_leaf.get_aabb(which);
                uint32_t source_item_ref_id = orig_leaf.get_item_ref_id(which);
                _node_add_item(
                    tnode.children[0],
                    source_item_ref_id,
                    source_item_aabb
                );
            } else {
                wildcard_node = tnode.children[0];
            }
        }
        for (int n = 0; n < num_b; n++) {
            int which = group_b[n];

            if (which != wildcard) {
                const BVHAABB_CLASS& source_item_aabb =
                    orig_leaf.get_aabb(which);
                uint32_t source_item_ref_id = orig_leaf.get_item_ref_id(which);
                _node_add_item(
                    tnode.children[1],
                    source_item_ref_id,
                    source_item_aabb
                );
            } else {
                wildcard_node = tnode.children[1];
            }
        }

        // Remove all items from the parent and replace with the child nodes.
        _leaves.free(orig_leaf_id);

        // Keep the references up to date.
        for (int n = 0; n < MAX_CHILDREN; n++) {
            _split_inform_references(tnode.children[n]);
        }

        refit_upward(p_node_id);

        BVH_ASSERT(wildcard_node != INVALID);
        return wildcard_node;
    }
};

#undef VERBOSE_PRINT

} // namespace BVH

#endif // BVH_TREE_H
