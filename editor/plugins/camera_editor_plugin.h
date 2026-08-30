// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef CAMERA_EDITOR_PLUGIN_H
#define CAMERA_EDITOR_PLUGIN_H

#include "editor/editor_node.h"
#include "editor/editor_plugin.h"
#include "scene/3d/camera.h"

class CameraEditor : public Control {
    REBEL_OBJECT(CameraEditor, Control);

    Panel* panel;
    Button* preview;
    Node* node;

    void _pressed();

protected:
    void _node_removed(Node* p_node);
    static void _bind_methods();

public:
    void edit(Node* p_camera);
    CameraEditor();
};

class CameraEditorPlugin : public EditorPlugin {
    REBEL_OBJECT(CameraEditorPlugin, EditorPlugin);

    EditorNode* editor;

public:
    String get_name() const override {
        return "Camera";
    }

    bool has_main_screen() const override {
        return false;
    }

    void edit(Object* p_object) override;
    bool handles(Object* p_object) const override;
    void make_visible(bool p_visible) override;

    CameraEditorPlugin(EditorNode* p_node);
    ~CameraEditorPlugin() override;
};

#endif // CAMERA_EDITOR_PLUGIN_H
