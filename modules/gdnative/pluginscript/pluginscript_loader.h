// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef PLUGINSCRIPT_LOADER_H
#define PLUGINSCRIPT_LOADER_H

// Rebel imports
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/script_language.h"

class PluginScriptLanguage;

class ResourceFormatLoaderPluginScript : public ResourceFormatLoader {
    PluginScriptLanguage* _language;

public:
    ResourceFormatLoaderPluginScript(PluginScriptLanguage* language);
    RES load(
        const String& p_path,
        const String& p_original_path = "",
        Error* r_error                = nullptr
    ) override;
    void get_recognized_extensions(List<String>* p_extensions) const override;
    bool handles_type(const String& p_type) const override;
    String get_resource_type(const String& p_path) const override;
};

class ResourceFormatSaverPluginScript : public ResourceFormatSaver {
    PluginScriptLanguage* _language;

public:
    ResourceFormatSaverPluginScript(PluginScriptLanguage* language);
    Error save(
        const String& p_path,
        const RES& p_resource,
        uint32_t p_flags = 0
    ) override;
    void get_recognized_extensions(
        const RES& p_resource,
        List<String>* p_extensions
    ) const override;
    bool recognize(const RES& p_resource) const override;
};

#endif // PLUGINSCRIPT_LOADER_H
