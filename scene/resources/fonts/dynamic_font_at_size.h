// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef DYNAMIC_FONT_AT_SIZE_H
#define DYNAMIC_FONT_AT_SIZE_H

#include "core/os/thread_safe.h"
#include "core/pair.h"
#include "scene/resources/fonts/dynamic_font_data.h"
#include "scene/resources/texture.h"

#include <ft2build.h>
#include FT_FREETYPE_H

class DynamicFontAtSize : public Reference {
    GDCLASS(DynamicFontAtSize, Reference);

    _THREAD_SAFE_CLASS_

    FT_Library library; /* handle to library     */
    FT_Face face;       /* handle to face object */
    FT_StreamRec stream;

    float ascent;
    float descent;
    float linegap;
    float rect_margin;
    float oversampling;
    float scale_color_font;

    uint32_t texture_flags;

    bool valid;

    struct CharTexture {
        PoolVector<uint8_t> imgdata;
        int texture_size;
        Vector<int> offsets;
        Ref<ImageTexture> texture;
    };

    Vector<CharTexture> textures;

    struct Character {
        bool found;
        int texture_idx;
        Rect2 rect;
        Rect2 rect_uv;
        float v_align;
        float h_align;
        float advance;

        Character() {
            texture_idx = 0;
            v_align     = 0;
        }

        static Character not_found();
    };

    struct TexturePosition {
        int index;
        int x;
        int y;
    };

    const Pair<const Character*, DynamicFontAtSize*> _find_char_with_font(
        CharType p_char,
        const Vector<Ref<DynamicFontAtSize>>& p_fallbacks
    ) const;
    Character _make_outline_char(CharType p_char);
    float _get_kerning_advance(
        const DynamicFontAtSize* font,
        CharType p_char,
        CharType p_next
    ) const;
    TexturePosition _find_texture_pos_for_glyph(
        int p_color_size,
        Image::Format p_image_format,
        int p_width,
        int p_height
    );
    Character _bitmap_to_character(
        FT_Bitmap bitmap,
        int yofs,
        int xofs,
        float advance
    );

    HashMap<CharType, Character> char_map;

    _FORCE_INLINE_ void _update_char(CharType p_char);

    friend class DynamicFontData;
    Ref<DynamicFontData> font;
    DynamicFontData::CacheID id;

    Error _load();

public:
    static float font_oversampling;

    float get_height() const;

    float get_ascent() const;
    float get_descent() const;

    Size2 get_char_size(
        CharType p_char,
        CharType p_next,
        const Vector<Ref<DynamicFontAtSize>>& p_fallbacks
    ) const;
    String get_available_chars() const;

    float draw_char(
        RID p_canvas_item,
        const Point2& p_pos,
        CharType p_char,
        CharType p_next,
        const Color& p_modulate,
        const Vector<Ref<DynamicFontAtSize>>& p_fallbacks,
        bool p_advance_only = false,
        bool p_outline      = false
    ) const;

    void set_texture_flags(uint32_t p_flags);
    void update_oversampling();

    DynamicFontAtSize();
    ~DynamicFontAtSize() override;
};

#endif // DYNAMIC_FONT_AT_SIZE_H
