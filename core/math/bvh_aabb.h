// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef BVH_AABB_H
#define BVH_AABB_H

#include "core/math/aabb.h"
#include "core/math/plane.h"
#include "core/math/vector3.h"

namespace BVH {

enum class IntersectResult {
    MISS,
    PARTIAL,
    FULL,
};

struct ConvexHull {
    const Plane* planes;
    int num_planes;
    const Vector3* points;
    int num_points;
};

template <class Point>
struct Segment {
    Point from;
    Point to;
};

// Optimized version of axis aligned bounding box.
template <typename BoundingBox = ::AABB, typename Point = Vector3>
class AABB {
public:
    AABB() = default;
    AABB(const BoundingBox& source);
    operator BoundingBox() const;

    void merge(const AABB& p_o);
    Point calculate_size() const;
    Point calculate_centre() const;
    real_t get_proximity_to(const AABB& p_b) const;
    int select_by_proximity(const AABB& p_a, const AABB& p_b) const;
    uint32_t find_cutting_planes(
        const ConvexHull& p_hull,
        uint32_t* p_plane_ids
    ) const;
    bool intersects_plane(const Plane& p_p) const;
    bool intersects_convex_optimized(
        const ConvexHull& p_hull,
        const uint32_t* p_plane_ids,
        uint32_t p_num_planes
    ) const;
    bool intersects_convex_partial(const ConvexHull& p_hull) const;
    IntersectResult intersects_convex(const ConvexHull& p_hull) const;
    bool is_within_convex(const ConvexHull& p_hull) const;
    bool is_point_within_hull(const ConvexHull& p_hull, const Vector3& p_pt)
        const;
    bool intersects_segment(const Segment<Point>& p_segment) const;
    bool intersects_point(const Point& p_pt) const;
    bool intersects(const AABB& p_o) const;
    bool is_other_within(const AABB& p_o) const;
    void grow(const Point& p_change);
    void expand(real_t p_change);
    // Surface area metric.
    float get_area() const;
    void set_to_max_opposite_extents();
    bool _any_morethan(const Point& p_a, const Point& p_b) const;
    bool _any_lessthan(const Point& p_a, const Point& p_b) const;

    // Minimums are stored with a negative value to test them with SIMD.
    Point minimum;
    Point maximum;
};

// Definitions

template <typename BoundingBox, typename Point>
AABB<BoundingBox, Point>::AABB(const BoundingBox& source) :
    minimum(source.position),
    maximum(source.position + source.size) {}

template <typename BoundingBox, typename Point>
AABB<BoundingBox, Point>::operator BoundingBox() const {
    BoundingBox bounding_box;
    bounding_box.position = minimum;
    bounding_box.size     = calculate_size();
    return bounding_box;
}

template <typename BoundingBox, typename Point>
void AABB<BoundingBox, Point>::merge(const AABB& p_o) {
    for (int axis = 0; axis < Point::AXIS_COUNT; ++axis) {
        minimum[axis] = MIN(minimum[axis], p_o.minimum[axis]);
        maximum[axis] = MAX(maximum[axis], p_o.maximum[axis]);
    }
}

template <typename BoundingBox, typename Point>
Point AABB<BoundingBox, Point>::calculate_size() const {
    return maximum - minimum;
}

template <typename BoundingBox, typename Point>
Point AABB<BoundingBox, Point>::calculate_centre() const {
    return Point(minimum + (calculate_size() * 0.5));
}

template <typename BoundingBox, typename Point>
real_t AABB<BoundingBox, Point>::get_proximity_to(const AABB& p_b) const {
    const Point d    = (minimum - p_b.minimum) + (maximum - p_b.maximum);
    real_t proximity = 0.0;
    for (int axis = 0; axis < Point::AXIS_COUNT; ++axis) {
        proximity += Math::abs(d[axis]);
    }
    return proximity;
}

template <typename BoundingBox, typename Point>
int AABB<BoundingBox, Point>::select_by_proximity(
    const AABB& p_a,
    const AABB& p_b
) const {
    return (get_proximity_to(p_a) < get_proximity_to(p_b) ? 0 : 1);
}

template <typename BoundingBox, typename Point>
uint32_t AABB<BoundingBox, Point>::find_cutting_planes(
    const ConvexHull& p_hull,
    uint32_t* p_plane_ids
) const {
    uint32_t count = 0;

    for (int n = 0; n < p_hull.num_planes; n++) {
        const Plane& p = p_hull.planes[n];
        if (intersects_plane(p)) {
            p_plane_ids[count++] = n;
        }
    }

    return count;
}

template <typename BoundingBox, typename Point>
bool AABB<BoundingBox, Point>::intersects_plane(const Plane& p_p) const {
    Vector3 size         = calculate_size();
    Vector3 half_extents = size * 0.5;
    Vector3 ofs          = minimum + half_extents;

    // Forward side of plane.
    Vector3 point_offset(
        (p_p.normal.x < 0) ? -half_extents.x : half_extents.x,
        (p_p.normal.y < 0) ? -half_extents.y : half_extents.y,
        (p_p.normal.z < 0) ? -half_extents.z : half_extents.z
    );
    Vector3 point = point_offset + ofs;

    if (!p_p.is_point_over(point)) {
        return false;
    }

    point = -point_offset + ofs;
    if (p_p.is_point_over(point)) {
        return false;
    }

    return true;
}

template <typename BoundingBox, typename Point>
bool AABB<BoundingBox, Point>::intersects_convex_optimized(
    const ConvexHull& p_hull,
    const uint32_t* p_plane_ids,
    uint32_t p_num_planes
) const {
    Vector3 size         = calculate_size();
    Vector3 half_extents = size * 0.5;
    Vector3 ofs          = minimum + half_extents;

    for (unsigned int i = 0; i < p_num_planes; i++) {
        const Plane& p = p_hull.planes[p_plane_ids[i]];
        Vector3 point(
            (p.normal.x > 0) ? -half_extents.x : half_extents.x,
            (p.normal.y > 0) ? -half_extents.y : half_extents.y,
            (p.normal.z > 0) ? -half_extents.z : half_extents.z
        );
        point += ofs;
        if (p.is_point_over(point)) {
            return false;
        }
    }

    return true;
}

template <typename BoundingBox, typename Point>
bool AABB<BoundingBox, Point>::intersects_convex_partial(
    const ConvexHull& p_hull
) const {
    BoundingBox bb = *this;
    return bb.intersects_convex_shape(
        p_hull.planes,
        p_hull.num_planes,
        p_hull.points,
        p_hull.num_points
    );
}

template <typename BoundingBox, typename Point>
IntersectResult AABB<BoundingBox, Point>::intersects_convex(
    const ConvexHull& p_hull
) const {
    if (intersects_convex_partial(p_hull)) {
        // Fully within is very important for tree checks.
        if (is_within_convex(p_hull)) {
            return IntersectResult::FULL;
        }

        return IntersectResult::PARTIAL;
    }

    return IntersectResult::MISS;
}

template <typename BoundingBox, typename Point>
bool AABB<BoundingBox, Point>::is_within_convex(const ConvexHull& p_hull
) const {
    // Use half extents routine.
    BoundingBox bb = *this;
    return bb.inside_convex_shape(p_hull.planes, p_hull.num_planes);
}

template <typename BoundingBox, typename Point>
bool AABB<BoundingBox, Point>::is_point_within_hull(
    const ConvexHull& p_hull,
    const Vector3& p_pt
) const {
    for (int n = 0; n < p_hull.num_planes; n++) {
        if (p_hull.planes[n].distance_to(p_pt) > 0.0f) {
            return false;
        }
    }
    return true;
}

template <typename BoundingBox, typename Point>
bool AABB<BoundingBox, Point>::intersects_segment(
    const Segment<Point>& p_segment
) const {
    BoundingBox bb = *this;
    return bb.intersects_segment(p_segment.from, p_segment.to);
}

template <typename BoundingBox, typename Point>
bool AABB<BoundingBox, Point>::intersects_point(const Point& p_pt) const {
    if (_any_lessthan(p_pt, minimum)) {
        return false;
    }
    if (_any_morethan(p_pt, maximum)) {
        return false;
    }
    return true;
}

template <typename BoundingBox, typename Point>
bool AABB<BoundingBox, Point>::intersects(const AABB& p_o) const {
    if (_any_morethan(minimum, p_o.maximum)) {
        return false;
    }
    if (_any_lessthan(maximum, p_o.minimum)) {
        return false;
    }
    return true;
}

template <typename BoundingBox, typename Point>
bool AABB<BoundingBox, Point>::is_other_within(const AABB& p_o) const {
    if (_any_morethan(minimum, p_o.minimum)) {
        return false;
    }
    if (_any_lessthan(maximum, p_o.maximum)) {
        return false;
    }
    return true;
}

template <typename BoundingBox, typename Point>
void AABB<BoundingBox, Point>::grow(const Point& p_change) {
    minimum -= p_change;
    maximum += p_change;
}

template <typename BoundingBox, typename Point>
void AABB<BoundingBox, Point>::expand(real_t p_change) {
    Point change;
    change.set_all(p_change);
    grow(change);
}

template <typename BoundingBox, typename Point>
float AABB<BoundingBox, Point>::get_area() const {
    Point d = calculate_size();
    return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
}

template <typename BoundingBox, typename Point>
void AABB<BoundingBox, Point>::set_to_max_opposite_extents() {
    maximum.set_all(FLT_MAX);
    minimum = -maximum;
}

template <typename BoundingBox, typename Point>
bool AABB<BoundingBox, Point>::_any_morethan(const Point& p_a, const Point& p_b)
    const {
    for (int axis = 0; axis < Point::AXIS_COUNT; ++axis) {
        if (p_a[axis] > p_b[axis]) {
            return true;
        }
    }
    return false;
}

template <typename BoundingBox, typename Point>
bool AABB<BoundingBox, Point>::_any_lessthan(const Point& p_a, const Point& p_b)
    const {
    for (int axis = 0; axis < Point::AXIS_COUNT; ++axis) {
        if (p_a[axis] < p_b[axis]) {
            return true;
        }
    }
    return false;
}

} // namespace BVH

#endif // BVH_AABB_H
