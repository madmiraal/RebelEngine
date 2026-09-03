// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef DYNAMIC_FONT_H
#define DYNAMIC_FONT_H

#include "modules/modules_enabled.gen.h" // For freetype.
#ifdef MODULE_FREETYPE_ENABLED

#include "core/io/resource_loader.h"
#include "core/os/mutex.h"
#include "dynamic_font_data.h"
#include "scene/resources/fonts/font.h"

class DynamicFontAtSize;

class DynamicFont : public Font {
    GDCLASS(DynamicFont, Font);

public:
    enum SpacingType {
        SPACING_TOP,
        SPACING_BOTTOM,
        SPACING_CHAR,
        SPACING_SPACE
    };

private:
    Ref<DynamicFontData> data;
    Ref<DynamicFontAtSize> data_at_size;
    Ref<DynamicFontAtSize> outline_data_at_size;

    Vector<Ref<DynamicFontData>> fallbacks;
    Vector<Ref<DynamicFontAtSize>> fallback_data_at_size;
    Vector<Ref<DynamicFontAtSize>> fallback_outline_data_at_size;

    DynamicFontData::CacheID cache_id;
    DynamicFontData::CacheID outline_cache_id;

    bool valid;
    int spacing_top;
    int spacing_bottom;
    int spacing_char;
    int spacing_space;

    Color outline_color;

protected:
    void _reload_cache(const char* p_triggering_property = "");

    bool _set(const StringName& p_name, const Variant& p_value);
    bool _get(const StringName& p_name, Variant& r_ret) const;
    void _get_property_list(List<PropertyInfo>* p_list) const;

    static void _bind_methods();

public:
    void set_font_data(const Ref<DynamicFontData>& p_data);
    Ref<DynamicFontData> get_font_data() const;

    void set_size(int p_size);
    int get_size() const;

    void set_outline_size(int p_size);
    int get_outline_size() const;

    void set_outline_color(Color p_color);
    Color get_outline_color() const;

    bool get_use_mipmaps() const;
    void set_use_mipmaps(bool p_enable);

    bool get_use_filter() const;
    void set_use_filter(bool p_enable);

    int get_spacing(int p_type) const;
    void set_spacing(int p_type, int p_value);

    void add_fallback(const Ref<DynamicFontData>& p_data);
    void set_fallback(int p_idx, const Ref<DynamicFontData>& p_data);
    int get_fallback_count() const;
    Ref<DynamicFontData> get_fallback(int p_idx) const;
    void remove_fallback(int p_idx);

    float get_height() const override;

    float get_ascent() const override;
    float get_descent() const override;

    Size2 get_char_size(CharType p_char, CharType p_next = 0) const override;
    String get_available_chars() const;

    bool is_distance_field_hint() const override;

    bool has_outline() const override;

    float draw_char(
        RID p_canvas_item,
        const Point2& p_pos,
        CharType p_char,
        CharType p_next         = 0,
        const Color& p_modulate = Color(1, 1, 1),
        bool p_outline          = false
    ) const override;

    SelfList<DynamicFont> font_list{this};

    static Mutex dynamic_font_mutex;
    static SelfList<DynamicFont>::List* dynamic_fonts;

    static void initialize_dynamic_fonts();
    static void finish_dynamic_fonts();
    static void update_oversampling();

    DynamicFont();
    ~DynamicFont() override;
};

VARIANT_ENUM_CAST(DynamicFont::SpacingType);

#endif // MODULE_FREETYPE_ENABLED

#endif // DYNAMIC_FONT_H
