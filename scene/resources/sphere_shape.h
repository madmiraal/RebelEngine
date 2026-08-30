// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef SPHERE_SHAPE_H
#define SPHERE_SHAPE_H

#include "scene/resources/shape.h"

class SphereShape : public Shape {
    REBEL_OBJECT(SphereShape, Shape);
    float radius;

protected:
    static void _bind_methods();

    void _update_shape() override;

public:
    void set_radius(float p_radius);
    float get_radius() const;

    Vector<Vector3> get_debug_mesh_lines() override;

    SphereShape();
};

#endif // SPHERE_SHAPE_H
