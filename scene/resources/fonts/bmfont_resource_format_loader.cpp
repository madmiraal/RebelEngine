// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "bmfont_resource_format_loader.h"

#include "scene/resources/fonts/bmfont.h"

void ResourceFormatLoaderBMFont::get_recognized_extensions(
    List<String>* extensions
) const {
    extensions->push_back("fnt");
}

String ResourceFormatLoaderBMFont::get_resource_type(const String& path) const {
    String el = path.get_extension().to_lower();
    if (el == "fnt") {
        return "BMFont";
    }
    return "";
}

bool ResourceFormatLoaderBMFont::handles_type(const String& type_name) const {
    return (type_name == "BMFont");
}

RES ResourceFormatLoaderBMFont::load(
    const String& p_path,
    const String&,
    Error* error
) {
    Ref<BitmapFont> font;
    font.instance();
    Error create_error = font->create_from_fnt(p_path);
    if (create_error) {
        if (error) {
            *error = create_error;
        }
        return {};
    }
    if (error) {
        *error = OK;
    }
    return font;
}
