// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef BM_FONT_RESOURCE_LOADER_H
#define BM_FONT_RESOURCE_LOADER_H

#include "core/io/resource_loader.h"

class BMFontResourceLoader : public ResourceFormatLoader {
public:
    RES load(
        const String& path,
        const String& original_path = "",
        Error* error                = nullptr
    ) override;
    void get_recognized_extensions(List<String>* extensions) const override;
    bool handles_type(const String& type) const override;
    String get_resource_type(const String& path) const override;
};

#endif // BM_FONT_RESOURCE_LOADER_H
