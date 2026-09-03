//
// Created by marcel on 03/09/2026.
//

#ifndef FONT_DRAWER_H
#define FONT_DRAWER_H

#include "core/color.h"
#include "core/reference.h"
#include "core/vector.h"
#include "scene/resources/fonts/font.h"

// Helper that draws outlines immediately and the characters in the destructor.
class FontDrawer {
public:
    FontDrawer(const Ref<Font>& font, const Color& outline_color);
    ~FontDrawer();

    float draw_char(
        RID canvas_item,
        const Point2& position,
        CharType character,
        CharType next_character = 0,
        const Color& color      = Color(1, 1, 1)
    );

private:
    struct PendingDraw {
        RID canvas_item;
        Point2 position;
        CharType character      = 0;
        CharType next_character = 0;
        Color color;
    };

    Vector<PendingDraw> pending_draws;
    const Ref<Font>& font;
    Color outline_color;
    bool has_outline;
};

#endif // FONT_DRAWER_H
