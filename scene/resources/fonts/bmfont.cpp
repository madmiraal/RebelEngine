// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "bmfont.h"

#include "core/error_macros.h"
#include "core/os/file_access.h"

static Map<String, String> get_key_value_pairs(const String& line) {
    Map<String, String> keys;
    int position = 0;
    while (position < line.size()) {
        const int equals_position = line.find("=", position);
        if (equals_position == -1) {
            break;
        }
        const int key_length = equals_position - position;
        String key           = line.substr(position, key_length);
        String value;
        if (line[equals_position + 1] == '"') {
            // Value is a text string surrounded by ".
            const int end_position = line.find("\"", equals_position + 2);
            if (end_position == -1) {
                break;
            }
            // Don't include the ".
            const int value_length = end_position - equals_position - 2;
            value    = line.substr(equals_position + 2, value_length);
            position = end_position + 1;
        } else {
            // Value is a number.
            int end_position = line.find(" ", equals_position + 1);
            if (end_position == -1) {
                // End of the line.
                end_position = line.size();
            }
            const int value_length = end_position - equals_position;
            value    = line.substr(equals_position + 1, value_length);
            position = end_position;
        }
        keys[key] = value;
        // Skip white-space.
        while (position < line.size() && line[position] == ' ') {
            position++;
        }
    }
    return keys;
}

static void extract_info(BitmapFont* font, const Map<String, String>& keys) {
    if (keys.has("face")) {
        font->set_name(keys["face"]);
    }
}

static void extract_common(BitmapFont* font, const Map<String, String>& keys) {
    if (keys.has("lineHeight")) {
        font->set_height(keys["lineHeight"].to_int());
    }
    if (keys.has("base")) {
        font->set_ascent(keys["base"].to_int());
    }
}

static void extract_page(
    BitmapFont* font,
    const Map<String, String>& keys,
    const String& directory
) {
    if (keys.has("file")) {
        const String file          = directory.plus_file(keys["file"]);
        const Ref<Texture> texture = ResourceLoader::load(file);
        ERR_FAIL_COND_MSG(texture.is_null(), "Can't load font texture!");
        font->add_texture(texture);
    }
}

static void extract_char(BitmapFont* font, const Map<String, String>& keys) {
    // The character id.
    CharType idx = 0;
    if (keys.has("id")) {
        idx = keys["id"].to_int();
    }
    // The character bounding box in the texture.
    Rect2i bounding_box;
    if (keys.has("x")) {
        bounding_box.position.x = keys["x"].to_int();
    }
    if (keys.has("y")) {
        bounding_box.position.y = keys["y"].to_int();
    }
    if (keys.has("width")) {
        bounding_box.size.width = keys["width"].to_int();
    }
    if (keys.has("height")) {
        bounding_box.size.height = keys["height"].to_int();
    }
    // How much the bounding box should be offset when displaying the image.
    Point2i offset;
    if (keys.has("xoffset")) {
        offset.x = keys["xoffset"].to_int();
    }
    if (keys.has("yoffset")) {
        offset.y = keys["yoffset"].to_int();
    }
    // The texture page that contains the character image.
    int texture = 0;
    if (keys.has("page")) {
        texture = keys["page"].to_int();
    }
    // How much the current position should be advanced after drawing the image.
    int advance = -1;
    if (keys.has("xadvance")) {
        advance = keys["xadvance"].to_int();
    }
    font->add_char(idx, texture, bounding_box, offset, advance);
}

static void extract_kerning(BitmapFont* font, const Map<String, String>& keys) {
    CharType first_character  = 0;
    CharType second_character = 0;
    int kerning               = 0;
    if (keys.has("first")) {
        first_character = keys["first"].to_int();
    }
    if (keys.has("second")) {
        second_character = keys["second"].to_int();
    }
    if (keys.has("amount")) {
        kerning = keys["amount"].to_int();
    }
    font->add_kerning_pair(first_character, second_character, -kerning);
}

float BitmapFont::get_ascent() const {
    return ascent;
}

float BitmapFont::get_descent() const {
    return height - ascent;
}

float BitmapFont::get_height() const {
    return height;
}

bool BitmapFont::is_distance_field_hint() const {
    return distance_field_hint;
}

Size2 BitmapFont::get_char_size(
    const CharType character,
    const CharType next_character
) const {
    const CharacterData* character_data = characters.getptr(character);
    if (!character_data) {
        if (fallback.is_valid()) {
            return fallback->get_char_size(character, next_character);
        }
        return {};
    }

    Size2i size(character_data->advance, character_data->bounding_box.size.y);
    if (next_character) {
        KerningPairKey kerning_pair_key;
        kerning_pair_key.first_character  = character;
        kerning_pair_key.second_character = next_character;
        const Map<KerningPairKey, int>::Element* E =
            kernings.find(kerning_pair_key);
        if (E) {
            size.width -= E->get();
        }
    }
    return size;
}

float BitmapFont::draw_char(
    const RID canvas_item,
    const Point2& position,
    const CharType character,
    const CharType next_character,
    const Color& color,
    const bool has_outline
) const {
    const CharacterData* character_data = characters.getptr(character);
    if (!character_data) {
        if (fallback.is_valid()) {
            return fallback->draw_char(
                canvas_item,
                position,
                character,
                next_character,
                color,
                has_outline
            );
        }
        return 0;
    }

    ERR_FAIL_COND_V(
        character_data->texture_index < -1
            || character_data->texture_index >= textures.size(),
        0
    );
    if (!has_outline && character_data->texture_index != -1) {
        const int x_offset = character_data->offset.x;
        const int y_offset = ascent + character_data->offset.y;
        const Point2i character_position =
            position + Point2i(x_offset, y_offset);
        VisualServer::get_singleton()->canvas_item_add_texture_rect_region(
            canvas_item,
            Rect2(character_position, character_data->bounding_box.size),
            textures[character_data->texture_index]->get_rid(),
            character_data->bounding_box,
            color,
            false,
            RID(),
            false
        );
    }

    return get_char_size(character, next_character).width;
}

Error BitmapFont::create_from_fnt(const String& font_filename) {
    // fnt format used by AngelCode BMFont.
    // http://www.angelcode.com/products/bmfont/

    const String directory  = font_filename.get_base_dir();
    FileAccess* file_access = FileAccess::open(font_filename, FileAccess::READ);
    ERR_FAIL_COND_V_MSG(
        !file_access,
        ERR_FILE_NOT_FOUND,
        "Can't open font: " + font_filename + "."
    );
    clear();

    while (true) {
        // First word is the tag name.
        String line          = file_access->get_line();
        const int tag_length = line.find(" ");
        String tag           = line.substr(0, tag_length);
        int position         = tag_length + 1;
        // Skip white-space.
        while (position < line.size() && line[position] == ' ') {
            position++;
        }

        // Get all key=value pairs for the tag.
        Map<String, String> keys = get_key_value_pairs(line.substr(position));

        // Extract tag information.
        if (tag == "info") {
            // Information on how the font was generated.
            extract_info(this, keys);
        } else if (tag == "common") {
            // Information common to all characters.
            extract_common(this, keys);
        } else if (tag == "page") {
            // Texture file information. One for each page in the font.
            extract_page(this, keys, directory);
        } else if (tag == "char") {
            // Character information. One for each character in the font.
            extract_char(this, keys);
        } else if (tag == "kerning") {
            // Kerning information. Adjusts the distance between two characters.
            extract_kerning(this, keys);
        }
        if (file_access->eof_reached()) {
            break;
        }
    }
    memdelete(file_access);
    return OK;
}

void BitmapFont::clear() {
    ascent              = 0;
    height              = 0;
    distance_field_hint = false;
    characters.clear();
    kernings.clear();
    textures.clear();
}

void BitmapFont::set_ascent(const int new_ascent) {
    ascent = new_ascent;
}

void BitmapFont::set_height(const int new_height) {
    height = new_height;
}

void BitmapFont::set_distance_field_hint(const bool new_distance_field_hint) {
    distance_field_hint = new_distance_field_hint;
    emit_changed();
}

Ref<BitmapFont> BitmapFont::get_fallback() const {
    return fallback;
}

void BitmapFont::set_fallback(const Ref<BitmapFont>& new_fallback) {
    for (Ref<BitmapFont> fallback_child = new_fallback;
         fallback_child != nullptr;
         fallback_child = fallback_child->get_fallback()) {
        ERR_FAIL_COND_MSG(
            fallback_child == this,
            "Can't add fallback, because it would create a fallback loop."
        );
    }
    fallback = new_fallback;
}

void BitmapFont::add_char(
    const CharType new_character,
    const int texture_index,
    const Rect2& bounding_box,
    const Point2& offset,
    int advance
) {
    if (advance < 0) {
        advance = static_cast<int>(bounding_box.size.width);
    }
    characters[new_character] = {texture_index, bounding_box, offset, advance};
}

int BitmapFont::get_kerning_pair(
    const CharType first_character,
    const CharType second_character
) const {
    KerningPairKey kerning_pair_key;
    kerning_pair_key.first_character  = first_character;
    kerning_pair_key.second_character = second_character;
    const Map<KerningPairKey, int>::Element* E =
        kernings.find(kerning_pair_key);
    if (E) {
        return E->get();
    }
    return 0;
}

void BitmapFont::add_kerning_pair(
    const CharType first_character,
    const CharType second_character,
    const int kerning
) {
    KerningPairKey kerning_pair_key;
    kerning_pair_key.first_character  = first_character;
    kerning_pair_key.second_character = second_character;
    if (kerning == 0 && kernings.has(kerning_pair_key)) {
        kernings.erase(kerning_pair_key);
    } else {
        kernings[kerning_pair_key] = kerning;
    }
}

int BitmapFont::get_texture_count() const {
    return textures.size();
}

Ref<Texture> BitmapFont::get_texture(const int index) const {
    ERR_FAIL_INDEX_V(index, textures.size(), Ref<Texture>());
    return textures[index];
}

void BitmapFont::add_texture(const Ref<Texture>& new_texture) {
    ERR_FAIL_COND_MSG(new_texture.is_null(), "Invalid Texture object.");
    textures.push_back(new_texture);
}

void BitmapFont::_bind_methods() {
    ClassDB::bind_method(
        D_METHOD("create_from_fnt", "path"),
        &BitmapFont::create_from_fnt
    );
    ClassDB::bind_method(D_METHOD("clear"), &BitmapFont::clear);
    ClassDB::bind_method(
        D_METHOD("set_ascent", "pixels"),
        &BitmapFont::set_ascent
    );
    ClassDB::bind_method(
        D_METHOD("set_height", "pixels"),
        &BitmapFont::set_height
    );
    ClassDB::bind_method(
        D_METHOD("set_distance_field_hint", "enable"),
        &BitmapFont::set_distance_field_hint
    );
    ClassDB::bind_method(D_METHOD("get_fallback"), &BitmapFont::get_fallback);
    ClassDB::bind_method(
        D_METHOD("set_fallback", "fallback"),
        &BitmapFont::set_fallback
    );
    ClassDB::bind_method(
        D_METHOD(
            "add_char",
            "character",
            "texture_index",
            "bounding_box",
            "offset",
            "advance"
        ),
        &BitmapFont::add_char,
        DEFVAL(Point2()),
        DEFVAL(-1)
    );

    ClassDB::bind_method(
        D_METHOD("get_kerning_pair", "first_character", "second_character"),
        &BitmapFont::get_kerning_pair
    );
    ClassDB::bind_method(
        D_METHOD(
            "add_kerning_pair",
            "first_character",
            "second_character",
            "kerning"
        ),
        &BitmapFont::add_kerning_pair
    );

    ClassDB::bind_method(
        D_METHOD("get_texture_count"),
        &BitmapFont::get_texture_count
    );
    ClassDB::bind_method(
        D_METHOD("get_texture", "index"),
        &BitmapFont::get_texture
    );
    ClassDB::bind_method(
        D_METHOD("add_texture", "texture"),
        &BitmapFont::add_texture
    );

    ADD_PROPERTY(
        PropertyInfo(Variant::REAL, "ascent", PROPERTY_HINT_RANGE, "0,1024,1"),
        "set_ascent",
        "get_ascent"
    );
    ADD_PROPERTY(
        PropertyInfo(Variant::BOOL, "distance_field"),
        "set_distance_field_hint",
        "is_distance_field_hint"
    );
    ADD_PROPERTY(
        PropertyInfo(
            Variant::OBJECT,
            "fallback",
            PROPERTY_HINT_RESOURCE_TYPE,
            "BitmapFont"
        ),
        "set_fallback",
        "get_fallback"
    );
    ADD_PROPERTY(
        PropertyInfo(Variant::REAL, "height", PROPERTY_HINT_RANGE, "1,1024,1"),
        "set_height",
        "get_height"
    );
}
