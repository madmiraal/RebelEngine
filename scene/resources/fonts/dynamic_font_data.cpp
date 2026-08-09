// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "dynamic_font_data.h"

#include "scene/resources/fonts/dynamic_font_at_size.h"

bool DynamicFontData::CacheID::operator<(CacheID right) const {
    return key < right.key;
}

Ref<DynamicFontAtSize> DynamicFontData::_get_dynamic_font_at_size(
    CacheID p_cache_id
) {
    if (size_cache.has(p_cache_id)) {
        return Ref<DynamicFontAtSize>(size_cache[p_cache_id]);
    }

    Ref<DynamicFontAtSize> dfas;

    dfas.instance();

    dfas->font = Ref<DynamicFontData>(this);

    size_cache[p_cache_id] = dfas.ptr();
    dfas->id               = p_cache_id;
    dfas->_load();

    return dfas;
}

void DynamicFontData::set_font_ptr(
    const uint8_t* p_font_mem,
    int p_font_mem_size
) {
    font_mem      = p_font_mem;
    font_mem_size = p_font_mem_size;
}

void DynamicFontData::set_font_path(const String& p_path) {
    font_path = p_path;
}

String DynamicFontData::get_font_path() const {
    return font_path;
}

void DynamicFontData::set_force_autohinter(bool p_force) {
    force_autohinter = p_force;
}

void DynamicFontData::_bind_methods() {
    ClassDB::bind_method(
        D_METHOD("set_antialiased", "antialiased"),
        &DynamicFontData::set_antialiased
    );
    ClassDB::bind_method(
        D_METHOD("is_antialiased"),
        &DynamicFontData::is_antialiased
    );
    ClassDB::bind_method(
        D_METHOD("set_font_path", "path"),
        &DynamicFontData::set_font_path
    );
    ClassDB::bind_method(
        D_METHOD("get_font_path"),
        &DynamicFontData::get_font_path
    );
    ClassDB::bind_method(
        D_METHOD("set_hinting", "mode"),
        &DynamicFontData::set_hinting
    );
    ClassDB::bind_method(
        D_METHOD("get_hinting"),
        &DynamicFontData::get_hinting
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

    // Only WOFF1 is supported as WOFF2 requires a Brotli decompression library
    // to be linked.
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

DynamicFontData::DynamicFontData() {
    antialiased      = true;
    force_autohinter = false;
    hinting          = DynamicFontData::HINTING_NORMAL;
    font_mem         = nullptr;
    font_mem_size    = 0;
}

DynamicFontData::~DynamicFontData() {}
