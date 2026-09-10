// SPDX-FileCopyrightText: 2026 Rebel Engine contributors
//
// SPDX-License-Identifier: MIT

#ifndef DYNAMIC_FONT_SETTINGS_H
#define DYNAMIC_FONT_SETTINGS_H

struct DynamicFontSettings {
    int font_size         = 16;
    int outline_thickness = 0;
    bool use_filter       = false;
    bool use_mipmaps      = false;

    constexpr bool operator<(const DynamicFontSettings& other) const {
        if (font_size != other.font_size) {
            return font_size < other.font_size;
        }
        if (outline_thickness != other.outline_thickness) {
            return outline_thickness < other.outline_thickness;
        }
        if (use_filter != other.use_filter) {
            return use_filter < other.use_filter;
        }
        return use_mipmaps < other.use_mipmaps;
    }
};

#endif // DYNAMIC_FONT_SETTINGS_H
