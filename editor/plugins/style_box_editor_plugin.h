// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef STYLE_BOX_EDITOR_PLUGIN_H
#define STYLE_BOX_EDITOR_PLUGIN_H

#include "editor/editor_inspector.h"
#include "editor/editor_node.h"
#include "scene/gui/option_button.h"
#include "scene/gui/texture_rect.h"
#include "scene/resources/style_box.h"

class StyleBoxPreview : public VBoxContainer {
    REBEL_OBJECT(StyleBoxPreview, VBoxContainer);

    Control* preview;
    Ref<StyleBox> stylebox;

    void _sb_changed();
    void _redraw();

protected:
    static void _bind_methods();

public:
    void edit(const Ref<StyleBox>& p_stylebox);

    StyleBoxPreview();
};

class EditorInspectorPluginStyleBox : public EditorInspectorPlugin {
    REBEL_OBJECT(EditorInspectorPluginStyleBox, EditorInspectorPlugin);

public:
    bool can_handle(Object* p_object) override;
    void parse_begin(Object* p_object) override;
    bool parse_property(
        Object* p_object,
        Variant::Type p_type,
        const String& p_path,
        PropertyHint p_hint,
        const String& p_hint_text,
        int p_usage
    ) override;
    void parse_end() override;
};

class StyleBoxEditorPlugin : public EditorPlugin {
    REBEL_OBJECT(StyleBoxEditorPlugin, EditorPlugin);

public:
    String get_name() const override {
        return "StyleBox";
    }

    StyleBoxEditorPlugin(EditorNode* p_node);
};

#endif // STYLE_BOX_EDITOR_PLUGIN_H
