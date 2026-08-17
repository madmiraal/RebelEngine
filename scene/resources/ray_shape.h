// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef RAY_SHAPE_H
#define RAY_SHAPE_H
#include "scene/resources/shape.h"

class RayShape : public Shape {
    GDCLASS(RayShape, Shape);
    float length;
    bool slips_on_slope;

protected:
    static void _bind_methods();
    void _update_shape() override;

public:
    void set_length(float p_length);
    float get_length() const;

    void set_slips_on_slope(bool p_active);
    bool get_slips_on_slope() const;

    Vector<Vector3> get_debug_mesh_lines() override;

    RayShape();
};
#endif // RAY_SHAPE_H
