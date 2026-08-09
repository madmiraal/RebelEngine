// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef BITMAP_FONT_H
#define BITMAP_FONT_H

#include "core/io/resource_loader.h"
#include "scene/resources/fonts/font.h"
#include "scene/resources/texture.h"

class BitmapFont : public Font {
    GDCLASS(BitmapFont, Font);
    RES_BASE_EXTENSION("font");

    Vector<Ref<Texture>> textures;

public:
    struct Character {
        int texture_idx;
        Rect2 rect;
        float v_align;
        float h_align;
        float advance;

        Character() {
            texture_idx = 0;
            v_align     = 0;
        }
    };

    struct KerningPairKey {
        union {
            struct {
                uint32_t A, B;
            };

            uint64_t pair;
        };

        _FORCE_INLINE_ bool operator<(const KerningPairKey& p_r) const {
            return pair < p_r.pair;
        }
    };

private:
    HashMap<CharType, Character> char_map;
    Map<KerningPairKey, int> kerning_map;

    float height;
    float ascent;
    bool distance_field_hint;

    void _set_chars(const PoolVector<int>& p_chars);
    PoolVector<int> _get_chars() const;
    void _set_kernings(const PoolVector<int>& p_kernings);
    PoolVector<int> _get_kernings() const;
    void _set_textures(const Vector<Variant>& p_textures);
    Vector<Variant> _get_textures() const;

    Ref<BitmapFont> fallback;

protected:
    static void _bind_methods();

public:
    Error create_from_fnt(const String& p_file);

    void set_height(float p_height);
    float get_height() const override;

    void set_ascent(float p_ascent);
    float get_ascent() const override;
    float get_descent() const override;

    void add_texture(const Ref<Texture>& p_texture);
    void add_char(
        CharType p_char,
        int p_texture_idx,
        const Rect2& p_rect,
        const Size2& p_align,
        float p_advance = -1
    );

    int get_character_count() const;
    Vector<CharType> get_char_keys() const;
    Character get_character(CharType p_char) const;

    int get_texture_count() const;
    Ref<Texture> get_texture(int p_idx) const;

    void add_kerning_pair(CharType p_A, CharType p_B, int p_kerning);
    int get_kerning_pair(CharType p_A, CharType p_B) const;
    Vector<KerningPairKey> get_kerning_pair_keys() const;

    Size2 get_char_size(CharType p_char, CharType p_next = 0) const override;

    void set_fallback(const Ref<BitmapFont>& p_fallback);
    Ref<BitmapFont> get_fallback() const;

    void clear();

    void set_distance_field_hint(bool p_distance_field);
    bool is_distance_field_hint() const override;

    float draw_char(
        RID p_canvas_item,
        const Point2& p_pos,
        CharType p_char,
        CharType p_next         = 0,
        const Color& p_modulate = Color(1, 1, 1),
        bool p_outline          = false
    ) const override;

    BitmapFont();
    ~BitmapFont() override;
};

#endif // BITMAP_FONT_H
