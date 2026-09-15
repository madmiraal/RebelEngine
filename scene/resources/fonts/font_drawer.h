// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef FONT_DRAWER_H
#define FONT_DRAWER_H

#include "core/reference.h"
#include "scene/resources/fonts/font.h"

// Helper class to draw outlines immediately and characters in the destructor.
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

    const Ref<Font>& font;
    Color outline_color;

    Vector<PendingDraw> pending_draws;
};

#endif // FONT_DRAWER_H
