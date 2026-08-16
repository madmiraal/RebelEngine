// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef BMFONT_RESOURCE_FORMAT_LOADER_H
#define BMFONT_RESOURCE_FORMAT_LOADER_H

#include "core/io/resource_loader.h"

class ResourceFormatLoaderBMFont : public ResourceFormatLoader {
public:
    void get_recognized_extensions(List<String>* extensions) const override;
    String get_resource_type(const String& path) const override;
    bool handles_type(const String& type_name) const override;
    RES load(
        const String& p_path,
        const String& p_original_path = "",
        Error* error                  = nullptr
    ) override;
};

#endif // BMFONT_RESOURCE_FORMAT_LOADER_H
