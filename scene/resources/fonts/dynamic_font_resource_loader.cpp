// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "dynamic_font_resource_loader.h"

#include "scene/resources/fonts/dynamic_font_data.h"

RES DynamicFontResourceLoader::load(
    const String& path,
    const String&,
    Error* error
) {
    Ref<DynamicFontData> dynamic_font_data;
    dynamic_font_data.instance();
    dynamic_font_data->set_font_path(path);
    if (error) {
        *error = OK;
    }
    return dynamic_font_data;
}

void DynamicFontResourceLoader::get_recognized_extensions(
    List<String>* extensions
) const {
    extensions->push_back("ttf");
    extensions->push_back("otf");
    extensions->push_back("woff");
    // WOFF2 requires a Brotli decompression library.
}

bool DynamicFontResourceLoader::handles_type(const String& type) const {
    return type == "DynamicFontData";
}

String DynamicFontResourceLoader::get_resource_type(const String& path) const {
    const String extension = path.get_extension().to_lower();
    if (extension == "ttf" || extension == "otf" || extension == "woff") {
        return "DynamicFontData";
    }
    return {};
}
