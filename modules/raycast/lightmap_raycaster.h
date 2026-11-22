// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef LIGHTMAP_RAYCASTER_H
#define LIGHTMAP_RAYCASTER_H

#include "core/object.h"
#include "scene/3d/lightmapper.h"
#include "scene/resources/mesh.h"

#include <embree4/rtcore.h>

class LightmapRaycasterEmbree : public LightmapRaycaster {
    GDCLASS(LightmapRaycasterEmbree, LightmapRaycaster);

public:
    LightmapRaycasterEmbree();
    ~LightmapRaycasterEmbree() override;

    void add_mesh(
        const Vector<Vector3>& vertices,
        const Vector<Vector3>& normals,
        const Vector<Vector2>& uv2s,
        unsigned int id
    ) override;

    bool intersect(Ray& ray) override;
    void intersect(Vector<Ray>& rays) override;

    void set_mesh_alpha_texture(
        Ref<Image> alpha_image,
        unsigned int id
    ) override;
    void set_mesh_filter(const Set<int>& mesh_ids) override;
    void clear_mesh_filter() override;

    void commit() override;

    static LightmapRaycaster* create_embree_raycaster();
    static void make_default_raycaster();

private:
    struct AlphaTexture {
        Vector<uint8_t> data;
        Size2i size;

        uint8_t sample(float u, float v) const;
    };

    RTCDevice embree_device;
    RTCScene embree_scene;

    static void filter_function(const RTCFilterFunctionNArguments* arguments);

    Map<unsigned int, AlphaTexture> alpha_textures;
    Set<int> filter_meshes;
};

#endif // LIGHTMAP_RAYCASTER_H
