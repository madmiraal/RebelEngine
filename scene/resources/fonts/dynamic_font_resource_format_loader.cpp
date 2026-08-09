// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "dynamic_font_resource_format_loader.h"

#include "scene/resources/fonts/dynamic_font_data.h"

RES ResourceFormatLoaderDynamicFont::load(
    const String& p_path,
    const String& p_original_path,
    Error* r_error
) {
    if (r_error) {
        *r_error = ERR_FILE_CANT_OPEN;
    }

    Ref<DynamicFontData> dfont;
    dfont.instance();
    dfont->set_font_path(p_path);

    if (r_error) {
        *r_error = OK;
    }

    return dfont;
}

void ResourceFormatLoaderDynamicFont::get_recognized_extensions(
    List<String>* p_extensions
) const {
    p_extensions->push_back("ttf");
    p_extensions->push_back("otf");
    // Only WOFF1 is supported as WOFF2 requires a Brotli decompression library
    // to be linked.
    p_extensions->push_back("woff");
}

bool ResourceFormatLoaderDynamicFont::handles_type(const String& p_type) const {
    return (p_type == "DynamicFontData");
}

String ResourceFormatLoaderDynamicFont::get_resource_type(const String& p_path
) const {
    String el = p_path.get_extension().to_lower();
    if (el == "ttf" || el == "otf" || el == "woff") {
        return "DynamicFontData";
    }
    return "";
}
