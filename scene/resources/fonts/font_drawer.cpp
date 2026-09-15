// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "font_drawer.h"

FontDrawer::FontDrawer(const Ref<Font>& font, const Color& outline_color) :
    font(font),
    outline_color(outline_color) {}

FontDrawer::~FontDrawer() {
    for (int i = 0; i < pending_draws.size(); ++i) {
        const PendingDraw& pending_draw = pending_draws[i];
        font->draw_char(
            pending_draw.canvas_item,
            pending_draw.position,
            pending_draw.character,
            pending_draw.next_character,
            pending_draw.color,
            false
        );
    }
}

float FontDrawer::draw_char(
    const RID canvas_item,
    const Point2& position,
    CharType character,
    CharType next_character,
    const Color& color
) {
    const bool has_outline = font->has_outline();
    if (has_outline) {
        const PendingDraw draw =
            {canvas_item, position, character, next_character, color};
        pending_draws.push_back(draw);
    }
    return font->draw_char(
        canvas_item,
        position,
        character,
        next_character,
        has_outline ? outline_color : color,
        has_outline
    );
}
