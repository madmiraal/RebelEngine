// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef SKELETON_IK_EDITOR_PLUGIN_H
#define SKELETON_IK_EDITOR_PLUGIN_H

#include "editor/editor_node.h"
#include "editor/editor_plugin.h"

class SkeletonIK;

class SkeletonIKEditorPlugin : public EditorPlugin {
    REBEL_OBJECT(SkeletonIKEditorPlugin, EditorPlugin);

    SkeletonIK* skeleton_ik;

    Button* play_btn;
    EditorNode* editor;

    void _play();

protected:
    static void _bind_methods();

public:
    String get_name() const override {
        return "SkeletonIK";
    }

    bool has_main_screen() const override {
        return false;
    }

    void edit(Object* p_object) override;
    bool handles(Object* p_object) const override;
    void make_visible(bool p_visible) override;

    SkeletonIKEditorPlugin(EditorNode* p_node);
    ~SkeletonIKEditorPlugin() override;
};

#endif // SKELETON_IK_EDITOR_PLUGIN_H
