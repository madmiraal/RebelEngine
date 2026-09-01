// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef HEIGHT_MAP_SHAPE_H
#define HEIGHT_MAP_SHAPE_H

#include "scene/resources/shape.h"

class HeightMapShape : public Shape {
    GDCLASS(HeightMapShape, Shape);

    int map_width;
    int map_depth;
    PoolRealArray map_data;
    float min_height;
    float max_height;

protected:
    static void _bind_methods();
    void _update_shape() override;

public:
    void set_map_width(int p_new);
    int get_map_width() const;
    void set_map_depth(int p_new);
    int get_map_depth() const;
    void set_map_data(PoolRealArray p_new);
    PoolRealArray get_map_data() const;

    Vector<Vector3> get_debug_mesh_lines() override;

    HeightMapShape();
};

#endif // HEIGHT_MAP_SHAPE_H
