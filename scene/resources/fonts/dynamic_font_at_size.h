// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef DYNAMIC_FONT_AT_SIZE_H
#define DYNAMIC_FONT_AT_SIZE_H

#include "core/image.h"
#include "core/os/thread_safe.h"
#include "core/pair.h"
#include "core/reference.h"
#include "scene/resources/fonts/dynamic_font_data.h"
#include "scene/resources/fonts/dynamic_font_settings.h"
#include "scene/resources/texture.h"

class ImageTexture;

class DynamicFontAtSize : public Reference {
    GDCLASS(DynamicFontAtSize, Reference);
    _THREAD_SAFE_CLASS_

public:
    static float font_oversampling;

    DynamicFontAtSize() = default;
    ~DynamicFontAtSize() override;

    float get_ascent() const;
    float get_descent() const;
    float get_height() const;
    Size2 get_char_size(
        CharType character,
        CharType next_character,
        const Vector<Ref<DynamicFontAtSize>>& fallbacks
    ) const;
    float draw_char(
        RID canvas_item,
        const Point2& position,
        CharType character,
        CharType next_character,
        const Color& color,
        const Vector<Ref<DynamicFontAtSize>>& fallbacks,
        bool advance_only = false,
        bool has_outline  = false
    ) const;

    String get_available_chars() const;
    void set_texture_flags(uint32_t new_texture_flags);
    void update_oversampling();

private:
    friend class DynamicFontData;

    struct CharacterData {
        Rect2 rect;
        Rect2 uv_rect;
        int texture_index       = -1;
        float horizontal_offset = 0;
        float vertical_offset   = 0;
        float advance           = 0;
        bool found              = false;
    };

    struct CharacterTexture {
        Ref<ImageTexture> texture;
        PoolVector<unsigned char> image_data;
        Vector<int> offsets;
        int texture_size = 0;
    };

    struct TextureLocation {
        int texture_index = -1;
        int x_offset      = 0;
        int y_offset      = 0;
    };

    FT_Face ft_face = nullptr;

    Ref<DynamicFontData> font_data;
    DynamicFontSettings font_settings;
    mutable Vector<CharacterTexture> textures_cache;
    mutable HashMap<CharType, CharacterData> character_data_cache;

    uint32_t texture_flags = 0;

    float ascent             = 1;
    float descent            = 1;
    float line_gap           = 1;
    float oversampling       = font_oversampling;
    float color_font_scaling = 1;

    bool valid = false;

    Error load();
    Pair<const CharacterData*, const DynamicFontAtSize*>
    get_character_data_and_font(
        CharType character,
        const Vector<Ref<DynamicFontAtSize>>& fallbacks
    ) const;
    const CharacterData* get_character_data(CharType character) const;
    CharacterData create_character_data(CharType character) const;
    CharacterData create_bitmap_character(const FT_GlyphSlot& ft_glyph_slot
    ) const;
    CharacterData create_bitmap_character(const FT_Glyph& ft_glyph) const;
    CharacterData create_bitmap_character(
        const FT_Bitmap& bitmap,
        FT_Int top,
        FT_Int left,
        float ft_glyph_advance
    ) const;
    CharacterData create_outline_character(CharType character) const;
    TextureLocation find_cached_texture_location(
        const Image::Format& image_format,
        int width,
        int height
    ) const;
    TextureLocation get_texture_location(
        const Image::Format& image_format,
        int width,
        int height
    ) const;
    TextureLocation create_new_cached_texture_location(
        const Image::Format& image_format,
        int width,
        int height
    ) const;

    static void draw_texture(
        RID canvas_item,
        const Point2& position,
        const CharacterData* character_data,
        const DynamicFontAtSize* font_at_size,
        const Color& color
    );
};

#endif // DYNAMIC_FONT_AT_SIZE_H
