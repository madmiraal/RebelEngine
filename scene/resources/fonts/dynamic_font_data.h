// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef DYNAMIC_FONT_DATA_H
#define DYNAMIC_FONT_DATA_H

#include "core/resource.h"

class DynamicFontAtSize;

class DynamicFontData : public Resource {
    GDCLASS(DynamicFontData, Resource);

public:
    struct CacheID {
        union {
            struct {
                uint32_t size         : 16;
                uint32_t outline_size : 8;
                uint32_t mipmaps      : 1;
                uint32_t filter       : 1;
                uint32_t unused       : 6;
            };

            uint32_t key;
        };

        bool operator<(CacheID right) const;

        CacheID() {
            key = 0;
        }
    };

    enum Hinting {
        HINTING_NONE,
        HINTING_LIGHT,
        HINTING_NORMAL
    };

    bool is_antialiased() const;
    void set_antialiased(bool p_antialiased);
    Hinting get_hinting() const;
    void set_hinting(Hinting p_hinting);

private:
    const uint8_t* font_mem;
    int font_mem_size;
    bool antialiased;
    bool force_autohinter;
    Hinting hinting;
    Vector<uint8_t> _fontdata;

    String font_path;
    Map<CacheID, DynamicFontAtSize*> size_cache;

    friend class DynamicFontAtSize;

    friend class DynamicFont;

    Ref<DynamicFontAtSize> _get_dynamic_font_at_size(CacheID p_cache_id);

protected:
    static void _bind_methods();

public:
    void set_font_ptr(const uint8_t* p_font_mem, int p_font_mem_size);
    void set_font_path(const String& p_path);
    String get_font_path() const;
    void set_force_autohinter(bool p_force);

    DynamicFontData();
    ~DynamicFontData() override;
};

VARIANT_ENUM_CAST(DynamicFontData::Hinting);

#endif // DYNAMIC_FONT_DATA_H
