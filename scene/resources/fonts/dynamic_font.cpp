// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "dynamic_font.h"

#include "scene/resources/fonts/dynamic_font_at_size.h"

// #include "modules/modules_enabled.gen.h" // For freetype.
// #ifdef MODULE_FREETYPE_ENABLED

// #include "core/os/file_access.h"
// #include "core/os/os.h"

// #define __STDC_LIMIT_MACROS
// #include <stdint.h>

void DynamicFont::_reload_cache(const char* p_triggering_property) {
    ERR_FAIL_COND(cache_id.size < 1);
    if (!data.is_valid()) {
        data_at_size.unref();
        outline_data_at_size.unref();
        fallbacks.resize(0);
        fallback_data_at_size.resize(0);
        fallback_outline_data_at_size.resize(0);
        return;
    }

    data_at_size = data->_get_dynamic_font_at_size(cache_id);
    if (outline_cache_id.outline_size > 0) {
        outline_data_at_size =
            data->_get_dynamic_font_at_size(outline_cache_id);
        fallback_outline_data_at_size.resize(fallback_data_at_size.size());
    } else {
        outline_data_at_size.unref();
        fallback_outline_data_at_size.resize(0);
    }

    for (int i = 0; i < fallbacks.size(); i++) {
        fallback_data_at_size.write[i] =
            fallbacks.write[i]->_get_dynamic_font_at_size(cache_id);
        if (outline_cache_id.outline_size > 0) {
            fallback_outline_data_at_size.write[i] =
                fallbacks.write[i]->_get_dynamic_font_at_size(outline_cache_id);
        }
    }

    emit_changed();
    _change_notify(p_triggering_property);
}

void DynamicFont::set_font_data(const Ref<DynamicFontData>& p_data) {
    data = p_data;
    _reload_cache(); // not passing the prop name as clearing the font data also
                     // clears fallbacks
}

Ref<DynamicFontData> DynamicFont::get_font_data() const {
    return data;
}

void DynamicFont::set_size(int p_size) {
    if (cache_id.size == p_size) {
        return;
    }
    cache_id.size         = p_size;
    outline_cache_id.size = p_size;
    _reload_cache("size");
}

int DynamicFont::get_size() const {
    return cache_id.size;
}

void DynamicFont::set_outline_size(int p_size) {
    if (outline_cache_id.outline_size == p_size) {
        return;
    }
    ERR_FAIL_COND(p_size < 0 || p_size > UINT8_MAX);
    outline_cache_id.outline_size = p_size;
    _reload_cache("outline_size");
}

int DynamicFont::get_outline_size() const {
    return outline_cache_id.outline_size;
}

void DynamicFont::set_outline_color(Color p_color) {
    if (p_color != outline_color) {
        outline_color = p_color;
        emit_changed();
        _change_notify("outline_color");
    }
}

Color DynamicFont::get_outline_color() const {
    return outline_color;
}

bool DynamicFont::get_use_mipmaps() const {
    return cache_id.mipmaps;
}

void DynamicFont::set_use_mipmaps(bool p_enable) {
    if (cache_id.mipmaps == p_enable) {
        return;
    }
    cache_id.mipmaps         = p_enable;
    outline_cache_id.mipmaps = p_enable;
    _reload_cache();
}

bool DynamicFont::get_use_filter() const {
    return cache_id.filter;
}

void DynamicFont::set_use_filter(bool p_enable) {
    if (cache_id.filter == p_enable) {
        return;
    }
    cache_id.filter         = p_enable;
    outline_cache_id.filter = p_enable;
    _reload_cache();
}

bool DynamicFontData::is_antialiased() const {
    return antialiased;
}

void DynamicFontData::set_antialiased(bool p_antialiased) {
    if (antialiased == p_antialiased) {
        return;
    }
    antialiased = p_antialiased;
}

DynamicFontData::Hinting DynamicFontData::get_hinting() const {
    return hinting;
}

void DynamicFontData::set_hinting(Hinting p_hinting) {
    if (hinting == p_hinting) {
        return;
    }
    hinting = p_hinting;
}

int DynamicFont::get_spacing(int p_type) const {
    if (p_type == SPACING_TOP) {
        return spacing_top;
    } else if (p_type == SPACING_BOTTOM) {
        return spacing_bottom;
    } else if (p_type == SPACING_CHAR) {
        return spacing_char;
    } else if (p_type == SPACING_SPACE) {
        return spacing_space;
    }

    return 0;
}

void DynamicFont::set_spacing(int p_type, int p_value) {
    if (p_type == SPACING_TOP) {
        spacing_top = p_value;
        _change_notify("extra_spacing_top");
    } else if (p_type == SPACING_BOTTOM) {
        spacing_bottom = p_value;
        _change_notify("extra_spacing_bottom");
    } else if (p_type == SPACING_CHAR) {
        spacing_char = p_value;
        _change_notify("extra_spacing_char");
    } else if (p_type == SPACING_SPACE) {
        spacing_space = p_value;
        _change_notify("extra_spacing_space");
    }

    emit_changed();
}

float DynamicFont::get_height() const {
    if (!data_at_size.is_valid()) {
        return 1;
    }

    return data_at_size->get_height() + spacing_top + spacing_bottom;
}

float DynamicFont::get_ascent() const {
    if (!data_at_size.is_valid()) {
        return 1;
    }

    return data_at_size->get_ascent() + spacing_top;
}

float DynamicFont::get_descent() const {
    if (!data_at_size.is_valid()) {
        return 1;
    }

    return data_at_size->get_descent() + spacing_bottom;
}

Size2 DynamicFont::get_char_size(CharType p_char, CharType p_next) const {
    if (!data_at_size.is_valid()) {
        return Size2(1, 1);
    }

    Size2 ret =
        data_at_size->get_char_size(p_char, p_next, fallback_data_at_size);
    if (p_char == ' ') {
        ret.width += spacing_space + spacing_char;
    } else if (p_next) {
        ret.width += spacing_char;
    }

    return ret;
}

String DynamicFont::get_available_chars() const {
    if (!data_at_size.is_valid()) {
        return "";
    }

    String chars = data_at_size->get_available_chars();

    for (int i = 0; i < fallback_data_at_size.size(); i++) {
        String fallback_chars = fallback_data_at_size[i]->get_available_chars();
        for (int j = 0; j < fallback_chars.length(); j++) {
            if (chars.find_char(fallback_chars[j]) == -1) {
                chars += fallback_chars[j];
            }
        }
    }

    return chars;
}

bool DynamicFont::is_distance_field_hint() const {
    return false;
}

bool DynamicFont::has_outline() const {
    return outline_cache_id.outline_size > 0;
}

float DynamicFont::draw_char(
    RID p_canvas_item,
    const Point2& p_pos,
    CharType p_char,
    CharType p_next,
    const Color& p_modulate,
    bool p_outline
) const {
    if (!data_at_size.is_valid()) {
        return 0;
    }

    int spacing = spacing_char;
    if (p_char == ' ') {
        spacing += spacing_space;
    }

    if (p_outline) {
        if (outline_data_at_size.is_valid()
            && outline_cache_id.outline_size > 0) {
            outline_data_at_size->draw_char(
                p_canvas_item,
                p_pos,
                p_char,
                p_next,
                p_modulate * outline_color,
                fallback_outline_data_at_size,
                false,
                true
            ); // Draw glyph outline.
        }
        return data_at_size->draw_char(
                   p_canvas_item,
                   p_pos,
                   p_char,
                   p_next,
                   p_modulate,
                   fallback_data_at_size,
                   true,
                   false
               )
             + spacing; // Return advance of the base glyph.
    } else {
        return data_at_size->draw_char(
                   p_canvas_item,
                   p_pos,
                   p_char,
                   p_next,
                   p_modulate,
                   fallback_data_at_size,
                   false,
                   false
               )
             + spacing; // Draw base glyph and return advance.
    }
}

void DynamicFont::set_fallback(int p_idx, const Ref<DynamicFontData>& p_data) {
    ERR_FAIL_COND(p_data.is_null());
    ERR_FAIL_INDEX(p_idx, fallbacks.size());
    fallbacks.write[p_idx] = p_data;
    fallback_data_at_size.write[p_idx] =
        fallbacks.write[p_idx]->_get_dynamic_font_at_size(cache_id);
}

void DynamicFont::add_fallback(const Ref<DynamicFontData>& p_data) {
    ERR_FAIL_COND(p_data.is_null());
    fallbacks.push_back(p_data);
    fallback_data_at_size.push_back(fallbacks.write[fallbacks.size() - 1]
                                        ->_get_dynamic_font_at_size(cache_id)
    ); // const..
    if (outline_cache_id.outline_size > 0) {
        fallback_outline_data_at_size.push_back(
            fallbacks.write[fallbacks.size() - 1]->_get_dynamic_font_at_size(
                outline_cache_id
            )
        );
    }

    emit_changed();
    _change_notify();
}

int DynamicFont::get_fallback_count() const {
    return fallbacks.size();
}

Ref<DynamicFontData> DynamicFont::get_fallback(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, fallbacks.size(), Ref<DynamicFontData>());

    return fallbacks[p_idx];
}

void DynamicFont::remove_fallback(int p_idx) {
    ERR_FAIL_INDEX(p_idx, fallbacks.size());
    fallbacks.remove(p_idx);
    fallback_data_at_size.remove(p_idx);
    emit_changed();
    _change_notify();
}

bool DynamicFont::_set(const StringName& p_name, const Variant& p_value) {
    String str = p_name;
    if (str.begins_with("fallback/")) {
        int idx                 = str.get_slicec('/', 1).to_int();
        Ref<DynamicFontData> fd = p_value;

        if (fd.is_valid()) {
            if (idx == fallbacks.size()) {
                add_fallback(fd);
                return true;
            } else if (idx >= 0 && idx < fallbacks.size()) {
                set_fallback(idx, fd);
                return true;
            } else {
                return false;
            }
        } else if (idx >= 0 && idx < fallbacks.size()) {
            remove_fallback(idx);
            return true;
        }
    }

    return false;
}

bool DynamicFont::_get(const StringName& p_name, Variant& r_ret) const {
    String str = p_name;
    if (str.begins_with("fallback/")) {
        int idx = str.get_slicec('/', 1).to_int();

        if (idx == fallbacks.size()) {
            r_ret = Ref<DynamicFontData>();
            return true;
        } else if (idx >= 0 && idx < fallbacks.size()) {
            r_ret = get_fallback(idx);
            return true;
        }
    }

    return false;
}

void DynamicFont::_get_property_list(List<PropertyInfo>* p_list) const {
    for (int i = 0; i < fallbacks.size(); i++) {
        p_list->push_back(PropertyInfo(
            Variant::OBJECT,
            "fallback/" + itos(i),
            PROPERTY_HINT_RESOURCE_TYPE,
            "DynamicFontData"
        ));
    }

    p_list->push_back(PropertyInfo(
        Variant::OBJECT,
        "fallback/" + itos(fallbacks.size()),
        PROPERTY_HINT_RESOURCE_TYPE,
        "DynamicFontData"
    ));
}

void DynamicFont::_bind_methods() {
    ClassDB::bind_method(
        D_METHOD("set_font_data", "data"),
        &DynamicFont::set_font_data
    );
    ClassDB::bind_method(
        D_METHOD("get_font_data"),
        &DynamicFont::get_font_data
    );

    ClassDB::bind_method(
        D_METHOD("get_available_chars"),
        &DynamicFont::get_available_chars
    );

    ClassDB::bind_method(D_METHOD("set_size", "data"), &DynamicFont::set_size);
    ClassDB::bind_method(D_METHOD("get_size"), &DynamicFont::get_size);

    ClassDB::bind_method(
        D_METHOD("set_outline_size", "size"),
        &DynamicFont::set_outline_size
    );
    ClassDB::bind_method(
        D_METHOD("get_outline_size"),
        &DynamicFont::get_outline_size
    );

    ClassDB::bind_method(
        D_METHOD("set_outline_color", "color"),
        &DynamicFont::set_outline_color
    );
    ClassDB::bind_method(
        D_METHOD("get_outline_color"),
        &DynamicFont::get_outline_color
    );

    ClassDB::bind_method(
        D_METHOD("set_use_mipmaps", "enable"),
        &DynamicFont::set_use_mipmaps
    );
    ClassDB::bind_method(
        D_METHOD("get_use_mipmaps"),
        &DynamicFont::get_use_mipmaps
    );
    ClassDB::bind_method(
        D_METHOD("set_use_filter", "enable"),
        &DynamicFont::set_use_filter
    );
    ClassDB::bind_method(
        D_METHOD("get_use_filter"),
        &DynamicFont::get_use_filter
    );
    ClassDB::bind_method(
        D_METHOD("set_spacing", "type", "value"),
        &DynamicFont::set_spacing
    );
    ClassDB::bind_method(
        D_METHOD("get_spacing", "type"),
        &DynamicFont::get_spacing
    );

    ClassDB::bind_method(
        D_METHOD("add_fallback", "data"),
        &DynamicFont::add_fallback
    );
    ClassDB::bind_method(
        D_METHOD("set_fallback", "idx", "data"),
        &DynamicFont::set_fallback
    );
    ClassDB::bind_method(
        D_METHOD("get_fallback", "idx"),
        &DynamicFont::get_fallback
    );
    ClassDB::bind_method(
        D_METHOD("remove_fallback", "idx"),
        &DynamicFont::remove_fallback
    );
    ClassDB::bind_method(
        D_METHOD("get_fallback_count"),
        &DynamicFont::get_fallback_count
    );

    ADD_GROUP("Settings", "");
    ADD_PROPERTY(
        PropertyInfo(Variant::INT, "size", PROPERTY_HINT_RANGE, "1,1024,1"),
        "set_size",
        "get_size"
    );
    ADD_PROPERTY(
        PropertyInfo(
            Variant::INT,
            "outline_size",
            PROPERTY_HINT_RANGE,
            "0,1024,1"
        ),
        "set_outline_size",
        "get_outline_size"
    );
    ADD_PROPERTY(
        PropertyInfo(Variant::COLOR, "outline_color"),
        "set_outline_color",
        "get_outline_color"
    );
    ADD_PROPERTY(
        PropertyInfo(Variant::BOOL, "use_mipmaps"),
        "set_use_mipmaps",
        "get_use_mipmaps"
    );
    ADD_PROPERTY(
        PropertyInfo(Variant::BOOL, "use_filter"),
        "set_use_filter",
        "get_use_filter"
    );
    ADD_GROUP("Extra Spacing", "extra_spacing");
    ADD_PROPERTYI(
        PropertyInfo(Variant::INT, "extra_spacing_top"),
        "set_spacing",
        "get_spacing",
        SPACING_TOP
    );
    ADD_PROPERTYI(
        PropertyInfo(Variant::INT, "extra_spacing_bottom"),
        "set_spacing",
        "get_spacing",
        SPACING_BOTTOM
    );
    ADD_PROPERTYI(
        PropertyInfo(Variant::INT, "extra_spacing_char"),
        "set_spacing",
        "get_spacing",
        SPACING_CHAR
    );
    ADD_PROPERTYI(
        PropertyInfo(Variant::INT, "extra_spacing_space"),
        "set_spacing",
        "get_spacing",
        SPACING_SPACE
    );
    ADD_GROUP("Font", "");
    ADD_PROPERTY(
        PropertyInfo(
            Variant::OBJECT,
            "font_data",
            PROPERTY_HINT_RESOURCE_TYPE,
            "DynamicFontData"
        ),
        "set_font_data",
        "get_font_data"
    );

    BIND_ENUM_CONSTANT(SPACING_TOP);
    BIND_ENUM_CONSTANT(SPACING_BOTTOM);
    BIND_ENUM_CONSTANT(SPACING_CHAR);
    BIND_ENUM_CONSTANT(SPACING_SPACE);
}

Mutex DynamicFont::dynamic_font_mutex;

SelfList<DynamicFont>::List* DynamicFont::dynamic_fonts = nullptr;

DynamicFont::DynamicFont() {
    cache_id.size         = 16;
    outline_cache_id.size = 16;
    spacing_top           = 0;
    spacing_bottom        = 0;
    spacing_char          = 0;
    spacing_space         = 0;
    outline_color         = Color(1, 1, 1);
    dynamic_font_mutex.lock();
    dynamic_fonts->add(&font_list);
    dynamic_font_mutex.unlock();
}

DynamicFont::~DynamicFont() {
    dynamic_font_mutex.lock();
    dynamic_fonts->remove(&font_list);
    dynamic_font_mutex.unlock();
}

void DynamicFont::initialize_dynamic_fonts() {
    dynamic_fonts = memnew(SelfList<DynamicFont>::List());
}

void DynamicFont::finish_dynamic_fonts() {
    memdelete(dynamic_fonts);
    dynamic_fonts = nullptr;
}

void DynamicFont::update_oversampling() {
    Vector<Ref<DynamicFont>> changed;

    dynamic_font_mutex.lock();

    SelfList<DynamicFont>* E = dynamic_fonts->first();
    while (E) {
        if (E->self()->data_at_size.is_valid()) {
            E->self()->data_at_size->update_oversampling();

            if (E->self()->outline_data_at_size.is_valid()) {
                E->self()->outline_data_at_size->update_oversampling();
            }

            for (int i = 0; i < E->self()->fallback_data_at_size.size(); i++) {
                if (E->self()->fallback_data_at_size[i].is_valid()) {
                    E->self()
                        ->fallback_data_at_size.write[i]
                        ->update_oversampling();

                    if (E->self()->has_outline()
                        && E->self()->fallback_outline_data_at_size[i].is_valid(
                        )) {
                        E->self()
                            ->fallback_outline_data_at_size.write[i]
                            ->update_oversampling();
                    }
                }
            }

            changed.push_back(Ref<DynamicFont>(E->self()));
        }

        E = E->next();
    }

    dynamic_font_mutex.unlock();

    for (int i = 0; i < changed.size(); i++) {
        changed.write[i]->emit_changed();
    }
}

// #endif
