// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "font.h"

#include "core/io/resource_loader.h"
#include "core/method_bind_ext.gen.inc"
#include "core/os/file_access.h"

Size2 Font::get_string_size(const String& string) const {
    const float height        = get_height();
    const int character_count = string.length();
    if (character_count == 0) {
        return {0, height};
    }
    const CharType* character = &string[0];
    float width               = 0;
    for (int i = 0; i < character_count; i++) {
        width += get_char_size(character[i], character[i + 1]).width;
    }
    return {width, height};
}

Size2 Font::get_wordwrap_string_size(
    const String& string,
    const float wrap_width
) const {
    const float line_height = get_height();
    ERR_FAIL_COND_V(wrap_width <= 0, Vector2(0, line_height));
    if (string.length() == 0) {
        return {wrap_width, line_height};
    }

    const float space_width    = get_char_size(' ').width;
    const Vector<String> lines = string.split("\n");
    float height               = 0;
    for (int line_index = 0; line_index < lines.size(); line_index++) {
        const String& line          = lines[line_index];
        const Vector<String> words  = line.split(" ");
        float line_width            = 0;
        height                     += line_height;
        for (int word_index = 0; word_index < words.size(); word_index++) {
            const String& word      = words[word_index];
            const float word_width  = get_string_size(word).x;
            line_width             += word_width;
            if (line_width > wrap_width) {
                height     += line_height;
                line_width  = word_width;
            } else {
                line_width += space_width;
            }
        }
    }

    return {wrap_width, height};
}

void Font::draw(
    const RID canvas_item,
    const Point2& position,
    const String& text,
    const Color& color,
    const float clip_width,
    const Color& outline_color
) const {
    const bool clipping = clip_width >= 0;
    Vector2 character_offset;
    int characters_drawn    = 0;
    const bool draw_outline = has_outline();
    for (int i = 0; i < text.length(); i++) {
        const float character_width = get_char_size(text[i]).width;
        const float total_width     = character_offset.x + character_width;
        if (clipping && total_width > clip_width) {
            break;
        }

        character_offset.x += draw_char(
            canvas_item,
            position + character_offset,
            text[i],
            text[i + 1],
            draw_outline ? outline_color : color,
            draw_outline
        );
        ++characters_drawn;
    }

    if (draw_outline) {
        character_offset = Vector2(0, 0);
        for (int i = 0; i < characters_drawn; i++) {
            character_offset.x += draw_char(
                canvas_item,
                position + character_offset,
                text[i],
                text[i + 1],
                color,
                false
            );
        }
    }
}

void Font::draw_horizontal_align(
    const RID canvas_item,
    const Point2& position,
    const HAlign horizontal_alignment,
    const float clip_width,
    const String& text,
    const Color& color,
    const Color& outline_color
) const {
    const float text_width  = get_string_size(text).width;
    float horizontal_offset = 0;
    if (text_width < clip_width) {
        switch (horizontal_alignment) {
            case HALIGN_LEFT: {
                horizontal_offset = 0;
            } break;
            case HALIGN_CENTER: {
                horizontal_offset = Math::floor((clip_width - text_width) / 2);
            } break;
            case HALIGN_RIGHT: {
                horizontal_offset = clip_width - text_width;
            } break;
        }
    }

    draw(
        canvas_item,
        position + Point2(horizontal_offset, 0),
        text,
        color,
        clip_width,
        outline_color
    );
}

void Font::update_changes() {
    emit_changed();
}

void Font::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_ascent"), &Font::get_ascent);
    ClassDB::bind_method(D_METHOD("get_descent"), &Font::get_descent);
    ClassDB::bind_method(D_METHOD("get_height"), &Font::get_height);
    ClassDB::bind_method(
        D_METHOD("is_distance_field_hint"),
        &Font::is_distance_field_hint
    );
    ClassDB::bind_method(D_METHOD("has_outline"), &Font::has_outline);
    ClassDB::bind_method(
        D_METHOD("get_char_size", "character", "next_character"),
        &Font::get_char_size,
        DEFVAL(0)
    );
    ClassDB::bind_method(
        D_METHOD(
            "draw_char",
            "canvas_item",
            "position",
            "character",
            "next_character",
            "color",
            "has_outline"
        ),
        &Font::draw_char,
        DEFVAL(-1),
        DEFVAL(Color(1, 1, 1)),
        DEFVAL(false)
    );
    ClassDB::bind_method(
        D_METHOD("get_string_size", "string"),
        &Font::get_string_size
    );
    ClassDB::bind_method(
        D_METHOD("get_wordwrap_string_size", "string", "wrap_width"),
        &Font::get_wordwrap_string_size
    );
    ClassDB::bind_method(
        D_METHOD(
            "draw",
            "canvas_item",
            "position",
            "string",
            "color",
            "clip_width",
            "outline_color"
        ),
        &Font::draw,
        DEFVAL(Color(1, 1, 1)),
        DEFVAL(-1),
        DEFVAL(Color(1, 1, 1))
    );
    ClassDB::bind_method(D_METHOD("update_changes"), &Font::update_changes);
}
