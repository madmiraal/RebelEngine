//
// Created by marcel on 03/09/2026.
//

#ifndef FONT_DRAWER_H
#define FONT_DRAWER_H

#include "core/color.h"
#include "core/reference.h"
#include "core/vector.h"
#include "scene/resources/fonts/font.h"

// Helper class to that draws outlines immediately and draws characters in its
// destructor.
class FontDrawer {
public:
    FontDrawer(const Ref<Font>& p_font, const Color& p_outline_color);
    ~FontDrawer();

    float draw_char(
        RID p_canvas_item,
        const Point2& p_pos,
        CharType p_char,
        CharType p_next         = 0,
        const Color& p_modulate = Color(1, 1, 1)
    );

private:
    struct PendingDraw {
        RID canvas_item;
        Point2 pos;
        CharType chr;
        CharType next;
        Color modulate;
    };

    Vector<PendingDraw> pending_draws;
    const Ref<Font>& font;
    Color outline_color;
    bool has_outline;
};

#endif // FONT_DRAWER_H
