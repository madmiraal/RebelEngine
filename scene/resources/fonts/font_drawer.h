// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef FONT_DRAWER_H
#define FONT_DRAWER_H

#include "core/reference.h"
#include "scene/resources/fonts/font.h"

// Helper class to that draws outlines immediately and draws characters in its
// destructor.
class FontDrawer {
    const Ref<Font>& font;
    Color outline_color;
    bool has_outline;

    struct PendingDraw {
        RID canvas_item;
        Point2 pos;
        CharType chr;
        CharType next;
        Color modulate;
    };

    Vector<PendingDraw> pending_draws;

public:
    FontDrawer(const Ref<Font>& p_font, const Color& p_outline_color) :
        font(p_font),
        outline_color(p_outline_color) {
        has_outline = p_font->has_outline();
    }

    float draw_char(
        RID p_canvas_item,
        const Point2& p_pos,
        CharType p_char,
        CharType p_next         = 0,
        const Color& p_modulate = Color(1, 1, 1)
    ) {
        if (has_outline) {
            PendingDraw draw =
                {p_canvas_item, p_pos, p_char, p_next, p_modulate};
            pending_draws.push_back(draw);
        }
        return font->draw_char(
            p_canvas_item,
            p_pos,
            p_char,
            p_next,
            has_outline ? outline_color : p_modulate,
            has_outline
        );
    }

    ~FontDrawer() {
        for (int i = 0; i < pending_draws.size(); ++i) {
            const PendingDraw& draw = pending_draws[i];
            font->draw_char(
                draw.canvas_item,
                draw.pos,
                draw.chr,
                draw.next,
                draw.modulate,
                false
            );
        }
    }
};

#endif // FONT_DRAWER_H
