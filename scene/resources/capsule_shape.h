// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef CAPSULE_SHAPE_H
#define CAPSULE_SHAPE_H

#include "scene/resources/shape.h"

class CapsuleShape : public Shape {
    REBEL_OBJECT(CapsuleShape, Shape);
    float radius;
    float height;

protected:
    static void _bind_methods();

    void _update_shape() override;

public:
    void set_radius(float p_radius);
    float get_radius() const;
    void set_height(float p_height);
    float get_height() const;

    Vector<Vector3> get_debug_mesh_lines() override;

    CapsuleShape();
};

#endif // CAPSULE_SHAPE_H
