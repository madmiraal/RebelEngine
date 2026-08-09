// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "dynamic_font_at_size.h"

#include "core/os/file_access.h"

#include FT_STROKER_H

Error DynamicFontAtSize::_load() {
    int error = FT_Init_FreeType(&library);

    ERR_FAIL_COND_V_MSG(
        error != 0,
        ERR_CANT_CREATE,
        "Error initializing FreeType."
    );

    if (font->font_mem == nullptr && font->font_path != String()) {
        FileAccess* f = FileAccess::open(font->font_path, FileAccess::READ);
        if (!f) {
            FT_Done_FreeType(library);
            ERR_FAIL_V_MSG(
                ERR_CANT_OPEN,
                "Cannot open font file '" + font->font_path + "'."
            );
        }

        uint64_t len    = f->get_len();
        font->_fontdata = Vector<uint8_t>();
        font->_fontdata.resize(len);
        f->get_buffer(font->_fontdata.ptrw(), len);
        font->set_font_ptr(font->_fontdata.ptr(), len);
        f->close();
        memdelete(f);
    }

    if (font->font_mem) {
        memset(&stream, 0, sizeof(FT_StreamRec));
        stream.base = (unsigned char*)font->font_mem;
        stream.size = font->font_mem_size;
        stream.pos  = 0;

        FT_Open_Args fargs;
        memset(&fargs, 0, sizeof(FT_Open_Args));
        fargs.memory_base = (unsigned char*)font->font_mem;
        fargs.memory_size = font->font_mem_size;
        fargs.flags       = FT_OPEN_MEMORY;
        fargs.stream      = &stream;
        error             = FT_Open_Face(library, &fargs, 0, &face);

    } else {
        FT_Done_FreeType(library);
        ERR_FAIL_V_MSG(ERR_UNCONFIGURED, "DynamicFont uninitialized.");
    }

    // error = FT_New_Face( library, src_path.utf8().get_data(),0,&face );

    if (error == FT_Err_Unknown_File_Format) {
        FT_Done_FreeType(library);
        ERR_FAIL_V_MSG(ERR_FILE_CANT_OPEN, "Unknown font format.");

    } else if (error) {
        FT_Done_FreeType(library);
        ERR_FAIL_V_MSG(ERR_FILE_CANT_OPEN, "Error loading font.");
    }

    if (FT_HAS_COLOR(face) && face->num_fixed_sizes > 0) {
        int best_match = 0;
        int diff = ABS(id.size - ((int64_t)face->available_sizes[0].width));
        scale_color_font =
            float(id.size * oversampling) / face->available_sizes[0].width;
        for (int i = 1; i < face->num_fixed_sizes; i++) {
            int ndiff =
                ABS(id.size - ((int64_t)face->available_sizes[i].width));
            if (ndiff < diff) {
                best_match       = i;
                diff             = ndiff;
                scale_color_font = float(id.size * oversampling)
                                 / face->available_sizes[i].width;
            }
        }
        FT_Select_Size(face, best_match);
    } else {
        FT_Set_Pixel_Sizes(face, 0, id.size * oversampling);
    }

    ascent =
        (face->size->metrics.ascender / 64.0) / oversampling * scale_color_font;
    descent = (-face->size->metrics.descender / 64.0) / oversampling
            * scale_color_font;
    linegap       = 0;
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

float DynamicFontAtSize::font_oversampling = 1.0;

float DynamicFontAtSize::get_height() const {
    return ascent + descent;
}

float DynamicFontAtSize::get_ascent() const {
    return ascent;
}

float DynamicFontAtSize::get_descent() const {
    return descent;
}

const Pair<const DynamicFontAtSize::Character*, DynamicFontAtSize*>
DynamicFontAtSize::_find_char_with_font(
    CharType p_char,
    const Vector<Ref<DynamicFontAtSize>>& p_fallbacks
) const {
    const Character* chr = char_map.getptr(p_char);
    ERR_FAIL_COND_V(
        !chr,
        (Pair<const Character*, DynamicFontAtSize*>(NULL, NULL))
    );

    if (!chr->found) {
        // not found, try in fallbacks
        for (int i = 0; i < p_fallbacks.size(); i++) {
            DynamicFontAtSize* fb =
                const_cast<DynamicFontAtSize*>(p_fallbacks[i].ptr());
            if (!fb->valid) {
                continue;
            }

            fb->_update_char(p_char);
            const Character* fallback_chr = fb->char_map.getptr(p_char);
            ERR_CONTINUE(!fallback_chr);

            if (!fallback_chr->found) {
                continue;
            }

            return Pair<const Character*, DynamicFontAtSize*>(fallback_chr, fb);
        }

        // not found, try 0xFFFD to display 'not found'.
        const_cast<DynamicFontAtSize*>(this)->_update_char(0xFFFD);
        chr = char_map.getptr(0xFFFD);
        ERR_FAIL_COND_V(
            !chr,
            (Pair<const Character*, DynamicFontAtSize*>(NULL, NULL))
        );
    }

    return Pair<const Character*, DynamicFontAtSize*>(
        chr,
        const_cast<DynamicFontAtSize*>(this)
    );
}

float DynamicFontAtSize::_get_kerning_advance(
    const DynamicFontAtSize* font,
    CharType p_char,
    CharType p_next
) const {
    float advance = 0.0;

    if (p_next) {
        FT_Vector delta;
        FT_Get_Kerning(
            font->face,
            FT_Get_Char_Index(font->face, p_char),
            FT_Get_Char_Index(font->face, p_next),
            FT_KERNING_DEFAULT,
            &delta
        );
        advance = (delta.x / 64.0) / oversampling;
    }

    return advance;
}

Size2 DynamicFontAtSize::get_char_size(
    CharType p_char,
    CharType p_next,
    const Vector<Ref<DynamicFontAtSize>>& p_fallbacks
) const {
    if (!valid) {
        return Size2(1, 1);
    }
    const_cast<DynamicFontAtSize*>(this)->_update_char(p_char);

    Pair<const Character*, DynamicFontAtSize*> char_pair_with_font =
        _find_char_with_font(p_char, p_fallbacks);
    const Character* ch     = char_pair_with_font.first;
    DynamicFontAtSize* font = char_pair_with_font.second;
    ERR_FAIL_COND_V(!ch, Size2());

    Size2 ret(0, get_height());

    if (ch->found) {
        ret.x = ch->advance;
    }
    ret.x += _get_kerning_advance(font, p_char, p_next);

    return ret;
}

String DynamicFontAtSize::get_available_chars() const {
    if (!valid) {
        return "";
    }

    String chars;

    FT_UInt gindex;
    FT_ULong charcode = FT_Get_First_Char(face, &gindex);
    while (gindex != 0) {
        if (charcode != 0) {
            chars += CharType(charcode);
        }
        charcode = FT_Get_Next_Char(face, charcode, &gindex);
    }

    return chars;
}

void DynamicFontAtSize::set_texture_flags(uint32_t p_flags) {
    texture_flags = p_flags;
    for (int i = 0; i < textures.size(); i++) {
        Ref<ImageTexture>& tex = textures.write[i].texture;
        if (!tex.is_null()) {
            tex->set_flags(p_flags);
        }
    }
}

float DynamicFontAtSize::draw_char(
    RID p_canvas_item,
    const Point2& p_pos,
    CharType p_char,
    CharType p_next,
    const Color& p_modulate,
    const Vector<Ref<DynamicFontAtSize>>& p_fallbacks,
    bool p_advance_only,
    bool p_outline
) const {
    if (!valid) {
        return 0;
    }

    const_cast<DynamicFontAtSize*>(this)->_update_char(p_char);

    Pair<const Character*, DynamicFontAtSize*> char_pair_with_font =
        _find_char_with_font(p_char, p_fallbacks);
    const Character* ch     = char_pair_with_font.first;
    DynamicFontAtSize* font = char_pair_with_font.second;

    ERR_FAIL_COND_V(!ch, 0.0);

    float advance = 0.0;

    // use normal character size if there's no outline character
    if (p_outline && !ch->found) {
        FT_GlyphSlot slot = face->glyph;
        int error         = FT_Load_Char(
            face,
            p_char,
            FT_HAS_COLOR(face) ? FT_LOAD_COLOR : FT_LOAD_DEFAULT
        );
        if (!error) {
            error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
            if (!error) {
                Character character = Character::not_found();
                character =
                    const_cast<DynamicFontAtSize*>(this)->_bitmap_to_character(
                        slot->bitmap,
                        slot->bitmap_top,
                        slot->bitmap_left,
                        slot->advance.x / 64.0
                    );
                advance = character.advance;
            }
        }
    }

    if (ch->found) {
        ERR_FAIL_COND_V(
            ch->texture_idx < -1 || ch->texture_idx >= font->textures.size(),
            0
        );

        if (!p_advance_only && ch->texture_idx != -1) {
            Point2 cpos     = p_pos;
            cpos.x         += ch->h_align;
            cpos.y         -= font->get_ascent();
            cpos.y         += ch->v_align;
            Color modulate  = p_modulate;
            if (FT_HAS_COLOR(font->face)) {
                modulate.r = modulate.g = modulate.b = 1.0;
            }
            RID texture = font->textures[ch->texture_idx].texture->get_rid();
            VisualServer::get_singleton()->canvas_item_add_texture_rect_region(
                p_canvas_item,
                Rect2(cpos, ch->rect.size),
                texture,
                ch->rect_uv,
                modulate,
                false,
                RID(),
                false
            );
        }

        advance = ch->advance;
    }

    advance += _get_kerning_advance(font, p_char, p_next);

    return advance;
}

DynamicFontAtSize::Character DynamicFontAtSize::Character::not_found() {
    Character ch;
    ch.texture_idx = -1;
    ch.advance     = 0;
    ch.h_align     = 0;
    ch.v_align     = 0;
    ch.found       = false;
    return ch;
}

DynamicFontAtSize::TexturePosition DynamicFontAtSize::
    _find_texture_pos_for_glyph(
        int p_color_size,
        Image::Format p_image_format,
        int p_width,
        int p_height
    ) {
    TexturePosition ret;
    ret.index = -1;
    ret.x     = 0;
    ret.y     = 0;

    int mw = p_width;
    int mh = p_height;

    for (int i = 0; i < textures.size(); i++) {
        const CharTexture& ct = textures[i];

        if (ct.texture->get_format() != p_image_format) {
            continue;
        }

        if (mw > ct.texture_size
            || mh > ct.texture_size) { // too big for this texture
            continue;
        }

        ret.y = 0x7FFFFFFF;
        ret.x = 0;

        for (int j = 0; j < ct.texture_size - mw; j++) {
            int max_y = 0;

            for (int k = j; k < j + mw; k++) {
                int y = ct.offsets[k];
                if (y > max_y) {
                    max_y = y;
                }
            }

            if (max_y < ret.y) {
                ret.y = max_y;
                ret.x = j;
            }
        }

        if (ret.y == 0x7FFFFFFF || ret.y + mh > ct.texture_size) {
            continue; // fail, could not fit it here
        }

        ret.index = i;
        break;
    }

    if (ret.index == -1) {
        // could not find texture to fit, create one
        ret.x = 0;
        ret.y = 0;

        int texsize = MAX(id.size * oversampling * 8, 256);
        if (mw > texsize) {
            texsize = mw; // special case, adapt to it?
        }
        if (mh > texsize) {
            texsize = mh; // special case, adapt to it?
        }

        texsize = next_power_of_2(texsize);

        texsize = MIN(texsize, 4096);

        CharTexture tex;
        tex.texture_size = texsize;
        tex.imgdata.resize(texsize * texsize * p_color_size); // grayscale alpha

        {
            // zero texture
            PoolVector<uint8_t>::Write w = tex.imgdata.write();
            ERR_FAIL_COND_V(
                texsize * texsize * p_color_size > tex.imgdata.size(),
                ret
            );

            // Initialize the texture to all-white pixels to prevent artifacts
            // when the font is displayed at a non-default scale with filtering
            // enabled.
            if (p_color_size == 2) {
                for (int i = 0; i < texsize * texsize * p_color_size; i += 2) {
                    w[i + 0] = 255;
                    w[i + 1] = 0;
                }
            } else {
                for (int i = 0; i < texsize * texsize * p_color_size; i += 4) {
                    w[i + 0] = 255;
                    w[i + 1] = 255;
                    w[i + 2] = 255;
                    w[i + 3] = 0;
                }
            }
        }
        tex.offsets.resize(texsize);
        for (int i = 0; i < texsize; i++) { // zero offsets
            tex.offsets.write[i] = 0;
        }

        textures.push_back(tex);
        ret.index = textures.size() - 1;
    }

    return ret;
}

DynamicFontAtSize::Character DynamicFontAtSize::_bitmap_to_character(
    FT_Bitmap bitmap,
    int yofs,
    int xofs,
    float advance
) {
    int w = bitmap.width;
    int h = bitmap.rows;

    int mw = w + rect_margin * 2;
    int mh = h + rect_margin * 2;

    ERR_FAIL_COND_V(mw > 4096, Character::not_found());
    ERR_FAIL_COND_V(mh > 4096, Character::not_found());

    int color_size = bitmap.pixel_mode == FT_PIXEL_MODE_BGRA ? 4 : 2;
    Image::Format require_format =
        color_size == 4 ? Image::FORMAT_RGBA8 : Image::FORMAT_LA8;

    TexturePosition tex_pos =
        _find_texture_pos_for_glyph(color_size, require_format, mw, mh);
    ERR_FAIL_COND_V(tex_pos.index < 0, Character::not_found());

    // fit character in char texture

    CharTexture& tex = textures.write[tex_pos.index];

    {
        PoolVector<uint8_t>::Write wr = tex.imgdata.write();

        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                int ofs = ((i + tex_pos.y + rect_margin) * tex.texture_size + j
                           + tex_pos.x + rect_margin)
                        * color_size;
                ERR_FAIL_COND_V(
                    ofs >= tex.imgdata.size(),
                    Character::not_found()
                );
                switch (bitmap.pixel_mode) {
                    case FT_PIXEL_MODE_MONO: {
                        int byte    = i * bitmap.pitch + (j >> 3);
                        int bit     = 1 << (7 - (j % 8));
                        wr[ofs + 0] = 255; // grayscale as 1
                        wr[ofs + 1] = (bitmap.buffer[byte] & bit) ? 255 : 0;
                    } break;
                    case FT_PIXEL_MODE_GRAY:
                        wr[ofs + 0] = 255; // grayscale as 1
                        wr[ofs + 1] = bitmap.buffer[i * bitmap.pitch + j];
                        break;
                    case FT_PIXEL_MODE_BGRA: {
                        int ofs_color = i * bitmap.pitch + (j << 2);
                        wr[ofs + 2]   = bitmap.buffer[ofs_color + 0];
                        wr[ofs + 1]   = bitmap.buffer[ofs_color + 1];
                        wr[ofs + 0]   = bitmap.buffer[ofs_color + 2];
                        wr[ofs + 3]   = bitmap.buffer[ofs_color + 3];
                    } break;
                    // TODO: FT_PIXEL_MODE_LCD
                    default:
                        ERR_FAIL_V_MSG(
                            Character::not_found(),
                            "Font uses unsupported pixel format: "
                                + itos(bitmap.pixel_mode) + "."
                        );
                        break;
                }
            }
        }
    }

    // blit to image and texture
    {
        Ref<Image> img = memnew(Image(
            tex.texture_size,
            tex.texture_size,
            0,
            require_format,
            tex.imgdata
        ));

        if (tex.texture.is_null()) {
            tex.texture.instance();
            tex.texture->create_from_image(
                img,
                Texture::FLAG_VIDEO_SURFACE | texture_flags
            );
        } else {
            tex.texture->set_data(img); // update
        }
    }

    // update height array

    for (int k = tex_pos.x; k < tex_pos.x + mw; k++) {
        tex.offsets.write[k] = tex_pos.y + mh;
    }

    Character chr;
    chr.h_align = xofs * scale_color_font / oversampling;
    chr.v_align =
        ascent
        - (yofs * scale_color_font / oversampling); // + ascent - descent;
    chr.advance     = advance * scale_color_font / oversampling;
    chr.texture_idx = tex_pos.index;
    chr.found       = true;

    chr.rect_uv = Rect2(tex_pos.x + rect_margin, tex_pos.y + rect_margin, w, h);
    chr.rect    = chr.rect_uv;
    chr.rect.position /= oversampling;
    chr.rect.size      = chr.rect.size * scale_color_font / oversampling;
    return chr;
}

DynamicFontAtSize::Character DynamicFontAtSize::_make_outline_char(
    CharType p_char
) {
    Character ret = Character::not_found();

    if (FT_Load_Char(
            face,
            p_char,
            FT_LOAD_NO_BITMAP
                | (font->force_autohinter ? FT_LOAD_FORCE_AUTOHINT : 0)
        )
        != 0) {
        return ret;
    }

    FT_Stroker stroker;
    if (FT_Stroker_New(library, &stroker) != 0) {
        return ret;
    }

    FT_Stroker_Set(
        stroker,
        (int)(id.outline_size * oversampling * 64.0),
        FT_STROKER_LINECAP_BUTT,
        FT_STROKER_LINEJOIN_ROUND,
        0
    );
    FT_Glyph glyph;
    FT_BitmapGlyph glyph_bitmap;

    if (FT_Get_Glyph(face->glyph, &glyph) != 0) {
        goto cleanup_stroker;
    }
    if (FT_Glyph_Stroke(&glyph, stroker, 1) != 0) {
        goto cleanup_glyph;
    }
    if (FT_Glyph_To_Bitmap(
            &glyph,
            font->antialiased ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO,
            nullptr,
            1
        )
        != 0) {
        goto cleanup_glyph;
    }

    glyph_bitmap = (FT_BitmapGlyph)glyph;
    ret          = _bitmap_to_character(
        glyph_bitmap->bitmap,
        glyph_bitmap->top,
        glyph_bitmap->left,
        glyph->advance.x / 65536.0
    );

cleanup_glyph:
    FT_Done_Glyph(glyph);
cleanup_stroker:
    FT_Stroker_Done(stroker);
    return ret;
}

void DynamicFontAtSize::_update_char(CharType p_char) {
    if (char_map.has(p_char)) {
        return;
    }

    _THREAD_SAFE_METHOD_

    Character character = Character::not_found();

    FT_GlyphSlot slot = face->glyph;

    if (FT_Get_Char_Index(face, p_char) == 0) {
        char_map[p_char] = character;
        return;
    }

    int ft_hinting;

    switch (font->hinting) {
        case DynamicFontData::HINTING_NONE:
            ft_hinting = FT_LOAD_NO_HINTING;
            break;
        case DynamicFontData::HINTING_LIGHT:
            ft_hinting = FT_LOAD_TARGET_LIGHT;
            break;
        default:
            ft_hinting = FT_LOAD_TARGET_NORMAL;
            break;
    }

    int error = FT_Load_Char(
        face,
        p_char,
        FT_HAS_COLOR(face)
            ? FT_LOAD_COLOR
            : FT_LOAD_DEFAULT
                  | (font->force_autohinter ? FT_LOAD_FORCE_AUTOHINT : 0)
                  | ft_hinting
    );
    if (error) {
        char_map[p_char] = character;
        return;
    }

    if (id.outline_size > 0) {
        character = _make_outline_char(p_char);
    } else {
        error = FT_Render_Glyph(
            face->glyph,
            font->antialiased ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO
        );
        if (!error) {
            character = _bitmap_to_character(
                slot->bitmap,
                slot->bitmap_top,
                slot->bitmap_left,
                slot->advance.x / 64.0
            );
        }
    }

    char_map[p_char] = character;
}

void DynamicFontAtSize::update_oversampling() {
    if (oversampling == font_oversampling || !valid) {
        return;
    }

    FT_Done_FreeType(library);
    textures.clear();
    char_map.clear();
    oversampling = font_oversampling;
    valid        = false;
    _load();
}

DynamicFontAtSize::DynamicFontAtSize() {
    valid            = false;
    rect_margin      = 1;
    ascent           = 1;
    descent          = 1;
    linegap          = 1;
    texture_flags    = 0;
    oversampling     = font_oversampling;
    scale_color_font = 1;
}

DynamicFontAtSize::~DynamicFontAtSize() {
    if (valid) {
        FT_Done_FreeType(library);
    }
    font->size_cache.erase(id);
    font.unref();
}
