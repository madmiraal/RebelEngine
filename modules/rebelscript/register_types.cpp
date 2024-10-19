// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "register_types.h"

#include "core/io/file_access_encrypted.h"
#include "core/io/resource_loader.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "rebelscript.h"
#include "rebelscript_tokenizer.h"

RebelScriptLanguage* script_language_gd = nullptr;
Ref<ResourceFormatLoaderRebelScript> resource_loader_gd;
Ref<ResourceFormatSaverRebelScript> resource_saver_gd;

#ifdef TOOLS_ENABLED

#include "editor/editor_export.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/rebelscript_highlighter.h"

#ifndef REBELSCRIPT_NO_LSP
#include "core/engine.h"
#include "language_server/rebelscript_language_server.h"
#endif // !REBELSCRIPT_NO_LSP

class EditorExportRebelScript : public EditorExportPlugin {
    GDCLASS(EditorExportRebelScript, EditorExportPlugin);

public:
    virtual void _export_file(
        const String& p_path,
        const String& p_type,
        const Set<String>& p_features
    ) {
        int script_mode = EditorExportPreset::MODE_SCRIPT_COMPILED;
        String script_key;

        const Ref<EditorExportPreset>& preset = get_export_preset();

        if (preset.is_valid()) {
            script_mode = preset->get_script_export_mode();
            script_key  = preset->get_script_encryption_key().to_lower();
        }

        if (!p_path.ends_with(".gd")
            || script_mode == EditorExportPreset::MODE_SCRIPT_TEXT) {
            return;
        }

        Vector<uint8_t> file = FileAccess::get_file_as_array(p_path);
        if (file.empty()) {
            return;
        }

        String txt;
        txt.parse_utf8((const char*)file.ptr(), file.size());
        file = RebelScriptTokenizerBuffer::parse_code_string(txt);

        if (!file.empty()) {
            if (script_mode == EditorExportPreset::MODE_SCRIPT_ENCRYPTED) {
                String tmp_path =
                    EditorSettings::get_singleton()->get_cache_dir().plus_file(
                        "script.gde"
                    );
                FileAccess* fa = FileAccess::open(tmp_path, FileAccess::WRITE);

                Vector<uint8_t> key;
                key.resize(32);
                for (int i = 0; i < 32; i++) {
                    int v = 0;
                    if (i * 2 < script_key.length()) {
                        CharType ct = script_key[i * 2];
                        if (ct >= '0' && ct <= '9') {
                            ct = ct - '0';
                        } else if (ct >= 'a' && ct <= 'f') {
                            ct = 10 + ct - 'a';
                        }
                        v |= ct << 4;
                    }

                    if (i * 2 + 1 < script_key.length()) {
                        CharType ct = script_key[i * 2 + 1];
                        if (ct >= '0' && ct <= '9') {
                            ct = ct - '0';
                        } else if (ct >= 'a' && ct <= 'f') {
                            ct = 10 + ct - 'a';
                        }
                        v |= ct;
                    }
                    key.write[i] = v;
                }
                FileAccessEncrypted* fae = memnew(FileAccessEncrypted);
                Error err                = fae->open_and_parse(
                    fa,
                    key,
                    FileAccessEncrypted::MODE_WRITE_AES256
                );

                if (err == OK) {
                    fae->store_buffer(file.ptr(), file.size());
                }

                memdelete(fae);

                file = FileAccess::get_file_as_array(tmp_path);
                add_file(p_path.get_basename() + ".gde", file, true);

                // Clean up temporary file.
                DirAccess::remove_file_or_error(tmp_path);

            } else {
                add_file(p_path.get_basename() + ".gdc", file, true);
            }
        }
    }
};

static void _editor_init() {
    Ref<EditorExportRebelScript> gd_export;
    gd_export.instance();
    EditorExport::get_singleton()->add_export_plugin(gd_export);

#ifndef REBELSCRIPT_NO_LSP
    register_lsp_types();
    RebelScriptLanguageServer* lsp_plugin = memnew(RebelScriptLanguageServer);
    EditorNode::get_singleton()->add_editor_plugin(lsp_plugin);
    Engine::get_singleton()->add_singleton(Engine::Singleton(
        "RebelScriptLanguageProtocol",
        RebelScriptLanguageProtocol::get_singleton()
    ));
#endif // !REBELSCRIPT_NO_LSP
}

#endif // TOOLS_ENABLED

void register_rebelscript_types() {
    ClassDB::register_class<RebelScript>();
    ClassDB::register_virtual_class<RebelScriptFunctionState>();

    script_language_gd = memnew(RebelScriptLanguage);
    ScriptServer::register_language(script_language_gd);

    resource_loader_gd.instance();
    ResourceLoader::add_resource_format_loader(resource_loader_gd);

    resource_saver_gd.instance();
    ResourceSaver::add_resource_format_saver(resource_saver_gd);

#ifdef TOOLS_ENABLED
    ScriptEditor::register_create_syntax_highlighter_function(
        RebelScriptSyntaxHighlighter::create
    );
    EditorNode::add_init_callback(_editor_init);
#endif // TOOLS_ENABLED
}

void unregister_rebelscript_types() {
    ScriptServer::unregister_language(script_language_gd);

    if (script_language_gd) {
        memdelete(script_language_gd);
    }

    ResourceLoader::remove_resource_format_loader(resource_loader_gd);
    resource_loader_gd.unref();

    ResourceSaver::remove_resource_format_saver(resource_saver_gd);
    resource_saver_gd.unref();
}
