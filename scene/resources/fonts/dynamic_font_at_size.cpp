// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "dynamic_font_at_size.h"

#include "core/os/file_access.h"
#include "scene/resources/texture.h"

#include FT_STROKER_H

static constexpr int margin = 1;

float DynamicFontAtSize::font_oversampling = 1.0;

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

DynamicFontAtSize::~DynamicFontAtSize() {
    font_data->font_at_sizes_cache.erase(font_settings);
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

    float advance =
        get_kerning_advance(font_at_size->ft_face, character, next_character)
        / oversampling;
    if (character_data->found) {
        if (!advance_only && character_data->texture_index != -1) {
            draw_texture(
                canvas_item,
                position,
                character_data,
                font_at_size,
                color
            );
        }
        return advance += character_data->advance;
    }

    if (has_outline) {
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
        const CharacterData bitmap_character_data =
            create_bitmap_character(ft_face->glyph);
        advance += bitmap_character_data.advance;
    }
    return advance;
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
    textures_cache.clear();
    character_data_cache.clear();
    oversampling = font_oversampling;
    valid        = false;
    load();
}

Error DynamicFontAtSize::load() {
    const Error error = font_data->initialize();
    if (error) {
        return error;
    }
    FT_Library ft_library = font_data->get_ft_library();
    FT_Stream ft_stream   = font_data->get_ft_stream();

    FT_Open_Args ft_open_args = {};
    ft_open_args.memory_base  = font_data->font_bytes;
    ft_open_args.memory_size  = font_data->font_bytes_length;
    ft_open_args.flags        = FT_OPEN_MEMORY;
    ft_open_args.stream       = ft_stream;

    const FT_Error ft_error =
        FT_Open_Face(ft_library, &ft_open_args, 0, &ft_face);
    if (ft_error) {
        if (ft_error == FT_Err_Unknown_File_Format) {
            ERR_FAIL_V_MSG(ERR_FILE_CANT_OPEN, "Unknown font format.");
        }
        ERR_FAIL_V_MSG(ERR_FILE_CANT_OPEN, "Error loading font.");
    }

    if (FT_HAS_COLOR(ft_face) && ft_face->num_fixed_sizes > 0) {
        int best_index = 0;
        int best_difference =
            ABS(font_settings.font_size
                - ((int64_t)(ft_face->available_sizes[0].width)));
        for (int i = 1; i < ft_face->num_fixed_sizes; i++) {
            const int this_difference =
                ABS(font_settings.font_size
                    - ((int64_t)(ft_face->available_sizes[i].width)));
            if (this_difference < best_difference) {
                best_index      = i;
                best_difference = this_difference;
            }
        }
        color_font_scaling =
            static_cast<float>(font_settings.font_size) * oversampling
            / static_cast<float>(ft_face->available_sizes[best_index].width);
        FT_Select_Size(ft_face, best_index);
    } else {
        const auto oversampled_size = static_cast<FT_UInt>(
            static_cast<float>(font_settings.font_size) * oversampling
        );
        FT_Set_Pixel_Sizes(ft_face, 0, oversampled_size);
    }

    ascent = float_from_ft_26_6(ft_face->size->metrics.ascender) / oversampling
           * color_font_scaling;
    descent = -float_from_ft_26_6(ft_face->size->metrics.descender)
            / oversampling * color_font_scaling;
    line_gap      = 0;
    texture_flags = 0;
    if (font_settings.use_mipmaps) {
        texture_flags |= Texture::FLAG_MIPMAPS;
    }
    if (font_settings.use_filter) {
        texture_flags |= Texture::FLAG_FILTER;
    }
    valid = true;
    return OK;
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
    if (font_settings.outline_thickness > 0) {
        return create_outline_character(character);
    }

    FT_Render_Mode render_mode = FT_RENDER_MODE_MONO;
    if (font_data->is_antialiased()) {
        render_mode = FT_RENDER_MODE_NORMAL;
    }
    error = FT_Render_Glyph(ft_face->glyph, render_mode);
    if (error) {
        return {};
    }
    return create_bitmap_character(ft_face->glyph);
}

DynamicFontAtSize::CharacterData DynamicFontAtSize::create_bitmap_character(
    const FT_GlyphSlot& ft_glyph_slot
) const {
    const float ft_glyph_advance = float_from_ft_26_6(ft_glyph_slot->advance.x);
    return create_bitmap_character(
        ft_glyph_slot->bitmap,
        ft_glyph_slot->bitmap_top,
        ft_glyph_slot->bitmap_left,
        ft_glyph_advance
    );
}

DynamicFontAtSize::CharacterData DynamicFontAtSize::create_bitmap_character(
    const FT_Glyph& ft_glyph
) const {
    const auto ft_bitmap_glyph   = reinterpret_cast<FT_BitmapGlyph>(ft_glyph);
    const float ft_glyph_advance = float_from_ft_16_16(ft_glyph->advance.x);
    return create_bitmap_character(
        ft_bitmap_glyph->bitmap,
        ft_bitmap_glyph->top,
        ft_bitmap_glyph->left,
        ft_glyph_advance
    );
}

DynamicFontAtSize::CharacterData DynamicFontAtSize::create_bitmap_character(
    const FT_Bitmap& bitmap,
    const FT_Int top,
    const FT_Int left,
    const float ft_glyph_advance
) const {
    const int bitmap_width       = static_cast<int>(bitmap.width);
    const int bitmap_rows        = static_cast<int>(bitmap.rows);
    const int width_with_margin  = static_cast<int>(bitmap.width) + margin * 2;
    const int height_with_margin = static_cast<int>(bitmap.rows) + margin * 2;
    ERR_FAIL_COND_V(width_with_margin > 4096, CharacterData{});
    ERR_FAIL_COND_V(height_with_margin > 4096, CharacterData{});
    const Image::Format image_format = image_format_from_ft_pixel_mode(
        static_cast<FT_Pixel_Mode>(bitmap.pixel_mode)
    );
    TextureLocation texture_location = get_texture_location(
        image_format,
        width_with_margin,
        height_with_margin
    );
    ERR_FAIL_COND_V(texture_location.texture_index < 0, CharacterData{});

    // Update cached character texture's image data.
    CharacterTexture& character_texture =
        textures_cache.write[texture_location.texture_index];
    const PoolVector<unsigned char>::Write image_data =
        character_texture.image_data.write();
    const int bytes_per_pixel = Image::get_format_pixel_size(image_format);
    for (int i = 0; i < bitmap_rows; i++) {
        for (int j = 0; j < bitmap_width; j++) {
            const int offset = ((i + texture_location.y_offset + margin)
                                    * character_texture.texture_size
                                + j + texture_location.x_offset + margin)
                             * bytes_per_pixel;
            ERR_FAIL_COND_V(
                offset >= character_texture.image_data.size(),
                CharacterData{}
            );
            switch (bitmap.pixel_mode) {
                case FT_PIXEL_MODE_MONO: {
                    // 1 bit per pixel.
                    const int byte         = i * bitmap.pitch + (j >> 3);
                    const int bit          = 1 << (7 - j % 8);
                    image_data[offset + 0] = 255; // grayscale as 1
                    image_data[offset + 1] =
                        bitmap.buffer[byte] & bit ? 255 : 0;
                } break;
                case FT_PIXEL_MODE_GRAY:
                    // 8 bits per pixel.
                    // TODO: Check number of gray levels in num_grays.
                    image_data[offset + 0] = 255; // grayscale as 1
                    image_data[offset + 1] =
                        bitmap.buffer[i * bitmap.pitch + j];
                    break;
                case FT_PIXEL_MODE_BGRA: {
                    const int source_offset = i * bitmap.pitch + (j << 2);
                    image_data[offset + 2]  = bitmap.buffer[source_offset + 0];
                    image_data[offset + 1]  = bitmap.buffer[source_offset + 1];
                    image_data[offset + 0]  = bitmap.buffer[source_offset + 2];
                    image_data[offset + 3]  = bitmap.buffer[source_offset + 3];
                } break;
                // TODO: FT_PIXEL_MODE_LCD
                default:
                    ERR_FAIL_V_MSG(
                        CharacterData{},
                        "Font uses unsupported pixel format: "
                            + itos(bitmap.pixel_mode) + "."
                    );
                    break;
            }
        }
    }

    // Update cached character texture's texture.
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

    // Update cached character texture's offsets.
    for (int k = texture_location.x_offset;
         k < texture_location.x_offset + width_with_margin;
         k++) {
        character_texture.offsets.write[k] =
            texture_location.y_offset + height_with_margin;
    }

    // Create Character Data.
    const float scaling = color_font_scaling / oversampling;
    const float x_position =
        static_cast<float>(texture_location.x_offset + margin) / oversampling;
    const float y_position =
        static_cast<float>(texture_location.y_offset + margin) / oversampling;
    const auto width        = static_cast<float>(bitmap.width) * scaling;
    const auto height       = static_cast<float>(bitmap.rows) * scaling;
    const int uv_x_position = texture_location.x_offset + margin;
    const int uv_y_position = texture_location.y_offset + margin;
    const int uv_width      = bitmap_width;
    const int uv_height     = bitmap_rows;
    const Rect2 rect{x_position, y_position, width, height};
    const Rect2i uv_rect{uv_x_position, uv_y_position, uv_width, uv_height};
    const float horizontal_offset = static_cast<float>(left) * scaling;
    const float vertical_offset   = ascent - static_cast<float>(top) * scaling;
    const float advance           = ft_glyph_advance * scaling;
    return {
        rect,
        uv_rect,
        texture_location.texture_index,
        horizontal_offset,
        vertical_offset,
        advance,
        true
    };
}

DynamicFontAtSize::CharacterData DynamicFontAtSize::create_outline_character(
    const CharType character
) const {
    FT_Library ft_library = font_data->get_ft_library();
    ERR_FAIL_NULL_V_MSG(ft_library, {}, "FreeType not initialized.");

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

    const FT_Fixed radius = ft_26_6_from_float(
        static_cast<float>(font_settings.outline_thickness) * oversampling
    );
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

    const CharacterData character_data = create_bitmap_character(ft_glyph);
    FT_Done_Glyph(ft_glyph);
    FT_Stroker_Done(ft_stroker);
    return character_data;
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

DynamicFontAtSize::TextureLocation DynamicFontAtSize::
    find_cached_texture_location(
        const Image::Format& image_format,
        const int width,
        const int height
    ) const {
    for (int i = 0; i < textures_cache.size(); i++) {
        const CharacterTexture& character_texture = textures_cache[i];
        if (character_texture.texture->get_format() != image_format
            || character_texture.texture_size < width
            || character_texture.texture_size < height) {
            continue;
        }

        int x_offset = 0;
        int y_offset = 0x7FFFFFFF;
        for (int j = 0; j < character_texture.texture_size - width; j++) {
            int max_y = 0;
            for (int k = j; k < j + width; k++) {
                int y = character_texture.offsets[k];
                if (y > max_y) {
                    max_y = y;
                }
            }
            if (max_y < y_offset) {
                x_offset = j;
                y_offset = max_y;
            }
        }

        if (y_offset == 0x7FFFFFFF
            || y_offset + height > character_texture.texture_size) {
            continue;
        }
        return {i, x_offset, y_offset};
    }
    return {};
}

DynamicFontAtSize::TextureLocation DynamicFontAtSize::get_texture_location(
    const Image::Format& image_format,
    const int width,
    const int height
) const {
    const TextureLocation texture_location =
        find_cached_texture_location(image_format, width, height);
    if (texture_location.texture_index != -1) {
        // Cached texture found.
        return texture_location;
    }
    return create_new_cached_texture_location(image_format, width, height);
}

DynamicFontAtSize::TextureLocation DynamicFontAtSize::
    create_new_cached_texture_location(
        const Image::Format& image_format,
        const int width,
        const int height
    ) const {
    const int bytes_per_pixel = Image::get_format_pixel_size(image_format);
    int texture_size = MAX(font_settings.font_size * oversampling * 8, 256);
    if (width > texture_size) {
        // Special case! Adapt to it?
        texture_size = width;
    }
    if (height > texture_size) {
        // Special case! Adapt to it?
        texture_size = height;
    }
    texture_size = static_cast<int>(next_power_of_2(texture_size));
    texture_size = MIN(texture_size, 4096);

    CharacterTexture character_texture;
    character_texture.texture_size = texture_size;
    character_texture.image_data.resize(
        texture_size * texture_size * bytes_per_pixel
    );
    // Initialize the texture to all-white pixels to prevent artifacts when the
    // font is displayed at a non-default scale with filtering enabled.
    const PoolVector<unsigned char>::Write image_data =
        character_texture.image_data.write();
    if (bytes_per_pixel == 2) {
        for (int i  = 0; i < texture_size * texture_size * bytes_per_pixel;
             i     += 2) {
            image_data[i + 0] = 255;
            image_data[i + 1] = 0;
        }
    } else {
        for (int i  = 0; i < texture_size * texture_size * bytes_per_pixel;
             i     += 4) {
            image_data[i + 0] = 255;
            image_data[i + 1] = 255;
            image_data[i + 2] = 255;
            image_data[i + 3] = 0;
        }
    }
    // Initialize all offsets to 0.
    character_texture.offsets.resize(texture_size);
    for (int i = 0; i < texture_size; i++) {
        character_texture.offsets.write[i] = 0;
    }

    textures_cache.push_back(character_texture);
    return {textures_cache.size() - 1, 0, 0};
}

void DynamicFontAtSize::draw_texture(
    const RID canvas_item,
    const Point2& position,
    const CharacterData* character_data,
    const DynamicFontAtSize* font_at_size,
    const Color& color
) {
    const float x_position = position.x + character_data->horizontal_offset;
    const float y_position =
        position.y + character_data->vertical_offset - font_at_size->ascent;
    const Point2 character_position{x_position, y_position};
    const Rect2 character_rect{character_position, character_data->rect.size};
    Color modulate = color;
    if (FT_HAS_COLOR(font_at_size->ft_face)) {
        modulate.r = modulate.g = modulate.b = 1.0;
    }
    const RID texture =
        font_at_size->textures_cache[character_data->texture_index]
            .texture->get_rid();
    VisualServer::get_singleton()->canvas_item_add_texture_rect_region(
        canvas_item,
        character_rect,
        texture,
        character_data->uv_rect,
        modulate,
        false,
        RID(),
        false
    );
}
