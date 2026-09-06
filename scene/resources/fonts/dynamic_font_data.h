//
// Created by marcel on 03/09/2026.
//

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
    void set_antialiased(bool new_antialiased);

    String get_font_path() const;
    void set_font_path(const String& new_font_path);
    void set_font_bytes(
        const unsigned char* new_font_bytes,
        int new_font_bytes_length
    );

    Hinting get_hinting() const;
    void set_hinting(Hinting new_hinting);
    void set_force_auto_hinter(bool new_force_auto_hinting);

    Ref<DynamicFontAtSize> get_font_at_size(CacheID cache_id);

protected:
    static void _bind_methods();

private:
    friend class DynamicFont;
    friend class DynamicFontAtSize;

    String font_path;
    Vector<unsigned char> font_data;
    Hinting hinting = HINTING_NORMAL;
    Map<CacheID, DynamicFontAtSize*> font_at_sizes_cache;

    const unsigned char* font_bytes = nullptr;
    int font_bytes_length           = 0;
    bool antialiased                = true;
    bool force_auto_hinter          = false;
};

VARIANT_ENUM_CAST(DynamicFontData::Hinting);

#endif // DYNAMIC_FONT_DATA_H
