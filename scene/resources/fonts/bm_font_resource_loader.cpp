// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "bm_font_resource_loader.h"

#include "scene/resources/fonts/bitmap_font.h"

RES BMFontResourceLoader::load(
    const String& path,
    const String&,
    Error* error
) {
    Ref<BitmapFont> font;
    font.instance();
    const Error create_error = font->create_from_fnt(path);
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

void BMFontResourceLoader::get_recognized_extensions(List<String>* extensions
) const {
    extensions->push_back("fnt");
}

bool BMFontResourceLoader::handles_type(const String& type) const {
    return type == "BitmapFont";
}

String BMFontResourceLoader::get_resource_type(const String& path) const {
    const String& extension = path.get_extension().to_lower();
    if (extension == "fnt") {
        return "BitmapFont";
    }
    return {};
}
