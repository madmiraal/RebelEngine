// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef BVH_NODE_H
#define BVH_NODE_H

namespace BVH {

template <class BoundingBox, class Point>
class Node {
public:
    bool is_leaf() const;
    void set_leaf_id(int id);
    int get_leaf_id() const;
    void clear();
    bool is_full_of_children() const;
    void remove_child_internal(uint32_t child_num);
    int find_child(uint32_t p_child_node_id);

    AABB<BoundingBox, Point> aabb;

    // Either number of children is positive or leaf id if negative.
    // Leaf id = 0 is disallowed.
    union {
        int32_t num_children;
        int32_t neg_leaf_id;
    };

    // Set to -1 if there is no parent.
    uint32_t parent_id;
    uint16_t children[2];
    // Height in the tree, where leaves are 0, and all above are +1.
    int32_t height;
};

// Definitions

template <class BoundingBox, class Point>
bool Node<BoundingBox, Point>::is_leaf() const {
    return num_children < 0;
}

template <class BoundingBox, class Point>
void Node<BoundingBox, Point>::set_leaf_id(int id) {
    neg_leaf_id = -id;
}

template <class BoundingBox, class Point>
int Node<BoundingBox, Point>::get_leaf_id() const {
    return -neg_leaf_id;
}

template <class BoundingBox, class Point>
void Node<BoundingBox, Point>::clear() {
    num_children = 0;
    parent_id    = -1;
    // Set to -1 for testing.
    height       = 0;
    // For safety, set to improbable value.
    aabb.set_to_max_opposite_extents();
}

template <class BoundingBox, class Point>
bool Node<BoundingBox, Point>::is_full_of_children() const {
    return num_children >= 2;
}

template <class BoundingBox, class Point>
void Node<BoundingBox, Point>::remove_child_internal(uint32_t child_num) {
    children[child_num] = children[num_children - 1];
    num_children--;
}

template <class BoundingBox, class Point>
int Node<BoundingBox, Point>::find_child(uint32_t p_child_node_id) {
    ERR_FAIL_COND_V_MSG(
        !is_leaf(),
        -1,
        "BVH node is a leaf - it has no children"
    );

    for (int n = 0; n < num_children; n++) {
        if (children[n] == p_child_node_id) {
            return n;
        }
    }

    // Not found.
    return -1;
}

} // namespace BVH

#endif // BVH_NODE_H
