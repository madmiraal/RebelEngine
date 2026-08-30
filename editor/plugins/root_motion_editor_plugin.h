// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef ROOT_MOTION_EDITOR_PLUGIN_H
#define ROOT_MOTION_EDITOR_PLUGIN_H

#include "editor/editor_inspector.h"
#include "editor/editor_spin_slider.h"
#include "editor/property_selector.h"
#include "scene/animation/animation_tree.h"

class EditorPropertyRootMotion : public EditorProperty {
    REBEL_OBJECT(EditorPropertyRootMotion, EditorProperty);
    Button* assign;
    Button* clear;
    NodePath base_hint;

    ConfirmationDialog* filter_dialog;
    Tree* filters;

    void _confirmed();
    void _node_assign();
    void _node_clear();

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    void update_property() override;
    void setup(const NodePath& p_base_hint);
    EditorPropertyRootMotion();
};

class EditorInspectorRootMotionPlugin : public EditorInspectorPlugin {
    REBEL_OBJECT(EditorInspectorRootMotionPlugin, EditorInspectorPlugin);

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

#endif // ROOT_MOTION_EDITOR_PLUGIN_H
