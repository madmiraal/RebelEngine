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
#include "core/math/bvh_cull_parameters.h"
#include "core/math/bvh_leaf.h"
#include "core/math/bvh_node.h"
#include "core/math/geometry.h"
#include "core/math/vector3.h"
#include "core/pooled_list.h"
#include "core/print_string.h"

#include <limits.h>

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

typedef uint32_t ItemID;

template <class T>
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

template <class BoundingBox>
struct ItemPairs {
    struct Link {
        void set(ItemID new_item_id, void* new_userdata) {
            item_id  = new_item_id;
            userdata = new_userdata;
        }

        ItemID item_id;
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

    void add_pair_to(ItemID item_id, void* p_userdata) {
        Link temp;
        temp.set(item_id, p_userdata);

        extended_pairs.push_back(temp);
        num_pairs++;
    }

    uint32_t find_pair_to(ItemID item_id) const {
        for (int n = 0; n < num_pairs; n++) {
            if (extended_pairs[n].item_id == item_id) {
                return n;
            }
        }
        return -1;
    }

    bool contains_pair_to(ItemID item_id) const {
        return find_pair_to(item_id) != INVALID;
    }

    // Return success
    void* remove_pair_to(ItemID item_id) {
        void* userdata = nullptr;

        for (int n = 0; n < num_pairs; n++) {
            if (extended_pairs[n].item_id == item_id) {
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

struct ItemRef {
    uint32_t node_id;
    uint32_t item_id;

    bool is_active() const {
        return node_id != INACTIVE;
    }

    void set_inactive() {
        node_id = INACTIVE;
        item_id = INACTIVE;
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

template <
    class T,
    int MAX_ITEMS,
    class BoundingBox = ::AABB,
    class Point       = Vector3>
class Tree {
public:
    // Instead of using a linked list, we use item references for quick lookups.
    PooledList<ItemRef, true> _refs;
    PooledList<ItemExtra<T>, true> _extra;
    PooledList<ItemPairs<BoundingBox>> _pairs;

    PooledList<Node<BoundingBox, Point>, true> _nodes;
    PooledList<Leaf<MAX_ITEMS, BoundingBox, Point>, true> _leaves;

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
    Tree(bool use_pairs = true);

private:
    const bool use_pairs;

    bool node_add_child(uint32_t p_node_id, uint32_t p_child_node_id);
    void node_replace_child(
        uint32_t p_parent_id,
        uint32_t p_old_child_id,
        uint32_t p_new_child_id
    );
    void node_remove_child(
        uint32_t p_parent_id,
        uint32_t p_child_id,
        uint32_t p_tree_id,
        bool p_prevent_sibling = false
    );
    // A node can either be a node, or a node AND a leaf combo.
    // Both must be deleted to prevent a leak.
    void node_free_node_and_leaf(uint32_t p_node_id);
    void change_root_node(uint32_t p_new_root_id, uint32_t p_tree_id);
    void node_make_leaf(uint32_t p_node_id);
    void node_remove_item(
        uint32_t p_ref_id,
        uint32_t p_tree_id,
        AABB<BoundingBox, Point>* r_old_aabb = nullptr
    );
    // Return true if parent tree needs a refit.
    // The node AABB is calculated within this routine.
    bool _node_add_item(
        uint32_t p_node_id,
        uint32_t p_ref_id,
        const AABB<BoundingBox, Point>& p_aabb
    );
    uint32_t _node_create_another_child(
        uint32_t p_node_id,
        const AABB<BoundingBox, Point>& p_aabb
    );
    void _cull_translate_hits(CullParameters<T, BoundingBox, Point>& p_params);

public:
    int cull_convex(
        CullParameters<T, BoundingBox, Point>& r_params,
        bool p_translate_hits = true
    );
    int cull_segment(
        CullParameters<T, BoundingBox, Point>& r_params,
        bool p_translate_hits = true
    );
    int cull_point(
        CullParameters<T, BoundingBox, Point>& r_params,
        bool p_translate_hits = true
    );
    int cull_aabb(
        CullParameters<T, BoundingBox, Point>& r_params,
        bool p_translate_hits = true
    );
    bool _cull_hits_full(const CullParameters<T, BoundingBox, Point>& p_params);
    bool _cull_pairing_mask_test_hit(
        uint32_t p_maskA,
        uint32_t p_typeA,
        uint32_t p_maskB,
        uint32_t p_typeB
    ) const;
    void _cull_hit(
        uint32_t p_ref_id,
        CullParameters<T, BoundingBox, Point>& p_params
    );
    bool _cull_segment_iterative(
        uint32_t p_node_id,
        CullParameters<T, BoundingBox, Point>& r_params
    );
    bool _cull_point_iterative(
        uint32_t p_node_id,
        CullParameters<T, BoundingBox, Point>& r_params
    );
    bool _cull_aabb_iterative(
        uint32_t p_node_id,
        CullParameters<T, BoundingBox, Point>& r_params,
        bool p_fully_within = false
    );
    // Returns full with results.
    bool _cull_convex_iterative(
        uint32_t p_node_id,
        CullParameters<T, BoundingBox, Point>& r_params,
        bool p_fully_within = false
    );

#ifdef BVH_VERBOSE
    void _debug_recursive_print_tree(int p_tree_id) const;
    String _debug_aabb_to_string(const AABB<BoundingBox, Point>& aabb) const;
    void _debug_recursive_print_tree_node(uint32_t p_node_id, int depth = 0)
        const;
#endif

    void _integrity_check_all();
    void _integrity_check_up(uint32_t p_node_id);
    void _integrity_check_down(uint32_t p_node_id);
    // For slow incremental optimization, we periodically remove each item from
    // the tree and reinsert it to give it a chance to find a better position.
    void _logic_item_remove_and_reinsert(uint32_t p_ref_id);
    AABB<BoundingBox, Point> _logic_bvh_aabb_merge(
        const AABB<BoundingBox, Point>& a,
        const AABB<BoundingBox, Point>& b
    );
    int32_t _logic_balance(int32_t iA, uint32_t p_tree_id);
    // Either choose an existing node to add item to, or create a new node.
    uint32_t _logic_choose_item_add_node(
        uint32_t p_node_id,
        const AABB<BoundingBox, Point>& p_aabb
    );
    int _handle_get_tree_id(ItemID item_id) const;
    void _handle_sort(ItemID& p_ha, ItemID& p_hb) const;

private:
    void create_root_node(int p_tree);
    bool node_is_leaf_full(Node<BoundingBox, Point>& node) const;

public:
    Leaf<MAX_ITEMS, BoundingBox, Point>& _node_get_leaf(
        Node<BoundingBox, Point>& node
    );
    const Leaf<MAX_ITEMS, BoundingBox, Point>& _node_get_leaf(
        const Node<BoundingBox, Point>& node
    ) const;
    ItemID item_add(
        T* p_userdata,
        bool p_active,
        const BoundingBox& p_aabb,
        int32_t p_subindex,
        bool p_pairable,
        uint32_t p_pairable_type,
        uint32_t p_pairable_mask,
        bool p_invisible = false
    );
    void _debug_print_refs();
    // Return false if noop.
    bool item_move(ItemID item_id, const BoundingBox& p_aabb);
    void item_remove(ItemID item_id);
    bool item_activate(ItemID item_id, const BoundingBox& p_aabb);
    bool item_deactivate(ItemID item_id);
    bool item_get_active(ItemID item_id) const;
    // During collision testing, we set the from item's mask and pairable.
    void item_fill_cullparams(
        ItemID item_id,
        CullParameters<T, BoundingBox, Point>& r_params
    ) const;
    bool item_is_pairable(const ItemID& item_id);
    void item_get_bvh_aabb(
        const ItemID& item_id,
        AABB<BoundingBox, Point>& r_bvh_aabb
    );
    bool item_set_pairable(
        const ItemID& item_id,
        bool p_pairable,
        uint32_t p_pairable_type,
        uint32_t p_pairable_mask
    );
    void incremental_optimize();
    void update();
    void params_set_pairing_expansion(real_t p_value);
    // Also checks for special case of shrinkage.
    bool expanded_aabb_encloses_not_shrink(
        const BoundingBox& p_expanded_aabb,
        const BoundingBox& p_aabb
    ) const;
    void _debug_node_verify_bound(uint32_t p_node_id);
    void node_update_aabb(Node<BoundingBox, Point>& node);
    void refit_all(int p_tree_id);
    void refit_upward(uint32_t p_node_id);
    void refit_upward_and_balance(uint32_t p_node_id, uint32_t p_tree_id);
    void refit_downward(uint32_t p_node_id);
    // Go down to the leaves, then refit upward.
    void refit_branch(uint32_t p_node_id);
    void _split_inform_references(uint32_t p_node_id);
    void _split_leaf_sort_groups_simple(
        int& num_a,
        int& num_b,
        uint16_t* group_a,
        uint16_t* group_b,
        const AABB<BoundingBox, Point>* temp_bounds,
        const AABB<BoundingBox, Point> full_bound
    );
    void _split_leaf_sort_groups(
        int& num_a,
        int& num_b,
        uint16_t* group_a,
        uint16_t* group_b,
        const AABB<BoundingBox, Point>* temp_bounds
    );
    uint32_t split_leaf(
        uint32_t p_node_id,
        const AABB<BoundingBox, Point>& p_added_item_aabb
    );
    // AABB is the new inserted node.
    uint32_t split_leaf_complex(
        uint32_t p_node_id,
        const AABB<BoundingBox, Point>& p_added_item_aabb
    );
};

// Definitions

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
Tree<T, MAX_ITEMS, BoundingBox, Point>::Tree(bool use_pairs) :
    use_pairs(use_pairs) {
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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
bool Tree<T, MAX_ITEMS, BoundingBox, Point>::node_add_child(
    uint32_t p_node_id,
    uint32_t p_child_node_id
) {
    Node<BoundingBox, Point>& node = _nodes[p_node_id];
    if (node.is_full_of_children()) {
        return false;
    }

    node.children[node.num_children]  = p_child_node_id;
    node.num_children                += 1;

    // Back link from the child to the parent.
    Node<BoundingBox, Point>& node_child = _nodes[p_child_node_id];
    node_child.parent_id                 = p_node_id;

    return true;
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::node_replace_child(
    uint32_t p_parent_id,
    uint32_t p_old_child_id,
    uint32_t p_new_child_id
) {
    Node<BoundingBox, Point>& parent = _nodes[p_parent_id];
    BVH_ASSERT(!parent.is_leaf());

    int child_num = parent.find_child(p_old_child_id);
    BVH_ASSERT(child_num != INVALID);
    parent.children[child_num] = p_new_child_id;

    Node<BoundingBox, Point>& new_child = _nodes[p_new_child_id];
    new_child.parent_id                 = p_parent_id;
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::node_remove_child(
    uint32_t p_parent_id,
    uint32_t p_child_id,
    uint32_t p_tree_id,
    bool p_prevent_sibling
) {
    Node<BoundingBox, Point>& parent = _nodes[p_parent_id];
    BVH_ASSERT(!parent.is_leaf());

    int child_num = parent.find_child(p_child_id);
    BVH_ASSERT(child_num != INVALID);

    parent.remove_child_internal(child_num);

    // Currently, there is no need to keep back references for children.

    // Always a node id, as node is never a leaf.
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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::node_free_node_and_leaf(
    uint32_t p_node_id
) {
    Node<BoundingBox, Point>& node = _nodes[p_node_id];
    if (node.is_leaf()) {
        int leaf_id = node.get_leaf_id();
        _leaves.free(leaf_id);
    }

    _nodes.free(p_node_id);
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::change_root_node(
    uint32_t p_new_root_id,
    uint32_t p_tree_id
) {
    _root_node_id[p_tree_id]       = p_new_root_id;
    Node<BoundingBox, Point>& root = _nodes[p_new_root_id];

    // A root node has no parent.
    root.parent_id = INVALID;
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::node_make_leaf(uint32_t p_node_id
) {
    uint32_t child_leaf_id;
    Leaf<MAX_ITEMS, BoundingBox, Point>* child_leaf =
        _leaves.request(child_leaf_id);
    child_leaf->clear();

    BVH_ASSERT(child_leaf_id != 0);

    Node<BoundingBox, Point>& node = _nodes[p_node_id];
    node.neg_leaf_id               = -(int)child_leaf_id;
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::node_remove_item(
    uint32_t p_ref_id,
    uint32_t p_tree_id,
    AABB<BoundingBox, Point>* r_old_aabb
) {
    ItemRef& ref           = _refs[p_ref_id];
    uint32_t owner_node_id = ref.node_id;

    // TODO: Check if this is needed.
    if (owner_node_id == INVALID) {
        return;
    }

    Node<BoundingBox, Point>& node = _nodes[owner_node_id];
    CRASH_COND(!node.is_leaf());

    Leaf<MAX_ITEMS, BoundingBox, Point>& leaf = _node_get_leaf(node);

    // If the AABB is not determining the corner size, don't refit,
    // because merging AABBs takes a lot of time.
    const AABB<BoundingBox, Point>& old_aabb = leaf.get_aabb(ref.item_id);

    // Shrink a little to prevent using corner AABBs.
    // To miss the corners, first we shrink by node_expansion, which is
    // added to the overall bound of the leaf.
    // Second, we shrink by an epsilon, to miss the very corner AABBs,
    // which are important in determining the bound.
    // AABBs within this can be removed and not affect the overall bound.
    AABB<BoundingBox, Point> node_bound = node.aabb;
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
        if (node.parent_id != INVALID) {
            // Check if the root noode only has one child.
            uint32_t parent_id = node.parent_id;

            node_remove_child(parent_id, owner_node_id, p_tree_id);
            refit_upward(parent_id);

            // Recycle the node.
            node_free_node_and_leaf(owner_node_id);
        }
    }

    ref.node_id = INVALID;
    ref.item_id = INVALID;
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
bool Tree<T, MAX_ITEMS, BoundingBox, Point>::_node_add_item(
    uint32_t p_node_id,
    uint32_t p_ref_id,
    const AABB<BoundingBox, Point>& p_aabb
) {
    ItemRef& ref = _refs[p_ref_id];
    ref.node_id  = p_node_id;

    Node<BoundingBox, Point>& node = _nodes[p_node_id];
    BVH_ASSERT(node.is_leaf());
    Leaf<MAX_ITEMS, BoundingBox, Point>& leaf = _node_get_leaf(node);

    // We only need to refit if the added item is changing the node's AABB.
    bool needs_refit = true;

    AABB<BoundingBox, Point> expanded = p_aabb;
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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
uint32_t Tree<T, MAX_ITEMS, BoundingBox, Point>::_node_create_another_child(
    uint32_t p_node_id,
    const AABB<BoundingBox, Point>& p_aabb
) {
    uint32_t child_node_id;
    Node<BoundingBox, Point>* child_node = _nodes.request(child_node_id);
    child_node->clear();

    // TODO: Check if necessary.
    child_node->aabb = p_aabb;

    node_add_child(p_node_id, child_node_id);

    return child_node_id;
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::_cull_translate_hits(
    CullParameters<T, BoundingBox, Point>& p_params
) {
    int num_hits = _cull_hits.size();
    int left     = p_params.result_max - p_params.result_count_overall;

    if (num_hits > left) {
        num_hits = left;
    }

    int out_n = p_params.result_count_overall;

    for (int n = 0; n < num_hits; n++) {
        uint32_t ref_id = _cull_hits[n];

        const ItemExtra<T>& ex       = _extra[ref_id];
        p_params.result_array[out_n] = ex.userdata;

        if (p_params.subindex_array) {
            p_params.subindex_array[out_n] = ex.subindex;
        }

        out_n++;
    }

    p_params.result_count          = num_hits;
    p_params.result_count_overall += num_hits;
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
int Tree<T, MAX_ITEMS, BoundingBox, Point>::cull_convex(
    CullParameters<T, BoundingBox, Point>& r_params,
    bool p_translate_hits
) {
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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
int Tree<T, MAX_ITEMS, BoundingBox, Point>::cull_segment(
    CullParameters<T, BoundingBox, Point>& r_params,
    bool p_translate_hits
) {
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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
int Tree<T, MAX_ITEMS, BoundingBox, Point>::cull_point(
    CullParameters<T, BoundingBox, Point>& r_params,
    bool p_translate_hits
) {
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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
int Tree<T, MAX_ITEMS, BoundingBox, Point>::cull_aabb(
    CullParameters<T, BoundingBox, Point>& r_params,
    bool p_translate_hits
) {
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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
bool Tree<T, MAX_ITEMS, BoundingBox, Point>::_cull_hits_full(
    const CullParameters<T, BoundingBox, Point>& p_params
) {
    // Instead of checking every hit, do a lazy check for this condition.
    // It isn't a problem if we write too many _cull_hits, because
    // we only the result_max amount will be translated and outputted.
    // We stop the cull checks after the maximum has been reached.
    return (int)_cull_hits.size() >= p_params.result_max;
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
bool Tree<T, MAX_ITEMS, BoundingBox, Point>::_cull_pairing_mask_test_hit(
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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::_cull_hit(
    uint32_t p_ref_id,
    CullParameters<T, BoundingBox, Point>& p_params
) {
    // It would be more efficient to do before plane checks,
    // but done here to ease gettting started.
    if (use_pairs) {
        const ItemExtra<T>& ex = _extra[p_ref_id];

        if (!_cull_pairing_mask_test_hit(
                p_params.mask,
                p_params.pairable_type,
                ex.pairable_mask,
                ex.pairable_type
            )) {
            return;
        }
    }

    _cull_hits.push_back(p_ref_id);
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
bool Tree<T, MAX_ITEMS, BoundingBox, Point>::_cull_segment_iterative(
    uint32_t p_node_id,
    CullParameters<T, BoundingBox, Point>& r_params
) {
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
        Node<BoundingBox, Point>& node = _nodes[csp.node_id];

        if (node.is_leaf()) {
            // Lazy check for hits full condition.
            if (_cull_hits_full(r_params)) {
                return false;
            }

            Leaf<MAX_ITEMS, BoundingBox, Point>& leaf = _node_get_leaf(node);

            // Test the children individually.
            for (int n = 0; n < leaf.num_items; n++) {
                const AABB<BoundingBox, Point>& aabb = leaf.get_aabb(n);

                if (aabb.intersects_segment(r_params.segment)) {
                    uint32_t child_id = leaf.get_item_ref_id(n);

                    // Register a hit.
                    _cull_hit(child_id, r_params);
                }
            }
        } else {
            // Test the children individually.
            for (int n = 0; n < node.num_children; n++) {
                uint32_t child_id = node.children[n];
                const AABB<BoundingBox, Point>& child_bvh_aabb =
                    _nodes[child_id].aabb;

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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
bool Tree<T, MAX_ITEMS, BoundingBox, Point>::_cull_point_iterative(
    uint32_t p_node_id,
    CullParameters<T, BoundingBox, Point>& r_params
) {
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
        Node<BoundingBox, Point>& node = _nodes[cpp.node_id];
        // Check for hit.
        if (!node.aabb.intersects_point(r_params.point)) {
            continue;
        }

        if (node.is_leaf()) {
            // Lazy check for hits full condition.
            if (_cull_hits_full(r_params)) {
                return false;
            }

            Leaf<MAX_ITEMS, BoundingBox, Point>& leaf = _node_get_leaf(node);

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
            for (int n = 0; n < node.num_children; n++) {
                uint32_t child_id = node.children[n];

                // Add to the stack.
                CullPointParams* child = ii.request();
                child->node_id         = child_id;
            }
        }

    } // Loop while more nodes to pop.

    // Return true if the results are not full.
    return true;
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
bool Tree<T, MAX_ITEMS, BoundingBox, Point>::_cull_aabb_iterative(
    uint32_t p_node_id,
    CullParameters<T, BoundingBox, Point>& r_params,
    bool p_fully_within
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
        Node<BoundingBox, Point>& node = _nodes[cap.node_id];

        if (node.is_leaf()) {
            // Lazy check for hits full condition.
            if (_cull_hits_full(r_params)) {
                return false;
            }

            Leaf<MAX_ITEMS, BoundingBox, Point>& leaf = _node_get_leaf(node);

            // If fully within, we add all items that pass mask checks.
            if (cap.fully_within) {
                for (int n = 0; n < leaf.num_items; n++) {
                    uint32_t child_id = leaf.get_item_ref_id(n);

                    // Register a hit.
                    _cull_hit(child_id, r_params);
                }
            } else {
                for (int n = 0; n < leaf.num_items; n++) {
                    const AABB<BoundingBox, Point>& aabb = leaf.get_aabb(n);

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
                for (int n = 0; n < node.num_children; n++) {
                    uint32_t child_id = node.children[n];
                    const AABB<BoundingBox, Point>& child_bvh_aabb =
                        _nodes[child_id].aabb;

                    if (child_bvh_aabb.intersects(r_params.bvh_aabb)) {
                        // Is the node totally within the aabb?
                        bool fully_within =
                            r_params.bvh_aabb.is_other_within(child_bvh_aabb);

                        // Add to the stack.
                        CullAABBParams* child = ii.request();

                        // Should always return a valid child.
                        child->node_id      = child_id;
                        child->fully_within = fully_within;
                    }
                }
            } else {
                for (int n = 0; n < node.num_children; n++) {
                    uint32_t child_id = node.children[n];

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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
bool Tree<T, MAX_ITEMS, BoundingBox, Point>::_cull_convex_iterative(
    uint32_t p_node_id,
    CullParameters<T, BoundingBox, Point>& r_params,
    bool p_fully_within
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
        const Node<BoundingBox, Point>& node = _nodes[ccp.node_id];

        if (!ccp.fully_within) {
            IntersectResult result = node.aabb.intersects_convex(r_params.hull);

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

        if (node.is_leaf()) {
            // Lazy check for hits full condition.
            if (_cull_hits_full(r_params)) {
                return false;
            }

            const Leaf<MAX_ITEMS, BoundingBox, Point>& leaf =
                _node_get_leaf(node);

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
                uint32_t num_planes =
                    node.aabb.find_cutting_planes(r_params.hull, plane_ids);
                BVH_ASSERT(num_planes <= max_planes);

// #define BVH_CONVEX_CULL_OPTIMIZED_RIGOR_CHECK
#ifdef BVH_CONVEX_CULL_OPTIMIZED_RIGOR_CHECK
                uint32_t results[MAX_ITEMS];
                uint32_t num_results = 0;
#endif

                // Test children individually.
                for (int n = 0; n < leaf.num_items; n++) {
                    const AABB<BoundingBox, Point>& aabb = leaf.get_aabb(n);

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
                    const AABB<BoundingBox, Point>& aabb = leaf.get_aabb(n);

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
                    const AABB<BoundingBox, Point>& aabb = leaf.get_aabb(n);

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
            for (int n = 0; n < node.num_children; n++) {
                uint32_t child_id = node.children[n];

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

#ifdef BVH_VERBOSE
template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::_debug_recursive_print_tree(
    int p_tree_id
) const {
    if (_root_node_id[p_tree_id] != INVALID) {
        _debug_recursive_print_tree_node(_root_node_id[p_tree_id]);
    }
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
String Tree<T, MAX_ITEMS, BoundingBox, Point>::_debug_aabb_to_string(
    const AABB<BoundingBox, Point>& aabb
) const {
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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::_debug_recursive_print_tree_node(
    uint32_t p_node_id,
    int depth = 0
) const {
    const Node<BoundingBox, Point>& node = _nodes[p_node_id];

    String sz = "";
    for (int n = 0; n < depth; n++) {
        sz += "\t";
    }
    sz += itos(p_node_id);

    if (node.is_leaf()) {
        sz += " L";
        sz += itos(node.height) + " ";
        const Leaf<MAX_ITEMS, BoundingBox, Point>& leaf = _node_get_leaf(node);

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
        sz += itos(node.height) + " ";
    }

    sz += _debug_aabb_to_string(node.aabb);
    print_line(sz);

    if (!node.is_leaf()) {
        for (int n = 0; n < node.num_children; n++) {
            _debug_recursive_print_tree_node(node.children[n], depth + 1);
        }
    }
}
#endif

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::_integrity_check_all() {
#ifdef BVH_INTEGRITY_CHECKS
    for (int n = 0; n < NUM_TREES; n++) {
        uint32_t root = _root_node_id[n];
        if (root != INVALID) {
            _integrity_check_down(root);
        }
    }
#endif
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::_integrity_check_up(
    uint32_t p_node_id
) {
    Node<BoundingBox, Point>& node = _nodes[p_node_id];

    AABB<BoundingBox, Point> bvh_aabb = node.aabb;
    node_update_aabb(node);

    AABB<BoundingBox, Point> bvh_aabb2 = node.aabb;
    bvh_aabb2.expand(-_node_expansion);

    CRASH_COND(!bvh_aabb.is_other_within(bvh_aabb2));
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::_integrity_check_down(
    uint32_t p_node_id
) {
    const Node<BoundingBox, Point>& node = _nodes[p_node_id];

    if (node.is_leaf()) {
        _integrity_check_up(p_node_id);
    } else {
        CRASH_COND(node.num_children != 2);

        for (int n = 0; n < node.num_children; n++) {
            uint32_t child_id = node.children[n];

            // Check that the children's parent pointers are correct.
            Node<BoundingBox, Point>& child = _nodes[child_id];
            CRASH_COND(child.parent_id != p_node_id);

            _integrity_check_down(child_id);
        }
    }
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::_logic_item_remove_and_reinsert(
    uint32_t p_ref_id
) {
    ItemRef& ref = _refs[p_ref_id];

    if (!ref.is_active()) {
        return;
    }

    if (ref.item_id == INVALID) {
        return;
    }

    BVH_ASSERT(ref.node_id != INVALID);

    uint32_t tree_id = _handle_get_tree_id(p_ref_id);

    // Remove and reinsert.
    AABB<BoundingBox, Point> bvh_aabb;
    node_remove_item(p_ref_id, tree_id, &bvh_aabb);

    // We must choose where to add it to the tree.
    ref.node_id = _logic_choose_item_add_node(_root_node_id[tree_id], bvh_aabb);
    _node_add_item(ref.node_id, p_ref_id, bvh_aabb);

    refit_upward_and_balance(ref.node_id, tree_id);
}

// _logic_bvh_aabb_merge and _logic balance are based on the 'Balance'
// function from Randy Gaul's qu3e: https://github.com/RandyGaul/qu3e
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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
AABB<BoundingBox, Point> Tree<T, MAX_ITEMS, BoundingBox, Point>::
    _logic_bvh_aabb_merge(
        const AABB<BoundingBox, Point>& a,
        const AABB<BoundingBox, Point>& b
    ) {
    AABB<BoundingBox, Point> c = a;
    c.merge(b);
    return c;
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
int32_t Tree<T, MAX_ITEMS, BoundingBox, Point>::_logic_balance(
    int32_t iA,
    uint32_t p_tree_id
) {
    // Uncomment to bypass balance.
    // return iA;

    Node<BoundingBox, Point>* A = &_nodes[iA];

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
    int32_t iB                  = A->children[0];
    int32_t iC                  = A->children[1];
    Node<BoundingBox, Point>* B = &_nodes[iB];
    Node<BoundingBox, Point>* C = &_nodes[iC];

    int32_t balance = C->height - B->height;

    // If C is higher, promote C.
    if (balance > 1) {
        int32_t iF                  = C->children[0];
        int32_t iG                  = C->children[1];
        Node<BoundingBox, Point>* F = &_nodes[iF];
        Node<BoundingBox, Point>* G = &_nodes[iG];

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
        int32_t iD                  = B->children[0];
        int32_t iE                  = B->children[1];
        Node<BoundingBox, Point>* D = &_nodes[iD];
        Node<BoundingBox, Point>* E = &_nodes[iE];

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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
uint32_t Tree<T, MAX_ITEMS, BoundingBox, Point>::_logic_choose_item_add_node(
    uint32_t p_node_id,
    const AABB<BoundingBox, Point>& p_aabb
) {
    while (true) {
        BVH_ASSERT(p_node_id != INVALID);
        Node<BoundingBox, Point>& node = _nodes[p_node_id];

        if (node.is_leaf()) {
            // If a leaf, and not full, use it.
            if (!node_is_leaf_full(node)) {
                return p_node_id;
            }
            // Else split the leaf, and use one of the children.
            return split_leaf(p_node_id, p_aabb);
        }

        // TODO: This should not happen.
        // It is not serious, but it would be nice to prevent.
        // May only happen with the root node.
        if (node.num_children == 1) {
            WARN_PRINT_ONCE(
                "BVH::recursive_choose_item_add_node, node with 1 child, "
                "recovering"
            );
            p_node_id = node.children[0];
        } else {
            BVH_ASSERT(node.num_children == 2);
            Node<BoundingBox, Point>& childA = _nodes[node.children[0]];
            Node<BoundingBox, Point>& childB = _nodes[node.children[1]];
            int which = p_aabb.select_by_proximity(childA.aabb, childB.aabb);

            p_node_id = node.children[which];
        }
    }
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
int Tree<T, MAX_ITEMS, BoundingBox, Point>::_handle_get_tree_id(ItemID item_id
) const {
    if (use_pairs) {
        int tree = 0;
        if (_extra[item_id].pairable) {
            tree = 1;
        }
        return tree;
    }
    return 0;
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::_handle_sort(
    ItemID& p_ha,
    ItemID& p_hb
) const {
    if (p_ha > p_hb) {
        ItemID temp = p_hb;
        p_hb        = p_ha;
        p_ha        = temp;
    }
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::create_root_node(int p_tree) {
    // If there is no root node, create one.
    if (_root_node_id[p_tree] == INVALID) {
        uint32_t root_node_id;
        Node<BoundingBox, Point>* node = _nodes.request(root_node_id);
        node->clear();
        _root_node_id[p_tree] = root_node_id;

        // Make the root node a leaf.
        uint32_t leaf_id;
        Leaf<MAX_ITEMS, BoundingBox, Point>* leaf = _leaves.request(leaf_id);
        leaf->clear();
        node->neg_leaf_id = -(int)leaf_id;
    }
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
bool Tree<T, MAX_ITEMS, BoundingBox, Point>::node_is_leaf_full(
    Node<BoundingBox, Point>& node
) const {
    const Leaf<MAX_ITEMS, BoundingBox, Point>& leaf = _node_get_leaf(node);
    return leaf.is_full();
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
Leaf<MAX_ITEMS, BoundingBox, Point>& Tree<T, MAX_ITEMS, BoundingBox, Point>::
    _node_get_leaf(Node<BoundingBox, Point>& node) {
    BVH_ASSERT(node.is_leaf());
    return _leaves[node.get_leaf_id()];
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
const Leaf<MAX_ITEMS, BoundingBox, Point>& Tree<
    T,
    MAX_ITEMS,
    BoundingBox,
    Point>::_node_get_leaf(const Node<BoundingBox, Point>& node) const {
    BVH_ASSERT(node.is_leaf());
    return _leaves[node.get_leaf_id()];
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
ItemID Tree<T, MAX_ITEMS, BoundingBox, Point>::item_add(
    T* p_userdata,
    bool p_active,
    const BoundingBox& p_aabb,
    int32_t p_subindex,
    bool p_pairable,
    uint32_t p_pairable_type,
    uint32_t p_pairable_mask,
    bool p_invisible
) {
#ifdef BVH_VERBOSE_TREE
    VERBOSE_PRINT("\nitem_add BEFORE");
    _debug_recursive_print_tree(p_tree_id);
    VERBOSE_PRINT("\n");
#endif

    AABB<BoundingBox, Point> bvh_aabb;
    bvh_aabb.from(p_aabb);

    // Note: We do not expand the AABB for the first create even if
    // leaf expansion is switched on, because:
    // 1. We don't know if this object will move in future.
    //    If it does, a non-expanded bound would be better.
    // 2. We don't know how many objects will be paired.
    //    This is used to modify the expansion margin.

    uint32_t ref_id;
    // This should never fail.
    ItemRef* ref = _refs.request(ref_id);

    // The extra data should be a parallel list to the references.
    uint32_t extra_id;
    ItemExtra<T>* extra = _extra.request(extra_id);
    BVH_ASSERT(extra_id == ref_id);

    // Pairs info.
    if (use_pairs) {
        uint32_t pairs_id;
        ItemPairs<BoundingBox>* pairs = _pairs.request(pairs_id);
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

    // Assign to ItemID to return.
    ItemID item_id = ref_id;

    uint32_t tree_id = 0;
    if (p_pairable) {
        tree_id = 1;
    }

    create_root_node(tree_id);

    // We must choose where to add it to the tree.
    if (p_active) {
        ref->node_id =
            _logic_choose_item_add_node(_root_node_id[tree_id], bvh_aabb);

        bool refit = _node_add_item(ref->node_id, ref_id, bvh_aabb);

        if (refit) {
            // Only need to refit from the parent.
            const Node<BoundingBox, Point>& add_node = _nodes[ref->node_id];
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

    return item_id;
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::_debug_print_refs() {
#ifdef BVH_VERBOSE_TREE
    print_line("refs.....");
    for (int n = 0; n < _refs.size(); n++) {
        const ItemRef& ref = _refs[n];
        print_line(
            "node_id " + itos(ref.node_id) + ", item_id " + itos(ref.item_id)
        );
    }

#endif
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
bool Tree<T, MAX_ITEMS, BoundingBox, Point>::item_move(
    ItemID item_id,
    const BoundingBox& p_aabb
) {
    ItemRef& ref = _refs[item_id];
    if (!ref.is_active()) {
        return false;
    }

    AABB<BoundingBox, Point> bvh_aabb;
    bvh_aabb.from(p_aabb);

#ifdef BVH_EXPAND_LEAF_AABBS
    if (use_pairs) {
        // Scale the pairing expansion by the number of pairs.
        bvh_aabb.expand(
            _pairs[item_id].scale_expansion_margin(_pairing_expansion)
        );
    } else {
        bvh_aabb.expand(_pairing_expansion);
    }
#endif

    BVH_ASSERT(ref.node_id != INVALID);
    Node<BoundingBox, Point>& node = _nodes[ref.node_id];

    // Does it fit within the current leaf AABB?
    if (node.aabb.is_other_within(bvh_aabb)) {
        // It has not moved enough to require a refit: Do nothing.
        // However, we update the exact AABB in the leaf, as this will be
        // needed for accurate collision detection.
        Leaf<MAX_ITEMS, BoundingBox, Point>& leaf = _node_get_leaf(node);

        AABB<BoundingBox, Point>& leaf_bvh_aabb = leaf.get_aabb(ref.item_id);

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
            "item_move " + itos(item_id.id())
            + "(within node aabb) : " + _debug_aabb_to_string(abb)
        );
#endif

        leaf_bvh_aabb = bvh_aabb;
        _integrity_check_all();

        return true;
    }

#ifdef BVH_VERBOSE_MOVES
    print_line(
        "item_move " + itos(item_id.id())
        + "(outside node aabb) : " + _debug_aabb_to_string(abb)
    );
#endif

    uint32_t tree_id = _handle_get_tree_id(item_id);

    // Remove and reinsert.
    node_remove_item(item_id, tree_id);

    // We must choose where to add it to the tree.
    ref.node_id = _logic_choose_item_add_node(_root_node_id[tree_id], bvh_aabb);

    // Add it to the tree.
    bool needs_refit = _node_add_item(ref.node_id, item_id, bvh_aabb);

    // Only need to refit from the parent.
    if (needs_refit) {
        const Node<BoundingBox, Point>& add_node = _nodes[ref.node_id];
        if (add_node.parent_id != INVALID) {
            // TODO: We don't need to rebalance all the time.
            refit_upward(add_node.parent_id);
        }
    }

    return true;
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::item_remove(ItemID item_id) {
    uint32_t tree_id = _handle_get_tree_id(item_id);

    VERBOSE_PRINT("item_remove [" + itos(item_id) + "] ");

    // Remove the active reference from the list for incremental optimize.
    // This list must be kept in sync with the references as they are added
    // or removed.
    uint32_t active_ref_id     = _extra[item_id].active_ref_id;
    uint32_t ref_id_moved_back = _active_refs[_active_refs.size() - 1];

    // Swap back and decrement for fast, unordered remove.
    _active_refs[active_ref_id] = ref_id_moved_back;
    _active_refs.resize(_active_refs.size() - 1);

    // Keep the moved active reference up to date.
    _extra[ref_id_moved_back].active_ref_id = active_ref_id;

    // If active, remove the item from the node.
    if (_refs[item_id].is_active()) {
        node_remove_item(item_id, tree_id);
    }

    // Remove the item reference.
    _refs.free(item_id);
    _extra.free(item_id);
    if (use_pairs) {
        _pairs.free(item_id);
    }

#ifdef BVH_VERBOSE_TREE
    _debug_recursive_print_tree(tree_id);
#endif
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
bool Tree<T, MAX_ITEMS, BoundingBox, Point>::item_activate(
    ItemID item_id,
    const BoundingBox& p_aabb
) {
    ItemRef& ref = _refs[item_id];
    if (ref.is_active()) {
        return false;
    }

    // Add to the tree.
    AABB<BoundingBox, Point> bvh_aabb;
    bvh_aabb.from(p_aabb);

    uint32_t tree_id = _handle_get_tree_id(item_id);

    // We must choose where to add it to the tree.
    ref.node_id = _logic_choose_item_add_node(_root_node_id[tree_id], bvh_aabb);
    _node_add_item(ref.node_id, item_id, bvh_aabb);

    refit_upward_and_balance(ref.node_id, tree_id);

    return true;
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
bool Tree<T, MAX_ITEMS, BoundingBox, Point>::item_deactivate(ItemID item_id) {
    ItemRef& ref = _refs[item_id];
    if (!ref.is_active()) {
        return false;
    }

    uint32_t tree_id = _handle_get_tree_id(item_id);

    // Remove it from the tree.
    AABB<BoundingBox, Point> bvh_aabb;
    node_remove_item(item_id, tree_id, &bvh_aabb);

    // Mark as inactive.
    ref.set_inactive();
    return true;
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
bool Tree<T, MAX_ITEMS, BoundingBox, Point>::item_get_active(ItemID item_id
) const {
    const ItemRef& ref = _refs[item_id];
    return ref.is_active();
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::item_fill_cullparams(
    ItemID item_id,
    CullParameters<T, BoundingBox, Point>& r_params
) const {
    const ItemExtra<T>& extra = _extra[item_id];

    // Only test from pairable items.
    r_params.test_pairable_only = extra.pairable == 0;

    // Take the mask of the from item into account.
    r_params.mask          = extra.pairable_mask;
    r_params.pairable_type = extra.pairable_type;
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
bool Tree<T, MAX_ITEMS, BoundingBox, Point>::item_is_pairable(
    const ItemID& item_id
) {
    const ItemExtra<T>& extra = _extra[item_id];
    return extra.pairable != 0;
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::item_get_bvh_aabb(
    const ItemID& item_id,
    AABB<BoundingBox, Point>& r_bvh_aabb
) {
    const ItemRef& ref = _refs[item_id];

    Node<BoundingBox, Point>& node            = _nodes[ref.node_id];
    Leaf<MAX_ITEMS, BoundingBox, Point>& leaf = _node_get_leaf(node);

    r_bvh_aabb = leaf.get_aabb(ref.item_id);
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
bool Tree<T, MAX_ITEMS, BoundingBox, Point>::item_set_pairable(
    const ItemID& item_id,
    bool p_pairable,
    uint32_t p_pairable_type,
    uint32_t p_pairable_mask
) {
    ItemExtra<T>& ex = _extra[item_id];
    ItemRef& ref     = _refs[item_id];

    bool active           = ref.is_active();
    bool pairable_changed = (ex.pairable != 0) != p_pairable;
    bool state_changed    = pairable_changed
                      || (ex.pairable_type != p_pairable_type)
                      || (ex.pairable_mask != p_pairable_mask);

    ex.pairable_type = p_pairable_type;
    ex.pairable_mask = p_pairable_mask;

    if (active && pairable_changed) {
        // Record AABB
        Node<BoundingBox, Point>& node            = _nodes[ref.node_id];
        Leaf<MAX_ITEMS, BoundingBox, Point>& leaf = _node_get_leaf(node);
        AABB<BoundingBox, Point> bvh_aabb         = leaf.get_aabb(ref.item_id);

        // Make sure the current tree is correct prior to changing.
        uint32_t tree_id = _handle_get_tree_id(item_id);

        // Remove from the old tree.
        node_remove_item(item_id, tree_id);

        // Set the pairable after getting the current tree,
        // because the pairable status determines which tree.
        ex.pairable = p_pairable;

        // Add to new tree.
        tree_id = _handle_get_tree_id(item_id);
        create_root_node(tree_id);

        // We must choose where to add it to the tree.
        ref.node_id =
            _logic_choose_item_add_node(_root_node_id[tree_id], bvh_aabb);
        bool needs_refit = _node_add_item(ref.node_id, item_id, bvh_aabb);

        // Only need to refit from the parent.
        if (needs_refit) {
            const Node<BoundingBox, Point>& add_node = _nodes[ref.node_id];
            if (add_node.parent_id != INVALID) {
                refit_upward_and_balance(add_node.parent_id, tree_id);
            }
        }
    } else {
        ex.pairable = p_pairable;
    }

    return state_changed;
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::incremental_optimize() {
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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::update() {
    incremental_optimize();

    // Keep the expansion values up to date with the world bound.
// #define BVH_ALLOW_AUTO_EXPANSION
#ifdef BVH_ALLOW_AUTO_EXPANSION
    if (_auto_node_expansion || _auto_pairing_expansion) {
        AABB<BoundingBox, Point> world_bound;
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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::params_set_pairing_expansion(
    real_t p_value
) {
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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
bool Tree<T, MAX_ITEMS, BoundingBox, Point>::expanded_aabb_encloses_not_shrink(
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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::_debug_node_verify_bound(
    uint32_t p_node_id
) {
    Node<BoundingBox, Point>& node           = _nodes[p_node_id];
    AABB<BoundingBox, Point> bvh_aabb_before = node.aabb;

    node_update_aabb(node);

    AABB<BoundingBox, Point> bvh_aabb_after = node.aabb;
    CRASH_COND(bvh_aabb_before != bvh_aabb_after);
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::node_update_aabb(
    Node<BoundingBox, Point>& node
) {
    node.aabb.set_to_max_opposite_extents();
    node.height = 0;

    if (!node.is_leaf()) {
        for (int n = 0; n < node.num_children; n++) {
            uint32_t child_node_id = node.children[n];

            // Merge with child AABB.
            const Node<BoundingBox, Point>& tchild = _nodes[child_node_id];
            node.aabb.merge(tchild.aabb);

            // Do heights at the same time.
            if (tchild.height > node.height) {
                node.height = tchild.height;
            }
        }

        // The height of a non leaf is always +1 the biggest child.
        node.height++;

#ifdef BVH_CHECKS
        if (!node.num_children) {
            // An empty AABB will break the parent AABBs.
            WARN_PRINT("Node has no children, AABB is undefined");
        }
#endif
    } else {
        const Leaf<MAX_ITEMS, BoundingBox, Point>& leaf = _node_get_leaf(node);

        for (int n = 0; n < leaf.num_items; n++) {
            node.aabb.merge(leaf.get_aabb(n));
        }

        // The leaf items are unexpanded. We expand only in the node AABBs.
        node.aabb.expand(_node_expansion);
#ifdef BVH_CHECKS
        if (!leaf.num_items) {
            // An empty AABB will break the parent AABBs.
            WARN_PRINT("Leafhas no items, AABB is undefined");
        }
#endif
    }
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::refit_all(int p_tree_id) {
    refit_downward(_root_node_id[p_tree_id]);
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::refit_upward(uint32_t p_node_id) {
    while (p_node_id != INVALID) {
        Node<BoundingBox, Point>& node = _nodes[p_node_id];
        node_update_aabb(node);
        p_node_id = node.parent_id;
    }
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::refit_upward_and_balance(
    uint32_t p_node_id,
    uint32_t p_tree_id
) {
    while (p_node_id != INVALID) {
        uint32_t before = p_node_id;
        p_node_id       = _logic_balance(p_node_id, p_tree_id);

        if (before != p_node_id) {
            VERBOSE_PRINT("REBALANCED!");
        }

        Node<BoundingBox, Point>& node = _nodes[p_node_id];

        // Update overall AABB from the children.
        node_update_aabb(node);

        p_node_id = node.parent_id;
    }
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::refit_downward(uint32_t p_node_id
) {
    Node<BoundingBox, Point>& node = _nodes[p_node_id];

    // Do children first.
    if (!node.is_leaf()) {
        for (int n = 0; n < node.num_children; n++) {
            refit_downward(node.children[n]);
        }
    }

    node_update_aabb(node);
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::refit_branch(uint32_t p_node_id) {
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
        Node<BoundingBox, Point>& node = _nodes[rp.node_id];

        // Do children first.
        if (!node.is_leaf()) {
            for (int n = 0; n < node.num_children; n++) {
                uint32_t child_id = node.children[n];

                // Add to the stack.
                RefitParams* child = ii.request();
                child->node_id     = child_id;
            }
        } else {
            // Only refit upward if leaf is dirty.
            Leaf<MAX_ITEMS, BoundingBox, Point>& leaf = _node_get_leaf(node);
            if (leaf.is_dirty()) {
                leaf.set_dirty(false);
                refit_upward(p_node_id);
            }
        }
    } // Loop while more nodes to pop.
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::_split_inform_references(
    uint32_t p_node_id
) {
    Node<BoundingBox, Point>& node            = _nodes[p_node_id];
    Leaf<MAX_ITEMS, BoundingBox, Point>& leaf = _node_get_leaf(node);

    for (int n = 0; n < leaf.num_items; n++) {
        uint32_t ref_id = leaf.get_item_ref_id(n);

        ItemRef& ref = _refs[ref_id];
        ref.node_id  = p_node_id;
        ref.item_id  = n;
    }
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::_split_leaf_sort_groups_simple(
    int& num_a,
    int& num_b,
    uint16_t* group_a,
    uint16_t* group_b,
    const AABB<BoundingBox, Point>* temp_bounds,
    const AABB<BoundingBox, Point> full_bound
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

        if (temp_bounds[ind].min.coord[split_axis] > centre.coord[split_axis]) {
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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
void Tree<T, MAX_ITEMS, BoundingBox, Point>::_split_leaf_sort_groups(
    int& num_a,
    int& num_b,
    uint16_t* group_a,
    uint16_t* group_b,
    const AABB<BoundingBox, Point>* temp_bounds
) {
    AABB<BoundingBox, Point> groupb_aabb;
    groupb_aabb.set_to_max_opposite_extents();
    for (int n = 0; n < num_b; n++) {
        int which = group_b[n];
        groupb_aabb.merge(temp_bounds[which]);
    }
    AABB<BoundingBox, Point> groupb_aabb_new;

    AABB<BoundingBox, Point> rest_aabb;

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

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
uint32_t Tree<T, MAX_ITEMS, BoundingBox, Point>::split_leaf(
    uint32_t p_node_id,
    const AABB<BoundingBox, Point>& p_added_item_aabb
) {
    return split_leaf_complex(p_node_id, p_added_item_aabb);
}

template <class T, int MAX_ITEMS, class BoundingBox, class Point>
uint32_t Tree<T, MAX_ITEMS, BoundingBox, Point>::split_leaf_complex(
    uint32_t p_node_id,
    const AABB<BoundingBox, Point>& p_added_item_aabb
) {
    VERBOSE_PRINT("split_leaf");

    // Note: The node before and after splitting may be a different address
    // in memory, because the vector could get relocated.
    // We need to reget the node after the split.
    BVH_ASSERT(_nodes[p_node_id].is_leaf());

    // Create child leaf nodes.
    uint32_t* child_ids = (uint32_t*)alloca(sizeof(uint32_t) * 2);

    for (int n = 0; n < 2; n++) {
        // Create node children.
        Node<BoundingBox, Point>* child_node = _nodes.request(child_ids[n]);

        child_node->clear();

        // Back link to parent.
        child_node->parent_id = p_node_id;

        // Make each child a leaf node.
        node_make_leaf(child_ids[n]);
    }

    // Don't get any leaves or nodes untill after the split.
    Node<BoundingBox, Point>& node                       = _nodes[p_node_id];
    uint32_t orig_leaf_id                                = node.get_leaf_id();
    const Leaf<MAX_ITEMS, BoundingBox, Point>& orig_leaf = _node_get_leaf(node);

    // Store the final child ids.
    for (int n = 0; n < 2; n++) {
        node.children[n] = child_ids[n];
    }

    // Mark as no longer a leaf node.
    node.num_children = 2;

    // Assign childeren to each group equally.
    // Plus 1 for the wildcard. The item being added.
    int max_children = orig_leaf.num_items + 1;

    uint16_t* group_a = (uint16_t*)alloca(sizeof(uint16_t) * max_children);
    uint16_t* group_b = (uint16_t*)alloca(sizeof(uint16_t) * max_children);

    // We are copying the AABBs, because we need one for the inserted item.
    AABB<BoundingBox, Point>* temp_bounds = (AABB<BoundingBox, Point>*)alloca(
        sizeof(AABB<BoundingBox, Point>) * max_children
    );

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
        node.aabb
    );

    uint32_t wildcard_node = INVALID;

    // Now there should be equal numbers in both groups.
    for (int n = 0; n < num_a; n++) {
        int which = group_a[n];

        if (which != wildcard) {
            const AABB<BoundingBox, Point>& source_item_aabb =
                orig_leaf.get_aabb(which);
            uint32_t source_item_ref_id = orig_leaf.get_item_ref_id(which);
            _node_add_item(
                node.children[0],
                source_item_ref_id,
                source_item_aabb
            );
        } else {
            wildcard_node = node.children[0];
        }
    }
    for (int n = 0; n < num_b; n++) {
        int which = group_b[n];

        if (which != wildcard) {
            const AABB<BoundingBox, Point>& source_item_aabb =
                orig_leaf.get_aabb(which);
            uint32_t source_item_ref_id = orig_leaf.get_item_ref_id(which);
            _node_add_item(
                node.children[1],
                source_item_ref_id,
                source_item_aabb
            );
        } else {
            wildcard_node = node.children[1];
        }
    }

    // Remove all items from the parent and replace with the child nodes.
    _leaves.free(orig_leaf_id);

    // Keep the references up to date.
    for (int n = 0; n < 2; n++) {
        _split_inform_references(node.children[n]);
    }

    refit_upward(p_node_id);

    BVH_ASSERT(wildcard_node != INVALID);
    return wildcard_node;
}

#undef VERBOSE_PRINT

} // namespace BVH

#endif // BVH_TREE_H
