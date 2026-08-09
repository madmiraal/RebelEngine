// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "bitmap_font_resource_format_loader.h"

#include "scene/resources/fonts/bitmap_font.h"

RES ResourceFormatLoaderBMFont::load(
    const String& p_path,
    const String& p_original_path,
    Error* r_error
) {
    if (r_error) {
        *r_error = ERR_FILE_CANT_OPEN;
    }

    Ref<BitmapFont> font;
    font.instance();

    Error err = font->create_from_fnt(p_path);

    if (err) {
        if (r_error) {
            *r_error = err;
        }
        return RES();
    }

    return font;
}

void ResourceFormatLoaderBMFont::get_recognized_extensions(
    List<String>* p_extensions
) const {
    p_extensions->push_back("fnt");
}

bool ResourceFormatLoaderBMFont::handles_type(const String& p_type) const {
    return (p_type == "BitmapFont");
}

String ResourceFormatLoaderBMFont::get_resource_type(const String& p_path
) const {
    String el = p_path.get_extension().to_lower();
    if (el == "fnt") {
        return "BitmapFont";
    }
    return "";
}
