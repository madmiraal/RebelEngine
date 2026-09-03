//
// Created by marcel on 03/09/2026.
//

#include "font_drawer.h"

FontDrawer::FontDrawer(const Ref<Font>& font, const Color& outline_color) :
    font(font),
    outline_color(outline_color) {
    has_outline = font->has_outline();
}

FontDrawer::~FontDrawer() {
    for (int i = 0; i < pending_draws.size(); ++i) {
        const PendingDraw& draw = pending_draws[i];
        font->draw_char(
            draw.canvas_item,
            draw.position,
            draw.character,
            draw.next_character,
            draw.color,
            false
        );
    }
}

float FontDrawer::draw_char(
    const RID canvas_item,
    const Point2& position,
    const CharType character,
    const CharType next_character,
    const Color& color
) {
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
