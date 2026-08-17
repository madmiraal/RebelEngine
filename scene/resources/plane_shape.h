// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef PLANE_SHAPE_H
#define PLANE_SHAPE_H

#include "scene/resources/shape.h"

class PlaneShape : public Shape {
    GDCLASS(PlaneShape, Shape);
    Plane plane;

protected:
    static void _bind_methods();
    void _update_shape() override;

public:
    void set_plane(Plane p_plane);
    Plane get_plane() const;

    Vector<Vector3> get_debug_mesh_lines() override;

    PlaneShape();
};
#endif // PLANE_SHAPE_H
