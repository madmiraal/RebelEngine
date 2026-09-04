// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef DYNAMIC_FONT_H
#define DYNAMIC_FONT_H

#include "modules/modules_enabled.gen.h" // For freetype.
#ifdef MODULE_FREETYPE_ENABLED

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

    SelfList<DynamicFont> font_list{this};

    static SelfList<DynamicFont>::List* dynamic_fonts;
    static Mutex dynamic_font_mutex;

    DynamicFont();
    ~DynamicFont() override;

    float get_ascent() const override;
    float get_descent() const override;
    float get_height() const override;
    bool is_distance_field_hint() const override;
    bool has_outline() const override;
    Size2 get_char_size(CharType character, CharType next_character = 0)
        const override;
    float draw_char(
        RID canvas_item,
        const Point2& position,
        CharType character,
        CharType next_character = 0,
        const Color& color      = Color(1, 1, 1),
        bool draw_outline       = false
    ) const override;

    String get_available_chars() const;
    Ref<DynamicFontData> get_font_data() const;
    void set_font_data(const Ref<DynamicFontData>& new_font_data);
    Color get_outline_color() const;
    void set_outline_color(Color new_outline_color);
    int get_outline_size() const;
    void set_outline_size(int new_size);
    int get_size() const;
    void set_size(int new_size);
    int get_spacing(int spacing_type) const;
    void set_spacing(int spacing_type, int new_value);
    bool get_use_filter() const;
    void set_use_filter(bool enabled);
    bool get_use_mipmaps() const;
    void set_use_mipmaps(bool enabled);

    int get_fallback_count() const;
    Ref<DynamicFontData> get_fallback(int index) const;
    void set_fallback(
        int index,
        const Ref<DynamicFontData>& new_fallback_font_data
    );
    void add_fallback(const Ref<DynamicFontData>& new_fallback_font_data);
    void remove_fallback(int index);

    static void initialize_dynamic_fonts();
    static void finish_dynamic_fonts();
    static void update_oversampling();

protected:
    bool _get(const StringName& name, Variant& result) const;
    bool _set(const StringName& name, const Variant& new_value);
    void _get_property_list(List<PropertyInfo>* list) const;
    static void _bind_methods();

private:
    Ref<DynamicFontData> font_data;
    Ref<DynamicFontAtSize> font_at_size;
    Ref<DynamicFontAtSize> outline_font_at_size;

    Vector<Ref<DynamicFontData>> fallback_fonts_data;
    Vector<Ref<DynamicFontAtSize>> fallback_fonts_at_size;
    Vector<Ref<DynamicFontAtSize>> fallback_outline_fonts_at_size;

    DynamicFontData::CacheID cache_id;
    DynamicFontData::CacheID outline_cache_id;

    Color outline_color{1, 1, 1};

    int top_spacing       = 0;
    int bottom_spacing    = 0;
    int character_spacing = 0;
    int space_spacing     = 0;

    void reload_cache(const char* triggering_property = "");
};

VARIANT_ENUM_CAST(DynamicFont::SpacingType);

#endif // MODULE_FREETYPE_ENABLED

#endif // DYNAMIC_FONT_H
