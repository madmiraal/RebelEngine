// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef BVH_LEAF_H
#define BVH_LEAF_H

namespace BVH {

template <int MAX_ITEMS, class BoundingBox, class Point>
class Leaf {
public:
    AABB<BoundingBox, Point>& get_aabb(uint32_t p_id);
    const AABB<BoundingBox, Point>& get_aabb(uint32_t p_id) const;
    uint32_t& get_item_ref_id(uint32_t p_id);
    const uint32_t& get_item_ref_id(uint32_t p_id) const;
    bool is_dirty() const;
    void set_dirty(bool p);
    void clear();
    bool is_full() const;
    void remove_item_unordered(uint32_t p_id);
    uint32_t request_item();

    uint16_t num_items;

private:
    // Separate data orientated lists for faster SIMD traversal.
    uint32_t item_ref_ids[MAX_ITEMS];
    AABB<BoundingBox, Point> aabbs[MAX_ITEMS];
    bool dirty;
};

// Definitions

template <int MAX_ITEMS, class BoundingBox, class Point>
AABB<BoundingBox, Point>& Leaf<MAX_ITEMS, BoundingBox, Point>::get_aabb(
    uint32_t p_id
) {
    return aabbs[p_id];
}

template <int MAX_ITEMS, class BoundingBox, class Point>
const AABB<BoundingBox, Point>& Leaf<MAX_ITEMS, BoundingBox, Point>::get_aabb(
    uint32_t p_id
) const {
    return aabbs[p_id];
}

template <int MAX_ITEMS, class BoundingBox, class Point>
uint32_t& Leaf<MAX_ITEMS, BoundingBox, Point>::get_item_ref_id(uint32_t p_id) {
    return item_ref_ids[p_id];
}

template <int MAX_ITEMS, class BoundingBox, class Point>
const uint32_t& Leaf<MAX_ITEMS, BoundingBox, Point>::get_item_ref_id(
    uint32_t p_id
) const {
    return item_ref_ids[p_id];
}

template <int MAX_ITEMS, class BoundingBox, class Point>
bool Leaf<MAX_ITEMS, BoundingBox, Point>::is_dirty() const {
    return dirty;
}

template <int MAX_ITEMS, class BoundingBox, class Point>
void Leaf<MAX_ITEMS, BoundingBox, Point>::set_dirty(bool new_dirty) {
    dirty = new_dirty;
}

template <int MAX_ITEMS, class BoundingBox, class Point>
void Leaf<MAX_ITEMS, BoundingBox, Point>::clear() {
    num_items = 0;
    dirty     = true;
}

template <int MAX_ITEMS, class BoundingBox, class Point>
bool Leaf<MAX_ITEMS, BoundingBox, Point>::is_full() const {
    return num_items >= MAX_ITEMS;
}

template <int MAX_ITEMS, class BoundingBox, class Point>
void Leaf<MAX_ITEMS, BoundingBox, Point>::remove_item_unordered(uint32_t p_id) {
    ERR_FAIL_UNSIGNED_INDEX_MSG(
        p_id,
        num_items,
        "Trying to remove an invalid BVH item"
    );
    num_items--;
    aabbs[p_id]        = aabbs[num_items];
    item_ref_ids[p_id] = item_ref_ids[num_items];
}

template <int MAX_ITEMS, class BoundingBox, class Point>
uint32_t Leaf<MAX_ITEMS, BoundingBox, Point>::request_item() {
    CRASH_COND_MSG(num_items >= MAX_ITEMS, "BVH leaf is full");
    uint32_t id = num_items;
    num_items++;
    return id;
}

} // namespace BVH

#endif // BVH_LEAF_H
