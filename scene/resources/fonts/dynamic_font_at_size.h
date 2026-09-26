// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef DYNAMIC_FONT_AT_SIZE_H
#define DYNAMIC_FONT_AT_SIZE_H

#include "modules/modules_enabled.gen.h"
#ifdef MODULE_FREETYPE_ENABLED

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
        Vector<int> y_offsets;
        int texture_size = 0;
    };

    DynamicFontAtSize(
        const Ref<DynamicFontData>& font_data,
        const DynamicFontSettings& font_settings
    );
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

private:
    FT_Face ft_face       = nullptr;
    FT_Stroker ft_stroker = nullptr;

    Ref<DynamicFontData> font_data;
    DynamicFontSettings font_settings;
    mutable Vector<CharacterTexture> textures_cache;
    mutable HashMap<CharType, CharacterData> character_data_cache;

    uint32_t texture_flags   = 0;
    float ascent             = 1;
    float descent            = 1;
    float color_font_scaling = 1;
    bool valid               = false;

    Pair<const CharacterData&, const Ref<DynamicFontAtSize>>
    get_character_data_and_font(
        CharType character,
        const Vector<Ref<DynamicFontAtSize>>& fallbacks
    ) const;
    const CharacterData& get_character_data(CharType character) const;
    CharacterData create_character_data(CharType character) const;
    CharacterData create_outline_character(CharType character) const;
};

#endif // MODULE_FREETYPE_ENABLED

#endif // DYNAMIC_FONT_AT_SIZE_H
