// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef BVH_CULL_PARAMETERS_H
#define BVH_CULL_PARAMETERS_H

#include <core/math/bvh_aabb.h>

namespace BVH {

template <class T, class BoundingBox, class Point>
struct CullParameters {
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
    AABB<BoundingBox, Point> bvh_aabb;
    ConvexHull hull;
    Segment<Point> segment;

    // When collision testing, non pairable moving items only need to be
    // tested against the pairable tree.
    // Collisions with other non pairable items are irrelevant.
    bool test_pairable_only;
};

} // namespace BVH

#endif // BVH_CULL_PARAMETERS_H
