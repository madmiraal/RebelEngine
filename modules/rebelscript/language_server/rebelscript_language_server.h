// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef REBELSCRIPT_LANGUAGE_SERVER_H
#define REBELSCRIPT_LANGUAGE_SERVER_H

#include "../rebelscript_parser.h"
#include "editor/editor_plugin.h"
#include "rebelscript_language_protocol.h"

class RebelScriptLanguageServer : public EditorPlugin {
    GDCLASS(RebelScriptLanguageServer, EditorPlugin);

    RebelScriptLanguageProtocol protocol;

    Thread thread;
    bool thread_running;
    bool started;
    bool use_thread;
    String host;
    int port;
    static void thread_main(void* p_userdata);

private:
    void _notification(int p_what);
    void _iteration();

public:
    Error parse_script_file(const String& p_path);
    RebelScriptLanguageServer();
    void start();
    void stop();
};

void register_lsp_types();

#endif // REBELSCRIPT_LANGUAGE_SERVER_H
