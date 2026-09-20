// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "dynamic_fonts.h"

#include "modules/modules_enabled.gen.h"
#ifdef MODULE_FREETYPE_ENABLED

#include "core/resource.h"
#include "scene/resources/fonts/dynamic_font.h"

namespace DynamicFonts {
namespace {
SelfList<DynamicFont>::List dynamic_fonts;
Mutex mutex;
float oversampling = 1;
} // namespace

float get_oversampling() {
    return oversampling;
}

void set_oversampling(const float new_oversampling) {
    if (new_oversampling == oversampling) {
        return;
    }
    oversampling = new_oversampling;

    const SelfList<DynamicFont>* E = dynamic_fonts.get_first();
    while (E) {
        E->get_self()->update_oversampling();
        E = E->get_next();
    }
}

void add(SelfList<DynamicFont>& new_dynamic_font) {
    MutexLock lock(mutex);
    dynamic_fonts.add(&new_dynamic_font);
}

void remove(SelfList<DynamicFont>& dynamic_font) {
    MutexLock lock(mutex);
    dynamic_fonts.remove(&dynamic_font);
}
} // namespace DynamicFonts

#endif // MODULE_FREETYPE_ENABLED
