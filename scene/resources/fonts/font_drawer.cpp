//
// Created by marcel on 03/09/2026.
//

#include "font_drawer.h"

FontDrawer::FontDrawer(const Ref<Font>& p_font, const Color& p_outline_color) :
    font(p_font),
    outline_color(p_outline_color) {
    has_outline = p_font->has_outline();
}

FontDrawer::~FontDrawer() {
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

float FontDrawer::draw_char(
    RID p_canvas_item,
    const Point2& p_pos,
    CharType p_char,
    CharType p_next,
    const Color& p_modulate
) {
    if (has_outline) {
        PendingDraw draw = {p_canvas_item, p_pos, p_char, p_next, p_modulate};
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
