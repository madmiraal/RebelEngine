// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "dynamic_font_data.h"

#include "core/os/file_access.h"
#include "scene/resources/fonts/dynamic_font_at_size.h"

DynamicFontData::DynamicFontData() {
    const FT_Error ft_error = FT_Init_FreeType(&ft_library);
    if (ft_error) {
        ft_library = nullptr;
        ERR_FAIL_MSG("Error initializing FreeType.");
    }
}

DynamicFontData::~DynamicFontData() {
    if (ft_library) {
        FT_Done_FreeType(ft_library);
    }
}

Error DynamicFontData::initialize() {
    ERR_FAIL_NULL_V_MSG(
        ft_library,
        ERR_CANT_CREATE,
        "FreeType not initialized."
    );
    if (font_path.empty()) {
        if (!font_bytes) {
            ERR_FAIL_V_MSG(ERR_UNCONFIGURED, "No font data found.");
        }
        return OK;
    }
    FileAccess* file_access = FileAccess::open(font_path, FileAccess::READ);
    if (!file_access) {
        ERR_FAIL_V_MSG(
            ERR_CANT_OPEN,
            "Cannot open font file '" + font_path + "'."
        );
    }
    const int length = static_cast<int>(file_access->get_len());
    font_data.resize(length);
    file_access->get_buffer(font_data.ptrw(), length);
    set_font_bytes(font_data.ptr(), length);
    file_access->close();
    memdelete(file_access);

    return OK;
}

bool DynamicFontData::is_antialiased() const {
    return antialiased;
}

void DynamicFontData::set_antialiased(const bool new_antialiased) {
    antialiased = new_antialiased;
}

String DynamicFontData::get_font_path() const {
    return font_path;
}

void DynamicFontData::set_font_path(const String& new_font_path) {
    font_path = new_font_path;
}

void DynamicFontData::set_font_bytes(
    const unsigned char* new_font_bytes,
    const int new_font_bytes_length
) {
    font_bytes        = new_font_bytes;
    font_bytes_length = new_font_bytes_length;
}

DynamicFontData::Hinting DynamicFontData::get_hinting() const {
    return hinting;
}

void DynamicFontData::set_hinting(const Hinting new_hinting) {
    hinting = new_hinting;
}

void DynamicFontData::set_force_auto_hinter(const bool new_force_auto_hinting) {
    force_auto_hinter = new_force_auto_hinting;
}

FT_Library DynamicFontData::get_ft_library() const {
    return ft_library;
}

Ref<DynamicFontAtSize> DynamicFontData::get_font_at_size(
    const DynamicFontSettings& font_settings
) {
    if (font_at_sizes_cache.has(font_settings)) {
        return {font_at_sizes_cache[font_settings]};
    }
    Ref<DynamicFontAtSize> font_at_size;
    font_at_size.instance();
    font_at_size->font_data            = Ref<DynamicFontData>(this);
    font_at_sizes_cache[font_settings] = font_at_size.ptr();
    font_at_size->font_settings        = font_settings;
    font_at_size->load();
    return font_at_size;
}

void DynamicFontData::_bind_methods() {
    ClassDB::bind_method(
        D_METHOD("is_antialiased"),
        &DynamicFontData::is_antialiased
    );
    ClassDB::bind_method(
        D_METHOD("set_antialiased", "antialiased"),
        &DynamicFontData::set_antialiased
    );
    ClassDB::bind_method(
        D_METHOD("get_font_path"),
        &DynamicFontData::get_font_path
    );
    ClassDB::bind_method(
        D_METHOD("set_font_path", "path"),
        &DynamicFontData::set_font_path
    );
    ClassDB::bind_method(
        D_METHOD("get_hinting"),
        &DynamicFontData::get_hinting
    );
    ClassDB::bind_method(
        D_METHOD("set_hinting", "mode"),
        &DynamicFontData::set_hinting
    );

    ADD_PROPERTY(
        PropertyInfo(Variant::BOOL, "antialiased"),
        "set_antialiased",
        "is_antialiased"
    );
    ADD_PROPERTY(
        PropertyInfo(
            Variant::INT,
            "hinting",
            PROPERTY_HINT_ENUM,
            "None,Light,Normal"
        ),
        "set_hinting",
        "get_hinting"
    );

    BIND_ENUM_CONSTANT(HINTING_NONE);
    BIND_ENUM_CONSTANT(HINTING_LIGHT);
    BIND_ENUM_CONSTANT(HINTING_NORMAL);

    // Only WOFF1 is supported.
    // WOFF2 requires a Brotli decompression library.
    ADD_PROPERTY(
        PropertyInfo(
            Variant::STRING,
            "font_path",
            PROPERTY_HINT_FILE,
            "*.ttf,*.otf,*.woff"
        ),
        "set_font_path",
        "get_font_path"
    );
}
