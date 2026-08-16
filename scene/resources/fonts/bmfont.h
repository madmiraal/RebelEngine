// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef BMFONT_H
#define BMFONT_H

#include "scene/resources/fonts/font.h"
#include "scene/resources/texture.h"

class BitmapFont : public Font {
    GDCLASS(BitmapFont, Font);
    RES_BASE_EXTENSION("font");

public:
    struct CharacterData {
        int texture_index = 0;
        Rect2i bounding_box;
        Point2i offset;
        int advance = -1;
    };

    struct KerningPairKey {
        union {
            struct {
                uint32_t first_character, second_character;
            };

            uint64_t pair;
        };

        _FORCE_INLINE_ bool operator<(const KerningPairKey& other) const {
            return pair < other.pair;
        }
    };

    float get_ascent() const override;
    float get_descent() const override;
    float get_height() const override;
    bool is_distance_field_hint() const override;
    Size2 get_char_size(CharType character, CharType next_character = 0)
        const override;
    float draw_char(
        RID canvas_item,
        const Point2& position,
        CharType character,
        CharType next_character = 0,
        const Color& color      = Color(1, 1, 1),
        bool has_outline        = false
    ) const override;

    Error create_from_fnt(const String& font_filename);
    void clear();

    void set_ascent(int new_ascent);
    void set_height(int new_height);
    void set_distance_field_hint(bool new_distance_field_hint);

    Ref<BitmapFont> get_fallback() const;
    void set_fallback(const Ref<BitmapFont>& new_fallback);

    void add_char(
        CharType new_character,
        int texture_index,
        const Rect2& bounding_box,
        const Point2& offset,
        int advance = -1
    );

    int get_kerning_pair(CharType first_character, CharType second_character)
        const;
    void add_kerning_pair(
        CharType first_character,
        CharType second_character,
        int kerning
    );

    int get_texture_count() const;
    Ref<Texture> get_texture(int index) const;
    void add_texture(const Ref<Texture>& new_texture);

protected:
    static void _bind_methods();

private:
    HashMap<CharType, CharacterData> characters;
    Map<KerningPairKey, int> kernings;
    Vector<Ref<Texture>> textures;
    Ref<BitmapFont> fallback;

    int ascent               = 0;
    int height               = 0;
    bool distance_field_hint = false;
};

#endif // BMFONT_H
