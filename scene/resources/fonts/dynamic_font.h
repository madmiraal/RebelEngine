// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef DYNAMIC_FONT_H
#define DYNAMIC_FONT_H

// #include "modules/modules_enabled.gen.h" // For freetype.
// #ifdef MODULE_FREETYPE_ENABLED

// #include "core/io/resource_loader.h"
// #include "core/os/mutex.h"
// #include "core/os/thread_safe.h"
// #include "core/pair.h"
#include "core/color.h"
#include "scene/resources/fonts/dynamic_font_data.h"
#include "scene/resources/fonts/font.h"

// #include "scene/resources/texture.h"

class DynamicFont : public Font {
    GDCLASS(DynamicFont, Font);

public:
    enum SpacingType {
        SPACING_TOP,
        SPACING_BOTTOM,
        SPACING_CHAR,
        SPACING_SPACE
    };

public:
    DynamicFont();
    ~DynamicFont() override;

    float get_ascent() const override;
    float get_descent() const override;
    float get_height() const override;
    bool is_distance_field_hint() const override;
    bool has_outline() const override;
    Size2 get_char_size(CharType p_char, CharType p_next = 0) const override;
    float draw_char(
        RID p_canvas_item,
        const Point2& p_pos,
        CharType p_char,
        CharType p_next         = 0,
        const Color& p_modulate = Color(1, 1, 1),
        bool p_outline          = false
    ) const override;

    Ref<DynamicFontData> get_font_data() const;
    void set_font_data(const Ref<DynamicFontData>& p_data);
    int get_outline_size() const;
    void set_outline_size(int p_size);
    Color get_outline_color() const;
    void set_outline_color(Color p_color);
    int get_size() const;
    void set_size(int p_size);
    int get_spacing(int p_type) const;
    void set_spacing(int p_type, int p_value);
    bool get_use_filter() const;
    void set_use_filter(bool p_enable);
    bool get_use_mipmaps() const;
    void set_use_mipmaps(bool p_enable);

    int get_fallback_count() const;
    Ref<DynamicFontData> get_fallback(int p_idx) const;
    void set_fallback(int p_idx, const Ref<DynamicFontData>& p_data);
    void add_fallback(const Ref<DynamicFontData>& p_data);
    void remove_fallback(int p_idx);

    String get_available_chars() const;
    SelfList<DynamicFont> font_list{this};

    static Mutex dynamic_font_mutex;
    static SelfList<DynamicFont>::List* dynamic_fonts;

    static void initialize_dynamic_fonts();
    static void finish_dynamic_fonts();
    static void update_oversampling();

protected:
    bool _get(const StringName& p_name, Variant& r_ret) const;
    bool _set(const StringName& p_name, const Variant& p_value);
    void _get_property_list(List<PropertyInfo>* p_list) const;
    void _reload_cache(const char* p_triggering_property = "");
    static void _bind_methods();

private:
    Ref<DynamicFontData> data;
    Ref<DynamicFontAtSize> data_at_size;
    Ref<DynamicFontAtSize> outline_data_at_size;

    Vector<Ref<DynamicFontData>> fallbacks;
    Vector<Ref<DynamicFontAtSize>> fallback_data_at_size;
    Vector<Ref<DynamicFontAtSize>> fallback_outline_data_at_size;

    DynamicFontData::CacheID cache_id;
    DynamicFontData::CacheID outline_cache_id;

    Color outline_color;

    int spacing_top;
    int spacing_bottom;
    int spacing_char;
    int spacing_space;
    bool valid;
};

VARIANT_ENUM_CAST(DynamicFont::SpacingType);

// #endif // MODULE_FREETYPE_ENABLED

#endif // DYNAMIC_FONT_H
