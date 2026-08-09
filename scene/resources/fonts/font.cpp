// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "font.h"

#include "core/io/resource_loader.h"
#include "core/method_bind_ext.gen.inc"
#include "core/os/file_access.h"

void Font::draw_halign(
    RID p_canvas_item,
    const Point2& p_pos,
    HAlign p_align,
    float p_width,
    const String& p_text,
    const Color& p_modulate,
    const Color& p_outline_modulate
) const {
    float length = get_string_size(p_text).width;
    if (length >= p_width) {
        draw(
            p_canvas_item,
            p_pos,
            p_text,
            p_modulate,
            p_width,
            p_outline_modulate
        );
        return;
    }

    float ofs = 0.f;
    switch (p_align) {
        case HALIGN_LEFT: {
            ofs = 0;
        } break;
        case HALIGN_CENTER: {
            ofs = Math::floor((p_width - length) / 2.0);
        } break;
        case HALIGN_RIGHT: {
            ofs = p_width - length;
        } break;
        default: {
            ERR_PRINT("Unknown halignment type");
        } break;
    }
    draw(
        p_canvas_item,
        p_pos + Point2(ofs, 0),
        p_text,
        p_modulate,
        p_width,
        p_outline_modulate
    );
}

void Font::draw(
    RID p_canvas_item,
    const Point2& p_pos,
    const String& p_text,
    const Color& p_modulate,
    int p_clip_w,
    const Color& p_outline_modulate
) const {
    Vector2 ofs;

    int chars_drawn   = 0;
    bool with_outline = has_outline();
    for (int i = 0; i < p_text.length(); i++) {
        int width = get_char_size(p_text[i]).width;

        if (p_clip_w >= 0 && (ofs.x + width) > p_clip_w) {
            break; // clip
        }

        ofs.x += draw_char(
            p_canvas_item,
            p_pos + ofs,
            p_text[i],
            p_text[i + 1],
            with_outline ? p_outline_modulate : p_modulate,
            with_outline
        );
        ++chars_drawn;
    }

    if (has_outline()) {
        ofs = Vector2(0, 0);
        for (int i = 0; i < chars_drawn; i++) {
            ofs.x += draw_char(
                p_canvas_item,
                p_pos + ofs,
                p_text[i],
                p_text[i + 1],
                p_modulate,
                false
            );
        }
    }
}

void Font::update_changes() {
    emit_changed();
}

void Font::_bind_methods() {
    ClassDB::bind_method(
        D_METHOD(
            "draw",
            "canvas_item",
            "position",
            "string",
            "modulate",
            "clip_w",
            "outline_modulate"
        ),
        &Font::draw,
        DEFVAL(Color(1, 1, 1)),
        DEFVAL(-1),
        DEFVAL(Color(1, 1, 1))
    );
    ClassDB::bind_method(D_METHOD("get_ascent"), &Font::get_ascent);
    ClassDB::bind_method(D_METHOD("get_descent"), &Font::get_descent);
    ClassDB::bind_method(D_METHOD("get_height"), &Font::get_height);
    ClassDB::bind_method(
        D_METHOD("is_distance_field_hint"),
        &Font::is_distance_field_hint
    );
    ClassDB::bind_method(
        D_METHOD("get_char_size", "char", "next"),
        &Font::get_char_size,
        DEFVAL(0)
    );
    ClassDB::bind_method(
        D_METHOD("get_string_size", "string"),
        &Font::get_string_size
    );
    ClassDB::bind_method(
        D_METHOD("get_wordwrap_string_size", "string", "width"),
        &Font::get_wordwrap_string_size
    );
    ClassDB::bind_method(D_METHOD("has_outline"), &Font::has_outline);
    ClassDB::bind_method(
        D_METHOD(
            "draw_char",
            "canvas_item",
            "position",
            "char",
            "next",
            "modulate",
            "outline"
        ),
        &Font::draw_char,
        DEFVAL(-1),
        DEFVAL(Color(1, 1, 1)),
        DEFVAL(false)
    );
    ClassDB::bind_method(D_METHOD("update_changes"), &Font::update_changes);
}

Font::Font() {}

Size2 Font::get_string_size(const String& p_string) const {
    float w = 0;

    int l = p_string.length();
    if (l == 0) {
        return Size2(0, get_height());
    }
    const CharType* sptr = &p_string[0];

    for (int i = 0; i < l; i++) {
        w += get_char_size(sptr[i], sptr[i + 1]).width;
    }

    return Size2(w, get_height());
}

Size2 Font::get_wordwrap_string_size(const String& p_string, float p_width)
    const {
    ERR_FAIL_COND_V(p_width <= 0, Vector2(0, get_height()));

    int l = p_string.length();
    if (l == 0) {
        return Size2(p_width, get_height());
    }

    float line_w         = 0;
    float h              = 0;
    float space_w        = get_char_size(' ').width;
    Vector<String> lines = p_string.split("\n");
    for (int i = 0; i < lines.size(); i++) {
        h                    += get_height();
        String t              = lines[i];
        line_w                = 0;
        Vector<String> words  = t.split(" ");
        for (int j = 0; j < words.size(); j++) {
            line_w += get_string_size(words[j]).x;
            if (line_w > p_width) {
                h      += get_height();
                line_w  = get_string_size(words[j]).x;
            } else {
                line_w += space_w;
            }
        }
    }

    return Size2(p_width, h);
}
