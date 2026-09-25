// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "dynamic_font_at_size.h"

#include "modules/modules_enabled.gen.h"
#ifdef MODULE_FREETYPE_ENABLED

#include "core/os/file_access.h"
#include "scene/resources/texture.h"

#include FT_STROKER_H

static constexpr int margin = 1;

float DynamicFontAtSize::font_oversampling = 1.0;

namespace {
struct CharacterLocation {
    int texture_index = -1;
    int x_offset      = 0;
    int y_offset      = 0;
};
} // namespace

// FreeType 16.16 numbers are fixed point numbers with 16 bits of precision.
constexpr static float float_from_ft_16_16(const FT_Long& ft_16_16) {
    // A FreeType 16.16 fixed point number has 16 points of precision.
    return static_cast<float>(ft_16_16) / (1 << 16);
}

// FreeType 26.6 numbers are fixed point numbers with 6 bits of precision.
constexpr static float float_from_ft_26_6(const FT_Long& ft_26_6) {
    // A FreeType 26.6 fixed point number has 6 points of precision.
    return static_cast<float>(ft_26_6) / (1 << 6);
}

constexpr static FT_Long ft_26_6_from_float(const float number) {
    return static_cast<FT_Long>(number * (1 << 6));
}

constexpr static Image::Format image_format_from_ft_pixel_mode(
    const FT_Pixel_Mode& ft_pixel_mode
) {
    switch (ft_pixel_mode) {
        case FT_PIXEL_MODE_MONO:
        case FT_PIXEL_MODE_GRAY:
            return Image::FORMAT_LA8;
        case FT_PIXEL_MODE_BGRA:
            return Image::FORMAT_RGBA8;
        default:
            // FT_PIXEL_MODE_NONE
            // FT_PIXEL_MODE_GRAY2
            // FT_PIXEL_MODE_GRAY4
            // FT_PIXEL_MODE_LCD
            // FT_PIXEL_MODE_LCD_V
            ERR_FAIL_V_MSG(Image::FORMAT_MAX, "Unsupported pixel mode.");
    }
}

constexpr static FT_Int32 ft_hinting_from_font_hinting(const int hinting) {
    switch (hinting) {
        case DynamicFontData::HINTING_NONE:
            return FT_LOAD_NO_HINTING;
        case DynamicFontData::HINTING_LIGHT:
            return FT_LOAD_TARGET_LIGHT;
        case DynamicFontData::HINTING_NORMAL:
            return FT_LOAD_TARGET_NORMAL;
        default:
            ERR_FAIL_V_MSG(FT_LOAD_TARGET_NORMAL, "Unknown hinting type.");
    }
}

constexpr static float get_kerning_advance(
    const FT_Face& face,
    const CharType character,
    const CharType next_character
) {
    if (!next_character) {
        return 0;
    }
    FT_Vector kerning{};
    FT_Get_Kerning(
        face,
        FT_Get_Char_Index(face, character),
        FT_Get_Char_Index(face, next_character),
        FT_KERNING_DEFAULT,
        &kerning
    );
    return float_from_ft_26_6(kerning.x);
}

static int calculate_texture_size(
    const int font_size,
    const float oversampling,
    const int character_width,
    const int character_height
) {
    ERR_FAIL_COND_V_MSG(
        character_width > 4096,
        0,
        "Character widths > 4096 are not supported."
    );
    ERR_FAIL_COND_V_MSG(
        character_height > 4096,
        0,
        "Character heights > 4096 are not supported."
    );
    int texture_size = MAX(font_size * oversampling * 8, 256);
    if (character_width > texture_size) {
        // Special case! Adapt to it?
        texture_size = character_width;
    }
    if (character_height > texture_size) {
        // Special case! Adapt to it?
        texture_size = character_height;
    }
    texture_size = static_cast<int>(next_power_of_2(texture_size));
    texture_size = MIN(texture_size, 4096);
    return texture_size;
}

static DynamicFontAtSize::CharacterData create_new_character_data(
    const CharacterLocation character_location,
    const int bitmap_width,
    const int bitmap_height,
    const FT_Int top,
    const FT_Int left,
    const float ft_glyph_advance,
    const float ascent,
    const float color_font_scaling,
    const float oversampling
) {
    const float scaling = color_font_scaling / oversampling;
    const float x_position =
        static_cast<float>(character_location.x_offset + margin) / oversampling;
    const float y_position =
        static_cast<float>(character_location.y_offset + margin) / oversampling;
    const auto width        = static_cast<float>(bitmap_width) * scaling;
    const auto height       = static_cast<float>(bitmap_height) * scaling;
    const int uv_x_position = character_location.x_offset + margin;
    const int uv_y_position = character_location.y_offset + margin;
    const int uv_width      = bitmap_width;
    const int uv_height     = bitmap_height;
    const Rect2 rect{x_position, y_position, width, height};
    const Rect2i uv_rect{uv_x_position, uv_y_position, uv_width, uv_height};
    const float horizontal_offset = static_cast<float>(left) * scaling;
    const float vertical_offset   = ascent - static_cast<float>(top) * scaling;
    const float advance           = ft_glyph_advance * scaling;
    return {
        rect,
        uv_rect,
        character_location.texture_index,
        horizontal_offset,
        vertical_offset,
        advance,
        true
    };
}

static DynamicFontAtSize::CharacterTexture create_new_character_texture(
    const Image::Format& image_format,
    const int texture_size
) {
    const int bytes_per_pixel = Image::get_format_pixel_size(image_format);
    const int image_data_size = texture_size * texture_size * bytes_per_pixel;
    DynamicFontAtSize::CharacterTexture character_texture;
    character_texture.texture_size = texture_size;
    character_texture.image_data.resize(image_data_size);

    // Initialize the texture to all-white pixels to prevent artifacts when the
    // font is displayed at a non-default scale with filtering enabled.
    const PoolVector<unsigned char>::Write image_data =
        character_texture.image_data.write();
    if (bytes_per_pixel == 2) {
        for (int i = 0; i < image_data_size; i += bytes_per_pixel) {
            image_data[i + 0] = 255;
            image_data[i + 1] = 0;
        }
    } else if (bytes_per_pixel == 4) {
        for (int i = 0; i < image_data_size; i += bytes_per_pixel) {
            image_data[i + 0] = 255;
            image_data[i + 1] = 255;
            image_data[i + 2] = 255;
            image_data[i + 3] = 0;
        }
    } else {
        ERR_FAIL_V_MSG({}, "Unsupported image format.");
    }

    // Initialize all y-offsets to 0.
    character_texture.y_offsets.resize(texture_size);
    for (int x = 0; x < texture_size; x++) {
        character_texture.y_offsets.write[x] = 0;
    }

    return character_texture;
}

static void draw_texture(
    const DynamicFontAtSize::CharacterTexture& character_texture,
    const DynamicFontAtSize::CharacterData& character_data,
    const RID canvas_item,
    const Point2& position,
    const float ascent,
    const Color& color
) {
    const float x_position = position.x + character_data.horizontal_offset;
    const float y_position =
        position.y + character_data.vertical_offset - ascent;
    const Point2 character_position{x_position, y_position};
    const Rect2 character_rect{character_position, character_data.rect.size};
    const RID texture = character_texture.texture->get_rid();
    VisualServer::get_singleton()->canvas_item_add_texture_rect_region(
        canvas_item,
        character_rect,
        texture,
        character_data.uv_rect,
        color,
        false,
        RID(),
        false
    );
}

static CharacterLocation find_character_location(
    const DynamicFontAtSize::CharacterTexture& character_texture,
    const int texture_index,
    const int character_width,
    const int character_height
) {
    int x_offset = 0;
    int y_offset = INT_MAX;
    for (int x = 0; x < character_texture.texture_size - character_width; x++) {
        int y_max = 0;
        for (int offset = x; offset < x + character_width; offset++) {
            const int y = character_texture.y_offsets[offset];
            if (y > y_max) {
                y_max = y;
            }
        }
        if (y_max < y_offset) {
            x_offset = x;
            y_offset = y_max;
        }
    }
    if (y_offset == INT_MAX
        || y_offset + character_height > character_texture.texture_size) {
        return {};
    }
    return {texture_index, x_offset, y_offset};
}

static CharacterLocation find_character_location(
    const Vector<DynamicFontAtSize::CharacterTexture>& character_textures,
    const Image::Format& image_format,
    const int character_width,
    const int character_height
) {
    const int texture_count = character_textures.size();
    for (int texture_index = 0; texture_index < texture_count;
         texture_index++) {
        const DynamicFontAtSize::CharacterTexture& character_texture =
            character_textures[texture_index];
        if (character_texture.texture->get_format() != image_format
            || character_texture.texture_size < character_width
            || character_texture.texture_size < character_height) {
            continue;
        }
        const CharacterLocation character_location = find_character_location(
            character_texture,
            texture_index,
            character_width,
            character_height
        );
        if (character_location.texture_index >= 0) {
            return character_location;
        }
    }
    return {};
}

static CharacterLocation create_new_character_location(
    Vector<DynamicFontAtSize::CharacterTexture>& character_textures,
    const Image::Format& image_format,
    const int texture_size,
    const int character_width,
    const int character_height
) {
    const CharacterLocation character_location = find_character_location(
        character_textures,
        image_format,
        character_width,
        character_height
    );
    if (character_location.texture_index != -1) {
        // Location found in existing Character Texture.
        return character_location;
    }

    const DynamicFontAtSize::CharacterTexture character_texture =
        create_new_character_texture(image_format, texture_size);
    ERR_FAIL_COND_V_MSG(
        character_texture.texture_size == 0,
        {},
        "Failed to create new Character Texture."
    );
    character_textures.push_back(character_texture);
    return {character_textures.size() - 1, 0, 0};
}

static Error update_character_texture_image_data(
    const FT_Bitmap& bitmap,
    DynamicFontAtSize::CharacterTexture& character_texture,
    const CharacterLocation& character_location,
    const int bytes_per_pixel
) {
    const int bitmap_width  = static_cast<int>(bitmap.width);
    const int bitmap_height = static_cast<int>(bitmap.rows);
    const PoolVector<unsigned char>::Write image_data =
        character_texture.image_data.write();
    for (int y = 0; y < bitmap_height; y++) {
        for (int x = 0; x < bitmap_width; x++) {
            const int offset = ((y + character_location.y_offset + margin)
                                    * character_texture.texture_size
                                + x + character_location.x_offset + margin)
                             * bytes_per_pixel;
            ERR_FAIL_COND_V(
                offset >= character_texture.image_data.size(),
                ERR_BUG
            );
            switch (bitmap.pixel_mode) {
                case FT_PIXEL_MODE_MONO: {
                    // 1 bit per pixel.
                    // Save as FORMAT_LA8: White with alpha on or off.
                    const int byte         = y * bitmap.pitch + (x >> 3);
                    const int bit          = 1 << (7 - x % 8);
                    image_data[offset + 0] = 255;
                    image_data[offset + 1] =
                        bitmap.buffer[byte] & bit ? 255 : 0;
                } break;
                case FT_PIXEL_MODE_GRAY:
                    // 8 bits per pixel.
                    // Save as FORMAT_LA8: White with alpha as gray scale.
                    // TODO: Check number of gray levels in num_grays.
                    image_data[offset + 0] = 255;
                    image_data[offset + 1] =
                        bitmap.buffer[y * bitmap.pitch + x];
                    break;
                case FT_PIXEL_MODE_BGRA: {
                    // Convert BGRA to RGBA.
                    const int source_offset = y * bitmap.pitch + (x << 2);
                    image_data[offset + 2]  = bitmap.buffer[source_offset + 0];
                    image_data[offset + 1]  = bitmap.buffer[source_offset + 1];
                    image_data[offset + 0]  = bitmap.buffer[source_offset + 2];
                    image_data[offset + 3]  = bitmap.buffer[source_offset + 3];
                } break;
                // TODO: FT_PIXEL_MODE_LCD
                default:
                    ERR_FAIL_V_MSG(
                        ERR_CANT_CREATE,
                        "Font uses unsupported pixel mode: "
                            + itos(bitmap.pixel_mode) + "."
                    );
                    break;
            }
        }
    }
    return OK;
}

static void update_character_texture_texture(
    DynamicFontAtSize::CharacterTexture& character_texture,
    const Image::Format image_format,
    const uint32_t texture_flags
) {
    const Ref<Image> image = memnew(Image(
        character_texture.texture_size,
        character_texture.texture_size,
        0,
        image_format,
        character_texture.image_data
    ));
    if (character_texture.texture.is_null()) {
        character_texture.texture.instance();
        character_texture.texture->create_from_image(
            image,
            Texture::FLAG_VIDEO_SURFACE | texture_flags
        );
    } else {
        character_texture.texture->set_data(image);
    }
}

static void update_character_texture_y_offsets(
    DynamicFontAtSize::CharacterTexture& character_texture,
    const CharacterLocation& character_location,
    const int character_width,
    const int character_height
) {
    for (int x = character_location.x_offset;
         x < character_location.x_offset + character_width;
         x++) {
        character_texture.y_offsets.write[x] =
            character_location.y_offset + character_height;
    }
}

static Error update_character_texture(
    DynamicFontAtSize::CharacterTexture& character_texture,
    const FT_Bitmap& bitmap,
    const Image::Format& image_format,
    const CharacterLocation& character_location,
    const int character_width,
    const int character_height,
    const uint32_t texture_flags
) {
    const int bytes_per_pixel = Image::get_format_pixel_size(image_format);

    const Error error = update_character_texture_image_data(
        bitmap,
        character_texture,
        character_location,
        bytes_per_pixel
    );
    if (error) {
        return {};
    }
    update_character_texture_texture(
        character_texture,
        image_format,
        texture_flags
    );
    update_character_texture_y_offsets(
        character_texture,
        character_location,
        character_width,
        character_height
    );
    return OK;
}

static DynamicFontAtSize::CharacterData create_bitmap_character(
    const FT_Bitmap& bitmap,
    const FT_Int top,
    const FT_Int left,
    const float ft_glyph_advance,
    Vector<DynamicFontAtSize::CharacterTexture>& textures_cache,
    const int font_size,
    const float ascent,
    const float oversampling,
    const float color_font_scaling,
    const uint32_t texture_flags
) {
    const int bitmap_width     = static_cast<int>(bitmap.width);
    const int bitmap_height    = static_cast<int>(bitmap.rows);
    const auto pixel_mode      = static_cast<FT_Pixel_Mode>(bitmap.pixel_mode);
    const int character_width  = bitmap_width + margin * 2;
    const int character_height = bitmap_height + margin * 2;
    const int texture_size     = calculate_texture_size(
        font_size,
        oversampling,
        character_width,
        character_height
    );
    const Image::Format image_format =
        image_format_from_ft_pixel_mode(pixel_mode);
    const CharacterLocation character_location = create_new_character_location(
        textures_cache,
        image_format,
        texture_size,
        character_width,
        character_height
    );
    ERR_FAIL_COND_V_MSG(
        character_location.texture_index < 0,
        DynamicFontAtSize::CharacterData{},
        "Failed to create new Character Location."
    );
    DynamicFontAtSize::CharacterTexture& character_texture =
        textures_cache.write[character_location.texture_index];
    const Error error = update_character_texture(
        character_texture,
        bitmap,
        image_format,
        character_location,
        character_width,
        character_height,
        texture_flags
    );
    if (error) {
        return {};
    }
    return create_new_character_data(
        character_location,
        bitmap_width,
        bitmap_height,
        top,
        left,
        ft_glyph_advance,
        ascent,
        color_font_scaling,
        oversampling
    );
}

DynamicFontAtSize::~DynamicFontAtSize() {
    if (valid) {
        FT_Done_FreeType(ft_library);
    }
    font_data->font_at_sizes_cache.erase(id);
    font_data.unref();
}

float DynamicFontAtSize::get_ascent() const {
    return ascent;
}

float DynamicFontAtSize::get_descent() const {
    return descent;
}

float DynamicFontAtSize::get_height() const {
    return ascent + descent;
}

Size2 DynamicFontAtSize::get_char_size(
    const CharType character,
    const CharType next_character,
    const Vector<Ref<DynamicFontAtSize>>& fallbacks
) const {
    if (!valid) {
        return {};
    }
    const auto pair = get_character_data_and_font(character, fallbacks);
    const CharacterData* character_data   = pair.first;
    const DynamicFontAtSize* font_at_size = pair.second;
    float width =
        get_kerning_advance(font_at_size->ft_face, character, next_character)
        / oversampling;
    if (character_data->found) {
        width += character_data->advance;
    }
    return {width, get_height()};
}

float DynamicFontAtSize::draw_char(
    const RID canvas_item,
    const Point2& position,
    const CharType character,
    const CharType next_character,
    const Color& color,
    const Vector<Ref<DynamicFontAtSize>>& fallbacks,
    const bool advance_only,
    const bool has_outline
) const {
    if (!valid) {
        return 0;
    }
    const auto pair = get_character_data_and_font(character, fallbacks);
    const CharacterData* character_data   = pair.first;
    const DynamicFontAtSize* font_at_size = pair.second;

    const float advance =
        get_kerning_advance(font_at_size->ft_face, character, next_character)
        / oversampling;

    if (character_data->found) {
        if (!advance_only && character_data->texture_index != -1) {
            const CharacterTexture& character_texture =
                font_at_size->textures_cache[character_data->texture_index];
            Color modulate = color;
            if (FT_HAS_COLOR(font_at_size->ft_face)) {
                modulate.r = modulate.g = modulate.b = 1.0;
            }
            draw_texture(
                character_texture,
                *character_data,
                canvas_item,
                position,
                ascent,
                modulate
            );
        }
        return advance + character_data->advance;
    }

    if (!has_outline) {
        return advance;
    }

    FT_Int32 load_flags = FT_LOAD_DEFAULT;
    if (FT_HAS_COLOR(ft_face)) {
        load_flags = FT_LOAD_COLOR;
    }
    int error = FT_Load_Char(ft_face, character, load_flags);
    if (error) {
        return advance;
    }
    error = FT_Render_Glyph(ft_face->glyph, FT_RENDER_MODE_NORMAL);
    if (error) {
        return advance;
    }
    const float ft_glyph_advance =
        float_from_ft_26_6(ft_face->glyph->advance.x);
    const CharacterData bitmap_character_data = create_bitmap_character(
        ft_face->glyph->bitmap,
        ft_face->glyph->bitmap_top,
        ft_face->glyph->bitmap_left,
        ft_glyph_advance,
        textures_cache,
        id.size,
        ascent,
        oversampling,
        color_font_scaling,
        texture_flags
    );
    return advance + bitmap_character_data.advance;
}

String DynamicFontAtSize::get_available_chars() const {
    if (!valid) {
        return {};
    }
    String characters;
    FT_UInt gindex;
    FT_ULong character_code = FT_Get_First_Char(ft_face, &gindex);
    while (gindex != 0) {
        if (character_code != 0) {
            characters += static_cast<CharType>(character_code);
        }
        character_code = FT_Get_Next_Char(ft_face, character_code, &gindex);
    }
    return characters;
}

void DynamicFontAtSize::set_texture_flags(const uint32_t new_texture_flags) {
    texture_flags = new_texture_flags;
    for (int i = 0; i < textures_cache.size(); i++) {
        Ref<ImageTexture>& texture = textures_cache.write[i].texture;
        if (!texture.is_null()) {
            texture->set_flags(new_texture_flags);
        }
    }
}

void DynamicFontAtSize::update_oversampling() {
    if (!valid || oversampling == font_oversampling) {
        return;
    }
    FT_Done_FreeType(ft_library);
    textures_cache.clear();
    character_data_cache.clear();
    oversampling = font_oversampling;
    valid        = false;
    load();
}

Error DynamicFontAtSize::load() {
    FT_Error ft_error = FT_Init_FreeType(&ft_library);
    ERR_FAIL_COND_V_MSG(
        ft_error != 0,
        ERR_CANT_CREATE,
        "Error initializing FreeType."
    );

    if (font_data->font_bytes == nullptr && !font_data->font_path.empty()) {
        FileAccess* file_access =
            FileAccess::open(font_data->font_path, FileAccess::READ);
        if (!file_access) {
            FT_Done_FreeType(ft_library);
            ERR_FAIL_V_MSG(
                ERR_CANT_OPEN,
                "Cannot open font file '" + font_data->font_path + "'."
            );
        }

        const int length = static_cast<int>(file_access->get_len());
        font_data->font_data.resize(length);
        file_access->get_buffer(font_data->font_data.ptrw(), length);
        font_data->set_font_bytes(font_data->font_data.ptr(), length);
        file_access->close();
        memdelete(file_access);
    }
    if (!font_data->font_bytes) {
        FT_Done_FreeType(ft_library);
        ERR_FAIL_V_MSG(ERR_UNCONFIGURED, "DynamicFontData uninitialized.");
    }

    ft_stream      = {};
    ft_stream.base = const_cast<unsigned char*>(font_data->font_bytes);
    ft_stream.size = font_data->font_bytes_length;
    ft_stream.pos  = 0;

    FT_Open_Args ft_open_args = {};
    ft_open_args.memory_base  = font_data->font_bytes;
    ft_open_args.memory_size  = font_data->font_bytes_length;
    ft_open_args.flags        = FT_OPEN_MEMORY;
    ft_open_args.stream       = &ft_stream;

    ft_error = FT_Open_Face(ft_library, &ft_open_args, 0, &ft_face);
    if (ft_error) {
        FT_Done_FreeType(ft_library);
        if (ft_error == FT_Err_Unknown_File_Format) {
            ERR_FAIL_V_MSG(ERR_FILE_CANT_OPEN, "Unknown font format.");
        }
        ERR_FAIL_V_MSG(ERR_FILE_CANT_OPEN, "Error loading font.");
    }

    if (FT_HAS_COLOR(ft_face) && ft_face->num_fixed_sizes > 0) {
        int best_index      = 0;
        int best_difference = INT_MAX;
        for (int i = 0; i < ft_face->num_fixed_sizes; i++) {
            const int this_difference =
                ABS(id.size - ((int64_t)(ft_face->available_sizes[i].width)));
            if (this_difference < best_difference) {
                best_index      = i;
                best_difference = this_difference;
            }
        }
        color_font_scaling =
            static_cast<float>(id.size) * oversampling
            / static_cast<float>(ft_face->available_sizes[best_index].width);
        FT_Select_Size(ft_face, best_index);
    } else {
        const auto oversampled_size =
            static_cast<FT_UInt>(static_cast<float>(id.size) * oversampling);
        FT_Set_Pixel_Sizes(ft_face, 0, oversampled_size);
    }

    ascent = float_from_ft_26_6(ft_face->size->metrics.ascender) / oversampling
           * color_font_scaling;
    descent = -float_from_ft_26_6(ft_face->size->metrics.descender)
            / oversampling * color_font_scaling;
    texture_flags = 0;
    if (id.mipmaps) {
        texture_flags |= Texture::FLAG_MIPMAPS;
    }
    if (id.filter) {
        texture_flags |= Texture::FLAG_FILTER;
    }
    valid = true;
    return OK;
}

Pair<const DynamicFontAtSize::CharacterData*, const DynamicFontAtSize*>
DynamicFontAtSize::get_character_data_and_font(
    const CharType character,
    const Vector<Ref<DynamicFontAtSize>>& fallbacks
) const {
    const CharacterData* character_data = get_character_data(character);
    if (character_data->found) {
        return {character_data, this};
    }
    // Character not found, try fallbacks.
    for (int i = 0; i < fallbacks.size(); i++) {
        auto* fallback = const_cast<DynamicFontAtSize*>(fallbacks[i].ptr());
        if (!fallback->valid) {
            continue;
        }
        character_data = fallback->get_character_data(character);
        if (character_data->found) {
            return {character_data, fallback};
        }
    }
    // Character not found. Try replacement character 0xFFFD.
    character_data = get_character_data(0xFFFD);
    return {character_data, const_cast<DynamicFontAtSize*>(this)};
}

const DynamicFontAtSize::CharacterData* DynamicFontAtSize::get_character_data(
    const CharType character
) const {
    _THREAD_SAFE_METHOD_
    if (character_data_cache.has(character)) {
        return character_data_cache.getptr(character);
    }
    if (FT_Get_Char_Index(ft_face, character) == 0) {
        // Font doesn't have this character.
        character_data_cache[character] = CharacterData{};
    } else {
        character_data_cache[character] = create_character_data(character);
    }
    return character_data_cache.getptr(character);
}

DynamicFontAtSize::CharacterData DynamicFontAtSize::create_character_data(
    const CharType character
) const {
    FT_Int32 load_flags = ft_hinting_from_font_hinting(font_data->hinting);
    if (FT_HAS_COLOR(ft_face)) {
        load_flags |= FT_LOAD_COLOR;
    }
    if (font_data->force_auto_hinter) {
        load_flags |= FT_LOAD_FORCE_AUTOHINT;
    }
    int error = FT_Load_Char(ft_face, character, load_flags);
    if (error) {
        return {};
    }
    if (id.outline_size > 0) {
        return create_outline_character(character);
    }

    FT_Render_Mode render_mode = FT_RENDER_MODE_NORMAL;
    if (!font_data->is_antialiased()) {
        render_mode = FT_RENDER_MODE_MONO;
    }
    error = FT_Render_Glyph(ft_face->glyph, render_mode);
    if (error) {
        return {};
    }
    const float ft_glyph_advance =
        float_from_ft_26_6(ft_face->glyph->advance.x);
    return create_bitmap_character(
        ft_face->glyph->bitmap,
        ft_face->glyph->bitmap_top,
        ft_face->glyph->bitmap_left,
        ft_glyph_advance,
        textures_cache,
        id.size,
        ascent,
        oversampling,
        color_font_scaling,
        texture_flags
    );
}

DynamicFontAtSize::CharacterData DynamicFontAtSize::create_outline_character(
    const CharType character
) const {
    FT_Int32 load_flags = FT_LOAD_NO_BITMAP;
    if (font_data->force_auto_hinter) {
        load_flags |= FT_LOAD_FORCE_AUTOHINT;
    }
    FT_Error error = FT_Load_Char(ft_face, character, load_flags);
    if (error) {
        return {};
    }

    FT_Stroker ft_stroker;
    error = FT_Stroker_New(ft_library, &ft_stroker);
    if (error) {
        return {};
    }

    const FT_Fixed radius =
        ft_26_6_from_float(static_cast<float>(id.outline_size) * oversampling);
    FT_Stroker_Set(
        ft_stroker,
        radius,
        FT_STROKER_LINECAP_BUTT,
        FT_STROKER_LINEJOIN_ROUND,
        0
    );

    FT_Glyph ft_glyph;
    error = FT_Get_Glyph(ft_face->glyph, &ft_glyph);
    if (error) {
        FT_Stroker_Done(ft_stroker);
        return {};
    }
    error = FT_Glyph_Stroke(&ft_glyph, ft_stroker, 1);
    if (error) {
        FT_Done_Glyph(ft_glyph);
        FT_Stroker_Done(ft_stroker);
        return {};
    }
    FT_Render_Mode render_mode = FT_RENDER_MODE_NORMAL;
    if (!font_data->is_antialiased()) {
        render_mode = FT_RENDER_MODE_MONO;
    }
    error = FT_Glyph_To_Bitmap(&ft_glyph, render_mode, nullptr, true);
    if (error) {
        FT_Done_Glyph(ft_glyph);
        FT_Stroker_Done(ft_stroker);
        return {};
    }

    const auto ft_bitmap_glyph   = reinterpret_cast<FT_BitmapGlyph>(ft_glyph);
    const float ft_glyph_advance = float_from_ft_16_16(ft_glyph->advance.x);
    const CharacterData character_data = create_bitmap_character(
        ft_bitmap_glyph->bitmap,
        ft_bitmap_glyph->top,
        ft_bitmap_glyph->left,
        ft_glyph_advance,
        textures_cache,
        id.size,
        ascent,
        oversampling,
        color_font_scaling,
        texture_flags
    );
    FT_Done_Glyph(ft_glyph);
    FT_Stroker_Done(ft_stroker);
    return character_data;
}

#endif // MODULE_FREETYPE_ENABLED
