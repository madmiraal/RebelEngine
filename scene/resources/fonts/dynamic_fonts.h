// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef DYNAMIC_FONTS_H
#define DYNAMIC_FONTS_H

#include "modules/modules_enabled.gen.h"
#ifdef MODULE_FREETYPE_ENABLED

#include "core/self_list.h"

class DynamicFont;

namespace DynamicFonts {
float get_oversampling();
void set_oversampling(float new_oversampling);

void add(SelfList<DynamicFont>& new_dynamic_font);
void remove(SelfList<DynamicFont>& dynamic_font);
} // namespace DynamicFonts

#endif // MODULE_FREETYPE_ENABLED

#endif // DYNAMIC_FONTS_H
