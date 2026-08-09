// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef BITMAP_FONT_RESOURCE_FORMAT_LOADER_H
#define BITMAP_FONT_RESOURCE_FORMAT_LOADER_H

#include "core/io/resource_loader.h"

class ResourceFormatLoaderBMFont : public ResourceFormatLoader {
public:
    RES load(
        const String& p_path,
        const String& p_original_path = "",
        Error* r_error                = nullptr
    ) override;
    void get_recognized_extensions(List<String>* p_extensions) const override;
    bool handles_type(const String& p_type) const override;
    String get_resource_type(const String& p_path) const override;
};

#endif // BITMAP_FONT_RESOURCE_FORMAT_LOADER_H
