// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef FONT_H
#define FONT_H

#include "core/resource.h"

class Font : public Resource {
    GDCLASS(Font, Resource);

public:
    virtual float get_ascent() const            = 0;
    virtual float get_descent() const           = 0;
    virtual float get_height() const            = 0;
    virtual bool is_distance_field_hint() const = 0;

    virtual bool has_outline() const {
        return false;
    }

    virtual Size2 get_char_size(CharType character, CharType next_character = 0)
        const = 0;

    virtual float draw_char(
        RID canvas_item,
        const Point2& position,
        CharType character,
        CharType next_character = 0,
        const Color& color      = Color(1, 1, 1),
        bool has_outline        = false
    ) const = 0;

    Size2 get_string_size(const String& string) const;
    Size2 get_wordwrap_string_size(const String& string, float wrap_width)
        const;

    void draw(
        RID canvas_item,
        const Point2& position,
        const String& text,
        const Color& color         = Color(1, 1, 1),
        float clip_width           = -1,
        const Color& outline_color = Color(1, 1, 1)
    ) const;

    void draw_horizontal_align(
        RID canvas_item,
        const Point2& position,
        HAlign horizontal_alignment,
        float clip_width,
        const String& text,
        const Color& color         = Color(1, 1, 1),
        const Color& outline_color = Color(1, 1, 1)
    ) const;

    void update_changes();

protected:
    static void _bind_methods();
};

#endif // FONT_H
