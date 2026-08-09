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

protected:
    static void _bind_methods();

public:
    virtual float get_height() const = 0;

    virtual float get_ascent() const  = 0;
    virtual float get_descent() const = 0;

    virtual Size2 get_char_size(CharType p_char, CharType p_next = 0) const = 0;
    Size2 get_string_size(const String& p_string) const;
    Size2 get_wordwrap_string_size(const String& p_string, float p_width) const;

    virtual bool is_distance_field_hint() const = 0;

    void draw(
        RID p_canvas_item,
        const Point2& p_pos,
        const String& p_text,
        const Color& p_modulate         = Color(1, 1, 1),
        int p_clip_w                    = -1,
        const Color& p_outline_modulate = Color(1, 1, 1)
    ) const;
    void draw_halign(
        RID p_canvas_item,
        const Point2& p_pos,
        HAlign p_align,
        float p_width,
        const String& p_text,
        const Color& p_modulate         = Color(1, 1, 1),
        const Color& p_outline_modulate = Color(1, 1, 1)
    ) const;

    virtual bool has_outline() const {
        return false;
    }

    virtual float draw_char(
        RID p_canvas_item,
        const Point2& p_pos,
        CharType p_char,
        CharType p_next         = 0,
        const Color& p_modulate = Color(1, 1, 1),
        bool p_outline          = false
    ) const = 0;

    void update_changes();
    Font();
};

#endif // FONT_H
