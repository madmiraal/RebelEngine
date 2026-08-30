// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef GRADIENT_EDITOR_PLUGIN_H
#define GRADIENT_EDITOR_PLUGIN_H

#include "editor/editor_node.h"
#include "editor/editor_plugin.h"
#include "scene/gui/gradient_edit.h"

class GradientEditor : public GradientEdit {
    REBEL_OBJECT(GradientEditor, GradientEdit);

    bool editing;
    Ref<Gradient> gradient;

    void _gradient_changed();
    void _ramp_changed();

protected:
    static void _bind_methods();

public:
    Size2 get_minimum_size() const override;
    void set_gradient(const Ref<Gradient>& p_gradient);
    GradientEditor();
};

class EditorInspectorPluginGradient : public EditorInspectorPlugin {
    REBEL_OBJECT(EditorInspectorPluginGradient, EditorInspectorPlugin);

public:
    bool can_handle(Object* p_object) override;
    void parse_begin(Object* p_object) override;
};

class GradientEditorPlugin : public EditorPlugin {
    REBEL_OBJECT(GradientEditorPlugin, EditorPlugin);

public:
    String get_name() const override {
        return "ColorRamp";
    }

    GradientEditorPlugin(EditorNode* p_node);
};

#endif // GRADIENT_EDITOR_PLUGIN_H
