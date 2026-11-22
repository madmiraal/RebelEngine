// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "lightmap_raycaster.h"

#ifdef __SSE2__
#include <pmmintrin.h>
#endif

static void embree_error_handler(void*, RTCError, const char* error_string) {
    print_error("Embree error: " + String(error_string));
}

static float blerp(
    const float c00,
    const float c10,
    const float c01,
    const float c11,
    const float tx,
    const float ty
) {
    return Math::lerp(Math::lerp(c00, c10, tx), Math::lerp(c01, c11, tx), ty);
}

LightmapRaycasterEmbree::LightmapRaycasterEmbree() {
#ifdef __SSE2__
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif

    embree_device = rtcNewDevice(nullptr);
    rtcSetDeviceErrorFunction(embree_device, &embree_error_handler, nullptr);
    embree_scene = rtcNewScene(embree_device);
}

LightmapRaycasterEmbree::~LightmapRaycasterEmbree() {
#ifdef __SSE2__
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_OFF);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_OFF);
#endif

    if (embree_scene != nullptr) {
        rtcReleaseScene(embree_scene);
    }

    if (embree_device != nullptr) {
        rtcReleaseDevice(embree_device);
    }
}

void LightmapRaycasterEmbree::add_mesh(
    const Vector<Vector3>& vertices,
    const Vector<Vector3>& normals,
    const Vector<Vector2>& uv2s,
    const unsigned int id
) {
    RTCGeometry embree_mesh =
        rtcNewGeometry(embree_device, RTC_GEOMETRY_TYPE_TRIANGLE);
    rtcSetGeometryVertexAttributeCount(embree_mesh, 2);

    const int vertex_count = vertices.size();
    ERR_FAIL_COND(vertex_count % 3 != 0);
    ERR_FAIL_COND(vertex_count != uv2s.size());
    ERR_FAIL_COND(!normals.empty() && vertex_count != normals.size());

    auto* embree_vertices = static_cast<Vector3*>(rtcSetNewGeometryBuffer(
        embree_mesh,
        RTC_BUFFER_TYPE_VERTEX,
        0,
        RTC_FORMAT_FLOAT3,
        sizeof(Vector3),
        vertex_count
    ));
    memcpy(embree_vertices, vertices.ptr(), sizeof(Vector3) * vertex_count);

    auto* embree_light_uvs = static_cast<Vector2*>(rtcSetNewGeometryBuffer(
        embree_mesh,
        RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE,
        0,
        RTC_FORMAT_FLOAT2,
        sizeof(Vector2),
        vertex_count
    ));
    memcpy(embree_light_uvs, uv2s.ptr(), sizeof(Vector2) * vertex_count);

    auto* embree_triangles = static_cast<uint32_t*>(rtcSetNewGeometryBuffer(
        embree_mesh,
        RTC_BUFFER_TYPE_INDEX,
        0,
        RTC_FORMAT_UINT3,
        sizeof(uint32_t) * 3,
        vertex_count / 3
    ));
    for (int i = 0; i < vertex_count; i++) {
        embree_triangles[i] = i;
    }

    if (!normals.empty()) {
        auto* embree_normals = static_cast<Vector3*>(rtcSetNewGeometryBuffer(
            embree_mesh,
            RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE,
            1,
            RTC_FORMAT_FLOAT3,
            sizeof(Vector3),
            vertex_count
        ));
        memcpy(embree_normals, normals.ptr(), sizeof(Vector3) * vertex_count);
    }

    rtcCommitGeometry(embree_mesh);
    rtcSetGeometryIntersectFilterFunction(embree_mesh, filter_function);
    rtcSetGeometryUserData(embree_mesh, this);
    rtcAttachGeometryByID(embree_scene, embree_mesh, id);
    rtcReleaseGeometry(embree_mesh);
}

bool LightmapRaycasterEmbree::intersect(Ray& ray) {
    rtcIntersect1(embree_scene, reinterpret_cast<RTCRayHit*>(&ray));
    return ray.geomID != RTC_INVALID_GEOMETRY_ID;
}

void LightmapRaycasterEmbree::intersect(Vector<Ray>& rays) {
    Ray* rays_ptrw = rays.ptrw();
    for (int i = 0; i < rays.size(); ++i) {
        intersect(rays_ptrw[i]);
    }
}

void LightmapRaycasterEmbree::set_mesh_alpha_texture(
    Ref<Image> alpha_image,
    const unsigned int id
) {
    if (alpha_image.is_valid() && alpha_image->get_size() != Vector2i()) {
        AlphaTexture alpha_texture;
        alpha_texture.size = alpha_image->get_size();
        alpha_texture.data.resize(alpha_image->get_data().size());

        const PoolVector<uint8_t>::Read alpha_image_data =
            alpha_image->get_data().read();
        uint8_t* alpha_texture_data = alpha_texture.data.ptrw();
        for (int i = 0; i < alpha_texture.data.size(); i++) {
            alpha_texture_data[i] = alpha_image_data[i];
        }

        alpha_textures.insert(id, alpha_texture);
    }
}

void LightmapRaycasterEmbree::set_mesh_filter(const Set<int>& mesh_ids) {
    for (Set<int>::Element* E = mesh_ids.front(); E; E = E->next()) {
        rtcDisableGeometry(rtcGetGeometry(embree_scene, E->get()));
    }
    commit();
    filter_meshes = mesh_ids;
}

void LightmapRaycasterEmbree::clear_mesh_filter() {
    for (Set<int>::Element* E = filter_meshes.front(); E; E = E->next()) {
        rtcEnableGeometry(rtcGetGeometry(embree_scene, E->get()));
    }
    commit();
    filter_meshes.clear();
}

void LightmapRaycasterEmbree::commit() {
    rtcCommitScene(embree_scene);
}

LightmapRaycaster* LightmapRaycasterEmbree::create_embree_raycaster() {
    return memnew(LightmapRaycasterEmbree);
}

void LightmapRaycasterEmbree::make_default_raycaster() {
    create_function = create_embree_raycaster;
}

void LightmapRaycasterEmbree::filter_function(
    const RTCFilterFunctionNArguments* arguments
) {
    const auto rtc_hit             = reinterpret_cast<RTCHit*>(arguments->hit);
    const unsigned int geometry_id = rtc_hit->geomID;

    auto* ray_caster =
        static_cast<LightmapRaycasterEmbree*>(arguments->geometryUserPtr);
    RTCGeometry geometry =
        rtcGetGeometry(ray_caster->embree_scene, geometry_id);

    rtcInterpolate0(
        geometry,
        rtc_hit->primID,
        rtc_hit->u,
        rtc_hit->v,
        RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE,
        0,
        &rtc_hit->u,
        2
    );

    if (ray_caster->alpha_textures.has(geometry_id)) {
        const AlphaTexture& alpha_texture =
            ray_caster->alpha_textures[geometry_id];
        if (alpha_texture.sample(rtc_hit->u, rtc_hit->v) < 128) {
            arguments->valid[0] = 0;
            return;
        }
    }

    rtcInterpolate0(
        geometry,
        rtc_hit->primID,
        rtc_hit->u,
        rtc_hit->v,
        RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE,
        1,
        &rtc_hit->Ng_x,
        3
    );
}

uint8_t LightmapRaycasterEmbree::AlphaTexture::sample(
    const float u,
    const float v
) const {
    const float x = u * static_cast<float>(size.x);
    const float y = v * static_cast<float>(size.y);
    const int xi  = static_cast<int>(x);
    const int yi  = static_cast<int>(y);

    uint8_t texels[4];

    for (int i = 0; i < 4; ++i) {
        const int sample_x = CLAMP(xi + i % 2, 0, size.x - 1);
        const int sample_y = CLAMP(yi + i / 2, 0, size.y - 1);
        texels[i]          = data[sample_y * size.x + sample_x];
    }

    return static_cast<uint8_t>(Math::round(blerp(
        texels[0],
        texels[1],
        texels[2],
        texels[3],
        x - static_cast<float>(xi),
        y - static_cast<float>(yi)
    )));
}
