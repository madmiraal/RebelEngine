// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef RASTERIZER_GLES2_H
#define RASTERIZER_GLES2_H

#include "rasterizer_canvas_gles2.h"
#include "rasterizer_scene_gles2.h"
#include "rasterizer_storage_gles2.h"
#include "servers/visual/rasterizer.h"

class RasterizerGLES2 : public Rasterizer {
    static Rasterizer* _create_current();

    RasterizerStorageGLES2* storage;
    RasterizerCanvasGLES2* canvas;
    RasterizerSceneGLES2* scene;

    double time_total;
    float time_scale;

public:
    RasterizerStorage* get_storage() override;
    RasterizerCanvas* get_canvas() override;
    RasterizerScene* get_scene() override;

    void set_boot_image(
        const Ref<Image>& p_image,
        const Color& p_color,
        bool p_scale,
        bool p_use_filter = true
    ) override;
    void set_shader_time_scale(float p_scale) override;

    void initialize() override;
    void begin_frame(double frame_step) override;
    void set_current_render_target(RID p_render_target) override;
    void restore_render_target(bool p_3d_was_drawn) override;
    void clear_render_target(const Color& p_color) override;
    void blit_render_target_to_screen(
        RID p_render_target,
        const Rect2& p_screen_rect,
        int p_screen = 0
    ) override;
    void output_lens_distorted_to_screen(
        RID p_render_target,
        const Rect2& p_screen_rect,
        float p_k1,
        float p_k2,
        const Vector2& p_eye_center,
        float p_oversample
    ) override;
    void end_frame(bool p_swap_buffers) override;
    void finalize() override;

    static Error is_viable();
    static void make_current();
    static void register_config();

    bool is_low_end() const override {
        return true;
    }

    static bool gl_check_errors();

    RasterizerGLES2();
    ~RasterizerGLES2() override;
};

#endif // RASTERIZER_GLES2_H
