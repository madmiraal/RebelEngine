// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "rebelscript_text_document.h"

#include "../rebelscript.h"
#include "core/os/os.h"
#include "editor/editor_settings.h"
#include "editor/plugins/script_text_editor.h"
#include "rebelscript_extend_parser.h"
#include "rebelscript_language_protocol.h"

void RebelScriptTextDocument::_bind_methods() {
    ClassDB::bind_method(
        D_METHOD("didOpen"),
        &RebelScriptTextDocument::didOpen
    );
    ClassDB::bind_method(
        D_METHOD("didClose"),
        &RebelScriptTextDocument::didClose
    );
    ClassDB::bind_method(
        D_METHOD("didChange"),
        &RebelScriptTextDocument::didChange
    );
    ClassDB::bind_method(
        D_METHOD("didSave"),
        &RebelScriptTextDocument::didSave
    );
    ClassDB::bind_method(
        D_METHOD("nativeSymbol"),
        &RebelScriptTextDocument::nativeSymbol
    );
    ClassDB::bind_method(
        D_METHOD("documentSymbol"),
        &RebelScriptTextDocument::documentSymbol
    );
    ClassDB::bind_method(
        D_METHOD("completion"),
        &RebelScriptTextDocument::completion
    );
    ClassDB::bind_method(
        D_METHOD("resolve"),
        &RebelScriptTextDocument::resolve
    );
    ClassDB::bind_method(D_METHOD("rename"), &RebelScriptTextDocument::rename);
    ClassDB::bind_method(
        D_METHOD("foldingRange"),
        &RebelScriptTextDocument::foldingRange
    );
    ClassDB::bind_method(
        D_METHOD("codeLens"),
        &RebelScriptTextDocument::codeLens
    );
    ClassDB::bind_method(
        D_METHOD("documentLink"),
        &RebelScriptTextDocument::documentLink
    );
    ClassDB::bind_method(
        D_METHOD("colorPresentation"),
        &RebelScriptTextDocument::colorPresentation
    );
    ClassDB::bind_method(D_METHOD("hover"), &RebelScriptTextDocument::hover);
    ClassDB::bind_method(
        D_METHOD("definition"),
        &RebelScriptTextDocument::definition
    );
    ClassDB::bind_method(
        D_METHOD("declaration"),
        &RebelScriptTextDocument::declaration
    );
    ClassDB::bind_method(
        D_METHOD("signatureHelp"),
        &RebelScriptTextDocument::signatureHelp
    );
    ClassDB::bind_method(
        D_METHOD("show_native_symbol_in_editor"),
        &RebelScriptTextDocument::show_native_symbol_in_editor
    );
}

void RebelScriptTextDocument::didOpen(const Variant& p_param) {
    lsp::TextDocumentItem doc = load_document_item(p_param);
    sync_script_content(doc.uri, doc.text);
}

void RebelScriptTextDocument::didClose(const Variant& p_param) {
    // Rebel Engine does nothing when closing a document.
}

void RebelScriptTextDocument::didChange(const Variant& p_param) {
    lsp::TextDocumentItem doc = load_document_item(p_param);
    Dictionary dict           = p_param;
    Array contentChanges      = dict["contentChanges"];
    for (int i = 0; i < contentChanges.size(); ++i) {
        lsp::TextDocumentContentChangeEvent evt;
        evt.load(contentChanges[i]);
        doc.text = evt.text;
    }
    sync_script_content(doc.uri, doc.text);
}

void RebelScriptTextDocument::didSave(const Variant& p_param) {
    lsp::TextDocumentItem doc = load_document_item(p_param);
    Dictionary dict           = p_param;
    String text               = dict["text"];

    sync_script_content(doc.uri, text);
}

lsp::TextDocumentItem RebelScriptTextDocument::load_document_item(
    const Variant& p_param
) {
    lsp::TextDocumentItem doc;
    Dictionary params = p_param;
    doc.load(params["textDocument"]);
    return doc;
}

void RebelScriptTextDocument::notify_client_show_symbol(
    const lsp::DocumentSymbol* symbol
) {
    ERR_FAIL_NULL(symbol);
    RebelScriptLanguageProtocol::get_singleton()->notify_client(
        "rebelscript/show_native_symbol",
        symbol->to_json(true)
    );
}

void RebelScriptTextDocument::initialize() {
    if (RebelScriptLanguageProtocol::get_singleton()->is_smart_resolve_enabled(
        )) {
        const HashMap<StringName, ClassMembers>& native_members =
            RebelScriptLanguageProtocol::get_singleton()
                ->get_workspace()
                ->native_members;

        const StringName* class_ptr = native_members.next(nullptr);
        while (class_ptr) {
            const ClassMembers& members = native_members.get(*class_ptr);

            const String* name = members.next(nullptr);
            while (name) {
                const lsp::DocumentSymbol* symbol = members.get(*name);
                lsp::CompletionItem item = symbol->make_completion_item();
                item.data = JOIN_SYMBOLS(String(*class_ptr), *name);
                native_member_completions.push_back(item.to_json());

                name = members.next(name);
            }

            class_ptr = native_members.next(class_ptr);
        }
    }
}

Variant RebelScriptTextDocument::nativeSymbol(const Dictionary& p_params) {
    Variant ret;

    lsp::NativeSymbolInspectParams params;
    params.load(p_params);

    if (const lsp::DocumentSymbol* symbol =
            RebelScriptLanguageProtocol::get_singleton()
                ->get_workspace()
                ->resolve_native_symbol(params)) {
        ret = symbol->to_json(true);
        notify_client_show_symbol(symbol);
    }

    return ret;
}

Array RebelScriptTextDocument::documentSymbol(const Dictionary& p_params) {
    Dictionary params = p_params["textDocument"];
    String uri        = params["uri"];
    String path       = RebelScriptLanguageProtocol::get_singleton()
                      ->get_workspace()
                      ->get_file_path(uri);
    Array arr;
    if (const Map<String, ExtendRebelScriptParser*>::Element* parser =
            RebelScriptLanguageProtocol::get_singleton()
                ->get_workspace()
                ->scripts.find(path)) {
        Vector<lsp::DocumentedSymbolInformation> list;
        parser->get()->get_symbols().symbol_tree_as_list(uri, list);
        for (int i = 0; i < list.size(); i++) {
            arr.push_back(list[i].to_json());
        }
    }
    return arr;
}

Array RebelScriptTextDocument::completion(const Dictionary& p_params) {
    Array arr;

    lsp::CompletionParams params;
    params.load(p_params);
    Dictionary request_data = params.to_json();

    List<ScriptCodeCompletionOption> options;
    RebelScriptLanguageProtocol::get_singleton()->get_workspace()->completion(
        params,
        &options
    );

    if (!options.empty()) {
        int i = 0;
        arr.resize(options.size());

        for (const List<ScriptCodeCompletionOption>::Element* E =
                 options.front();
             E;
             E = E->next()) {
            const ScriptCodeCompletionOption& option = E->get();
            lsp::CompletionItem item;
            item.label = option.display;
            item.data  = request_data;

            switch (option.kind) {
                case ScriptCodeCompletionOption::KIND_ENUM:
                    item.kind = lsp::CompletionItemKind::Enum;
                    break;
                case ScriptCodeCompletionOption::KIND_CLASS:
                    item.kind = lsp::CompletionItemKind::Class;
                    break;
                case ScriptCodeCompletionOption::KIND_MEMBER:
                    item.kind = lsp::CompletionItemKind::Property;
                    break;
                case ScriptCodeCompletionOption::KIND_FUNCTION:
                    item.kind = lsp::CompletionItemKind::Method;
                    break;
                case ScriptCodeCompletionOption::KIND_SIGNAL:
                    item.kind = lsp::CompletionItemKind::Event;
                    break;
                case ScriptCodeCompletionOption::KIND_CONSTANT:
                    item.kind = lsp::CompletionItemKind::Constant;
                    break;
                case ScriptCodeCompletionOption::KIND_VARIABLE:
                    item.kind = lsp::CompletionItemKind::Variable;
                    break;
                case ScriptCodeCompletionOption::KIND_FILE_PATH:
                    item.kind = lsp::CompletionItemKind::File;
                    break;
                case ScriptCodeCompletionOption::KIND_NODE_PATH:
                    item.kind = lsp::CompletionItemKind::Snippet;
                    break;
                case ScriptCodeCompletionOption::KIND_PLAIN_TEXT:
                    item.kind = lsp::CompletionItemKind::Text;
                    break;
            }

            arr[i] = item.to_json();
            i++;
        }
    } else if (RebelScriptLanguageProtocol::get_singleton()
                   ->is_smart_resolve_enabled()) {
        arr = native_member_completions.duplicate();

        for (Map<String, ExtendRebelScriptParser*>::Element* E =
                 RebelScriptLanguageProtocol::get_singleton()
                     ->get_workspace()
                     ->scripts.front();
             E;
             E = E->next()) {
            ExtendRebelScriptParser* script = E->get();
            const Array& items              = script->get_member_completions();

            const int start_size = arr.size();
            arr.resize(start_size + items.size());
            for (int i = start_size; i < arr.size(); i++) {
                arr[i] = items[i - start_size];
            }
        }
    }
    return arr;
}

Dictionary RebelScriptTextDocument::rename(const Dictionary& p_params) {
    lsp::TextDocumentPositionParams params;
    params.load(p_params);
    String new_name = p_params["newName"];

    return RebelScriptLanguageProtocol::get_singleton()
        ->get_workspace()
        ->rename(params, new_name);
}

Dictionary RebelScriptTextDocument::resolve(const Dictionary& p_params) {
    lsp::CompletionItem item;
    item.load(p_params);

    lsp::CompletionParams params;
    Variant data = p_params["data"];

    const lsp::DocumentSymbol* symbol = nullptr;

    if (data.get_type() == Variant::DICTIONARY) {
        params.load(p_params["data"]);
        symbol = RebelScriptLanguageProtocol::get_singleton()
                     ->get_workspace()
                     ->resolve_symbol(
                         params,
                         item.label,
                         item.kind == lsp::CompletionItemKind::Method
                             || item.kind == lsp::CompletionItemKind::Function
                     );

    } else if (data.get_type() == Variant::STRING) {
        String query = data;

        Vector<String> param_symbols = query.split(SYMBOL_SEPERATOR, false);

        if (param_symbols.size() >= 2) {
            String class_         = param_symbols[0];
            StringName class_name = class_;
            String member_name    = param_symbols[param_symbols.size() - 1];
            String inner_class_name;
            if (param_symbols.size() >= 3) {
                inner_class_name = param_symbols[1];
            }

            if (const ClassMembers* members =
                    RebelScriptLanguageProtocol::get_singleton()
                        ->get_workspace()
                        ->native_members.getptr(class_name)) {
                if (const lsp::DocumentSymbol* const* member =
                        members->getptr(member_name)) {
                    symbol = *member;
                }
            }

            if (!symbol) {
                if (const Map<String, ExtendRebelScriptParser*>::Element* E =
                        RebelScriptLanguageProtocol::get_singleton()
                            ->get_workspace()
                            ->scripts.find(class_name)) {
                    symbol = E->get()->get_member_symbol(
                        member_name,
                        inner_class_name
                    );
                }
            }
        }
    }

    if (symbol) {
        item.documentation = symbol->render();
    }

    if ((item.kind == lsp::CompletionItemKind::Method
         || item.kind == lsp::CompletionItemKind::Function)
        && !item.label.ends_with("):")) {
        item.insertText = item.label + "(";
        if (symbol && symbol->children.empty()) {
            item.insertText += ")";
        }
    } else if (item.kind == lsp::CompletionItemKind::Event) {
        if (params.context.triggerKind
                == lsp::CompletionTriggerKind::TriggerCharacter
            && (params.context.triggerCharacter == "(")) {
            const String quote_style =
                EDITOR_DEF("text_editor/completion/use_single_quotes", false)
                    ? "'"
                    : "\"";
            item.insertText = quote_style + item.label + quote_style;
        }
    }

    return item.to_json(true);
}

Array RebelScriptTextDocument::foldingRange(const Dictionary& p_params) {
    Array arr;
    return arr;
}

Array RebelScriptTextDocument::codeLens(const Dictionary& p_params) {
    Array arr;
    return arr;
}

Array RebelScriptTextDocument::documentLink(const Dictionary& p_params) {
    Array ret;

    lsp::DocumentLinkParams params;
    params.load(p_params);

    List<lsp::DocumentLink> links;
    RebelScriptLanguageProtocol::get_singleton()
        ->get_workspace()
        ->resolve_document_links(params.textDocument.uri, links);
    for (const List<lsp::DocumentLink>::Element* E = links.front(); E;
         E                                         = E->next()) {
        ret.push_back(E->get().to_json());
    }
    return ret;
}

Array RebelScriptTextDocument::colorPresentation(const Dictionary& p_params) {
    Array arr;
    return arr;
}

Variant RebelScriptTextDocument::hover(const Dictionary& p_params) {
    lsp::TextDocumentPositionParams params;
    params.load(p_params);

    const lsp::DocumentSymbol* symbol =
        RebelScriptLanguageProtocol::get_singleton()
            ->get_workspace()
            ->resolve_symbol(params);
    if (symbol) {
        lsp::Hover hover;
        hover.contents    = symbol->render();
        hover.range.start = params.position;
        hover.range.end   = params.position;
        return hover.to_json();

    } else if (RebelScriptLanguageProtocol::get_singleton()
                   ->is_smart_resolve_enabled()) {
        Dictionary ret;
        Array contents;
        List<const lsp::DocumentSymbol*> list;
        RebelScriptLanguageProtocol::get_singleton()
            ->get_workspace()
            ->resolve_related_symbols(params, list);
        for (List<const lsp::DocumentSymbol*>::Element* E = list.front(); E;
             E                                            = E->next()) {
            if (const lsp::DocumentSymbol* s = E->get()) {
                contents.push_back(s->render().value);
            }
        }
        ret["contents"] = contents;
        return ret;
    }

    return Variant();
}

Array RebelScriptTextDocument::definition(const Dictionary& p_params) {
    lsp::TextDocumentPositionParams params;
    params.load(p_params);
    List<const lsp::DocumentSymbol*> symbols;
    Array arr = this->find_symbols(params, symbols);
    return arr;
}

Variant RebelScriptTextDocument::declaration(const Dictionary& p_params) {
    lsp::TextDocumentPositionParams params;
    params.load(p_params);
    List<const lsp::DocumentSymbol*> symbols;
    Array arr = this->find_symbols(params, symbols);
    if (arr.empty() && !symbols.empty()
        && !symbols.front()->get()->native_class.empty(
        )) { // Find a native symbol
        const lsp::DocumentSymbol* symbol = symbols.front()->get();
        if (RebelScriptLanguageProtocol::get_singleton()
                ->is_goto_native_symbols_enabled()) {
            String id;
            switch (symbol->kind) {
                case lsp::SymbolKind::Class:
                    id = "class_name:" + symbol->name;
                    break;
                case lsp::SymbolKind::Constant:
                    id = "class_constant:" + symbol->native_class + ":"
                       + symbol->name;
                    break;
                case lsp::SymbolKind::Property:
                case lsp::SymbolKind::Variable:
                    id = "class_property:" + symbol->native_class + ":"
                       + symbol->name;
                    break;
                case lsp::SymbolKind::Enum:
                    id = "class_enum:" + symbol->native_class + ":"
                       + symbol->name;
                    break;
                case lsp::SymbolKind::Method:
                case lsp::SymbolKind::Function:
                    id = "class_method:" + symbol->native_class + ":"
                       + symbol->name;
                    break;
                default:
                    id = "class_global:" + symbol->native_class + ":"
                       + symbol->name;
                    break;
            }
            call_deferred("show_native_symbol_in_editor", id);
        } else {
            notify_client_show_symbol(symbol);
        }
    }
    return arr;
}

Variant RebelScriptTextDocument::signatureHelp(const Dictionary& p_params) {
    Variant ret;

    lsp::TextDocumentPositionParams params;
    params.load(p_params);

    lsp::SignatureHelp s;
    if (OK
        == RebelScriptLanguageProtocol::get_singleton()
               ->get_workspace()
               ->resolve_signature(params, s)) {
        ret = s.to_json();
    }

    return ret;
}

RebelScriptTextDocument::RebelScriptTextDocument() {
    file_checker = FileAccess::create(FileAccess::ACCESS_RESOURCES);
}

RebelScriptTextDocument::~RebelScriptTextDocument() {
    memdelete(file_checker);
}

void RebelScriptTextDocument::sync_script_content(
    const String& p_path,
    const String& p_content
) {
    String path = RebelScriptLanguageProtocol::get_singleton()
                      ->get_workspace()
                      ->get_file_path(p_path);
    RebelScriptLanguageProtocol::get_singleton()->get_workspace()->parse_script(
        path,
        p_content
    );

    EditorFileSystem::get_singleton()->update_file(path);
    Error error;
    Ref<RebelScript> script = ResourceLoader::load(path, "", false, &error);
    if (error == OK) {
        if (script->load_source_code(path) == OK) {
            script->reload(true);
        }
    }
}

void RebelScriptTextDocument::show_native_symbol_in_editor(
    const String& p_symbol_id
) {
    ScriptEditor::get_singleton()->call_deferred(
        "_help_class_goto",
        p_symbol_id
    );
    OS::get_singleton()->move_window_to_foreground();
}

Array RebelScriptTextDocument::find_symbols(
    const lsp::TextDocumentPositionParams& p_location,
    List<const lsp::DocumentSymbol*>& r_list
) {
    Array arr;
    const lsp::DocumentSymbol* symbol =
        RebelScriptLanguageProtocol::get_singleton()
            ->get_workspace()
            ->resolve_symbol(p_location);
    if (symbol) {
        lsp::Location location;
        location.uri       = symbol->uri;
        location.range     = symbol->range;
        const String& path = RebelScriptLanguageProtocol::get_singleton()
                                 ->get_workspace()
                                 ->get_file_path(symbol->uri);
        if (file_checker->file_exists(path)) {
            arr.push_back(location.to_json());
        }
        r_list.push_back(symbol);
    } else if (RebelScriptLanguageProtocol::get_singleton()
                   ->is_smart_resolve_enabled()) {
        List<const lsp::DocumentSymbol*> list;
        RebelScriptLanguageProtocol::get_singleton()
            ->get_workspace()
            ->resolve_related_symbols(p_location, list);
        for (List<const lsp::DocumentSymbol*>::Element* E = list.front(); E;
             E                                            = E->next()) {
            if (const lsp::DocumentSymbol* s = E->get()) {
                if (!s->uri.empty()) {
                    lsp::Location location;
                    location.uri   = s->uri;
                    location.range = s->range;
                    arr.push_back(location.to_json());
                    r_list.push_back(s);
                }
            }
        }
    }
    return arr;
}
