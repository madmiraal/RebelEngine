// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "dynamic_font.h"

#include "modules/modules_enabled.gen.h" // For freetype.
#ifdef MODULE_FREETYPE_ENABLED

#include "core/os/file_access.h"
#include "core/os/os.h"
#include "scene/resources/fonts/dynamic_font_at_size.h"

SelfList<DynamicFont>::List* DynamicFont::dynamic_fonts = nullptr;
Mutex DynamicFont::dynamic_font_mutex;

DynamicFont::DynamicFont() {
    cache_id.size         = 16;
    outline_cache_id.size = 16;
    dynamic_font_mutex.lock();
    dynamic_fonts->add(&font_list);
    dynamic_font_mutex.unlock();
}

DynamicFont::~DynamicFont() {
    dynamic_font_mutex.lock();
    dynamic_fonts->remove(&font_list);
    dynamic_font_mutex.unlock();
}

float DynamicFont::get_ascent() const {
    if (!font_at_size.is_valid()) {
        return 1;
    }
    return font_at_size->get_ascent() + top_spacing;
}

float DynamicFont::get_descent() const {
    if (!font_at_size.is_valid()) {
        return 1;
    }
    return font_at_size->get_descent() + bottom_spacing;
}

float DynamicFont::get_height() const {
    if (!font_at_size.is_valid()) {
        return 1;
    }
    return font_at_size->get_height() + top_spacing + bottom_spacing;
}

bool DynamicFont::is_distance_field_hint() const {
    return false;
}

bool DynamicFont::has_outline() const {
    return outline_cache_id.outline_size > 0;
}

Size2 DynamicFont::get_char_size(
    const CharType character,
    const CharType next_character
) const {
    if (!font_at_size.is_valid()) {
        return {1, 1};
    }

    Size2 character_size = font_at_size->get_char_size(
        character,
        next_character,
        fallback_fonts_at_size
    );
    if (character == ' ') {
        character_size.width += space_spacing + character_spacing;
    } else if (next_character) {
        character_size.width += character_spacing;
    }
    return character_size;
}

float DynamicFont::draw_char(
    const RID canvas_item,
    const Point2& position,
    const CharType character,
    const CharType next_character,
    const Color& color,
    const bool draw_outline
) const {
    if (!font_at_size.is_valid()) {
        return 0;
    }

    int spacing = character_spacing;
    if (character == ' ') {
        spacing += space_spacing;
    }

    if (draw_outline && outline_font_at_size.is_valid()
        && outline_cache_id.outline_size > 0) {
        outline_font_at_size->draw_char(
            canvas_item,
            position,
            character,
            next_character,
            color * outline_color,
            fallback_outline_fonts_at_size,
            false,
            true
        );
    }
    return font_at_size->draw_char(
               canvas_item,
               position,
               character,
               next_character,
               color,
               fallback_fonts_at_size,
               draw_outline,
               false
           )
         + spacing;
}

String DynamicFont::get_available_chars() const {
    if (!font_at_size.is_valid()) {
        return "";
    }
    String available_characters = font_at_size->get_available_chars();
    for (int i = 0; i < fallback_fonts_at_size.size(); i++) {
        String available_fallback_characters =
            fallback_fonts_at_size[i]->get_available_chars();
        for (int j = 0; j < available_fallback_characters.length(); j++) {
            const auto fallback_character = available_fallback_characters[j];
            if (available_characters.find_char(fallback_character) == -1) {
                available_characters += fallback_character;
            }
        }
    }
    return available_characters;
}

Ref<DynamicFontData> DynamicFont::get_font_data() const {
    return font_data;
}

void DynamicFont::set_font_data(const Ref<DynamicFontData>& new_font_data) {
    font_data = new_font_data;
    // Don't pass the property name,
    // because clearing the font data also clears the fallbacks.
    reload_cache();
}

Color DynamicFont::get_outline_color() const {
    return outline_color;
}

void DynamicFont::set_outline_color(const Color new_outline_color) {
    if (outline_color == new_outline_color) {
        return;
    }
    outline_color = new_outline_color;
    emit_changed();
    _change_notify("outline_color");
}

int DynamicFont::get_outline_size() const {
    return outline_cache_id.outline_size;
}

void DynamicFont::set_outline_size(const int new_size) {
    if (outline_cache_id.outline_size == new_size) {
        return;
    }
    ERR_FAIL_COND(new_size < 0 || new_size > UINT8_MAX);
    outline_cache_id.outline_size = new_size;
    reload_cache("outline_size");
}

int DynamicFont::get_size() const {
    return cache_id.size;
}

void DynamicFont::set_size(const int new_size) {
    if (cache_id.size == new_size) {
        return;
    }
    cache_id.size         = new_size;
    outline_cache_id.size = new_size;
    reload_cache("size");
}

int DynamicFont::get_spacing(const int spacing_type) const {
    if (spacing_type == SPACING_TOP) {
        return top_spacing;
    }
    if (spacing_type == SPACING_BOTTOM) {
        return bottom_spacing;
    }
    if (spacing_type == SPACING_CHAR) {
        return character_spacing;
    }
    if (spacing_type == SPACING_SPACE) {
        return space_spacing;
    }
    return 0;
}

void DynamicFont::set_spacing(int spacing_type, int new_value) {
    if (spacing_type == SPACING_TOP) {
        top_spacing = new_value;
        _change_notify("extra_spacing_top");
    } else if (spacing_type == SPACING_BOTTOM) {
        bottom_spacing = new_value;
        _change_notify("extra_spacing_bottom");
    } else if (spacing_type == SPACING_CHAR) {
        character_spacing = new_value;
        _change_notify("extra_spacing_char");
    } else if (spacing_type == SPACING_SPACE) {
        space_spacing = new_value;
        _change_notify("extra_spacing_space");
    }
    emit_changed();
}

bool DynamicFont::get_use_filter() const {
    return cache_id.filter;
}

void DynamicFont::set_use_filter(const bool enabled) {
    if (cache_id.filter == enabled) {
        return;
    }
    cache_id.filter         = enabled;
    outline_cache_id.filter = enabled;
    reload_cache();
}

bool DynamicFont::get_use_mipmaps() const {
    return cache_id.mipmaps;
}

void DynamicFont::set_use_mipmaps(const bool enabled) {
    if (cache_id.mipmaps == enabled) {
        return;
    }
    cache_id.mipmaps         = enabled;
    outline_cache_id.mipmaps = enabled;
    reload_cache();
}

int DynamicFont::get_fallback_count() const {
    return fallback_fonts_data.size();
}

Ref<DynamicFontData> DynamicFont::get_fallback(const int index) const {
    ERR_FAIL_INDEX_V(index, fallback_fonts_data.size(), Ref<DynamicFontData>());
    return fallback_fonts_data[index];
}

void DynamicFont::set_fallback(
    const int index,
    const Ref<DynamicFontData>& new_fallback_font_data
) {
    ERR_FAIL_COND(new_fallback_font_data.is_null());
    ERR_FAIL_INDEX(index, fallback_fonts_data.size());
    fallback_fonts_data.write[index] = new_fallback_font_data;
    fallback_fonts_at_size.write[index] =
        fallback_fonts_data.write[index]->get_font_at_size(cache_id);
}

void DynamicFont::add_fallback(
    const Ref<DynamicFontData>& new_fallback_font_data
) {
    ERR_FAIL_COND(new_fallback_font_data.is_null());
    fallback_fonts_data.push_back(new_fallback_font_data);
    fallback_fonts_at_size.push_back(fallback_fonts_data
                                         .write[fallback_fonts_data.size() - 1]
                                         ->get_font_at_size(cache_id));
    if (outline_cache_id.outline_size > 0) {
        fallback_outline_fonts_at_size.push_back(
            fallback_fonts_data.write[fallback_fonts_data.size() - 1]
                ->get_font_at_size(outline_cache_id)
        );
    }
    emit_changed();
    _change_notify();
}

void DynamicFont::remove_fallback(const int index) {
    ERR_FAIL_INDEX(index, fallback_fonts_data.size());
    fallback_fonts_data.remove(index);
    fallback_fonts_at_size.remove(index);
    emit_changed();
    _change_notify();
}

void DynamicFont::initialize_dynamic_fonts() {
    dynamic_fonts = memnew(SelfList<DynamicFont>::List());
}

void DynamicFont::finish_dynamic_fonts() {
    memdelete(dynamic_fonts);
    dynamic_fonts = nullptr;
}

void DynamicFont::update_oversampling() {
    Vector<Ref<DynamicFont>> changed_fonts;
    dynamic_font_mutex.lock();
    SelfList<DynamicFont>* E = dynamic_fonts->get_first();
    while (E) {
        if (E->get_self()->font_at_size.is_valid()) {
            E->get_self()->font_at_size->update_oversampling();
            if (E->get_self()->outline_font_at_size.is_valid()) {
                E->get_self()->outline_font_at_size->update_oversampling();
            }
            for (int i = 0; i < E->get_self()->fallback_fonts_at_size.size();
                 i++) {
                if (E->get_self()->fallback_fonts_at_size[i].is_valid()) {
                    E->get_self()
                        ->fallback_fonts_at_size.write[i]
                        ->update_oversampling();
                    if (E->get_self()->has_outline()
                        && E->get_self()
                               ->fallback_outline_fonts_at_size[i]
                               .is_valid()) {
                        E->get_self()
                            ->fallback_outline_fonts_at_size.write[i]
                            ->update_oversampling();
                    }
                }
            }
            changed_fonts.push_back(Ref<DynamicFont>(E->get_self()));
        }
        E = E->get_next();
    }
    dynamic_font_mutex.unlock();

    for (int i = 0; i < changed_fonts.size(); i++) {
        changed_fonts.write[i]->emit_changed();
    }
}

bool DynamicFont::_get(const StringName& name, Variant& result) const {
    String string = name;
    if (string.begins_with("fallback/")) {
        int index = string.get_slicec('/', 1).to_int();
        if (index == fallback_fonts_data.size()) {
            result = Ref<DynamicFontData>();
            return true;
        }
        if (index >= 0 && index < fallback_fonts_data.size()) {
            result = get_fallback(index);
            return true;
        }
    }
    return false;
}

bool DynamicFont::_set(const StringName& name, const Variant& new_value) {
    String string = name;
    if (string.begins_with("fallback/")) {
        int index                          = string.get_slicec('/', 1).to_int();
        Ref<DynamicFontData> new_font_data = new_value;
        if (new_font_data.is_valid()) {
            if (index == fallback_fonts_data.size()) {
                add_fallback(new_font_data);
                return true;
            }
            if (index >= 0 && index < fallback_fonts_data.size()) {
                set_fallback(index, new_font_data);
                return true;
            }
            return false;
        }
        if (index >= 0 && index < fallback_fonts_data.size()) {
            remove_fallback(index);
            return true;
        }
    }
    return false;
}

void DynamicFont::_get_property_list(List<PropertyInfo>* list) const {
    for (int i = 0; i < fallback_fonts_data.size(); i++) {
        list->push_back(PropertyInfo(
            Variant::OBJECT,
            "fallback/" + itos(i),
            PROPERTY_HINT_RESOURCE_TYPE,
            "DynamicFontData"
        ));
    }

    list->push_back(PropertyInfo(
        Variant::OBJECT,
        "fallback/" + itos(fallback_fonts_data.size()),
        PROPERTY_HINT_RESOURCE_TYPE,
        "DynamicFontData"
    ));
}

void DynamicFont::_bind_methods() {
    ClassDB::bind_method(
        D_METHOD("get_available_chars"),
        &DynamicFont::get_available_chars
    );
    ClassDB::bind_method(
        D_METHOD("get_font_data"),
        &DynamicFont::get_font_data
    );
    ClassDB::bind_method(
        D_METHOD("set_font_data", "data"),
        &DynamicFont::set_font_data
    );
    ClassDB::bind_method(
        D_METHOD("get_outline_color"),
        &DynamicFont::get_outline_color
    );
    ClassDB::bind_method(
        D_METHOD("set_outline_color", "color"),
        &DynamicFont::set_outline_color
    );
    ClassDB::bind_method(
        D_METHOD("get_outline_size"),
        &DynamicFont::get_outline_size
    );
    ClassDB::bind_method(
        D_METHOD("set_outline_size", "size"),
        &DynamicFont::set_outline_size
    );
    ClassDB::bind_method(D_METHOD("get_size"), &DynamicFont::get_size);
    ClassDB::bind_method(D_METHOD("set_size", "data"), &DynamicFont::set_size);
    ClassDB::bind_method(
        D_METHOD("get_spacing", "type"),
        &DynamicFont::get_spacing
    );
    ClassDB::bind_method(
        D_METHOD("set_spacing", "type", "value"),
        &DynamicFont::set_spacing
    );
    ClassDB::bind_method(
        D_METHOD("get_use_filter"),
        &DynamicFont::get_use_filter
    );
    ClassDB::bind_method(
        D_METHOD("set_use_filter", "enable"),
        &DynamicFont::set_use_filter
    );
    ClassDB::bind_method(
        D_METHOD("get_use_mipmaps"),
        &DynamicFont::get_use_mipmaps
    );
    ClassDB::bind_method(
        D_METHOD("set_use_mipmaps", "enable"),
        &DynamicFont::set_use_mipmaps
    );

    ClassDB::bind_method(
        D_METHOD("get_fallback_count"),
        &DynamicFont::get_fallback_count
    );
    ClassDB::bind_method(
        D_METHOD("get_fallback", "idx"),
        &DynamicFont::get_fallback
    );
    ClassDB::bind_method(
        D_METHOD("set_fallback", "idx", "data"),
        &DynamicFont::set_fallback
    );
    ClassDB::bind_method(
        D_METHOD("add_fallback", "data"),
        &DynamicFont::add_fallback
    );
    ClassDB::bind_method(
        D_METHOD("remove_fallback", "idx"),
        &DynamicFont::remove_fallback
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

void DynamicFont::reload_cache(const char* triggering_property) {
    ERR_FAIL_COND(cache_id.size < 1);
    if (!font_data.is_valid()) {
        font_at_size.unref();
        outline_font_at_size.unref();
        fallback_fonts_data.resize(0);
        fallback_fonts_at_size.resize(0);
        fallback_outline_fonts_at_size.resize(0);
        return;
    }

    font_at_size = font_data->get_font_at_size(cache_id);
    if (outline_cache_id.outline_size > 0) {
        outline_font_at_size = font_data->get_font_at_size(outline_cache_id);
        fallback_outline_fonts_at_size.resize(fallback_fonts_at_size.size());
    } else {
        outline_font_at_size.unref();
        fallback_outline_fonts_at_size.resize(0);
    }

    for (int i = 0; i < fallback_fonts_data.size(); i++) {
        fallback_fonts_at_size.write[i] =
            fallback_fonts_data.write[i]->get_font_at_size(cache_id);
        if (outline_cache_id.outline_size > 0) {
            fallback_outline_fonts_at_size.write[i] =
                fallback_fonts_data.write[i]->get_font_at_size(outline_cache_id
                );
        }
    }

    emit_changed();
    _change_notify(triggering_property);
}

#endif // MODULE_FREETYPE_ENABLED
