// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef ROOM_MANAGER_EDITOR_PLUGIN_H
#define ROOM_MANAGER_EDITOR_PLUGIN_H

#include "editor/editor_node.h"
#include "editor/editor_plugin.h"
#include "scene/3d/occluder.h"
#include "scene/3d/portal.h"
#include "scene/3d/room.h"
#include "scene/3d/room_manager.h"
#include "scene/resources/material.h"

class RoomManagerEditorPlugin : public EditorPlugin {
    REBEL_OBJECT(RoomManagerEditorPlugin, EditorPlugin);

    RoomManager* _room_manager;

    ToolButton* button_flip_portals;
    EditorNode* editor;

    void _flip_portals();

protected:
    static void _bind_methods();

public:
    String get_name() const override {
        return "RoomManager";
    }

    bool has_main_screen() const override {
        return false;
    }

    void edit(Object* p_object) override;
    bool handles(Object* p_object) const override;
    void make_visible(bool p_visible) override;

    RoomManagerEditorPlugin(EditorNode* p_node);
    ~RoomManagerEditorPlugin() override;
};

///////////////////////

class RoomEditorPlugin : public EditorPlugin {
    REBEL_OBJECT(RoomEditorPlugin, EditorPlugin);

    Room* _room;
    ToolButton* button_generate;
    EditorNode* editor;
    UndoRedo* undo_redo;

    void _generate_points();

protected:
    static void _bind_methods();

public:
    String get_name() const override {
        return "Room";
    }

    bool has_main_screen() const override {
        return false;
    }

    void edit(Object* p_object) override;
    bool handles(Object* p_object) const override;
    void make_visible(bool p_visible) override;

    RoomEditorPlugin(EditorNode* p_node);
    ~RoomEditorPlugin() override;
};

///////////////////////

class PortalEditorPlugin : public EditorPlugin {
    REBEL_OBJECT(PortalEditorPlugin, EditorPlugin);

    Portal* _portal;
    ToolButton* button_flip;
    EditorNode* editor;

    void _flip_portal();

protected:
    static void _bind_methods();

public:
    String get_name() const override {
        return "Portal";
    }

    bool has_main_screen() const override {
        return false;
    }

    void edit(Object* p_object) override;
    bool handles(Object* p_object) const override;
    void make_visible(bool p_visible) override;

    PortalEditorPlugin(EditorNode* p_node);
    ~PortalEditorPlugin() override;
};

///////////////////////

class OccluderEditorPlugin : public EditorPlugin {
    REBEL_OBJECT(OccluderEditorPlugin, EditorPlugin);

    Occluder* _occluder;
    ToolButton* button_center;
    EditorNode* editor;
    UndoRedo* undo_redo;

    void _center();

protected:
    static void _bind_methods();

public:
    String get_name() const override {
        return "Occluder";
    }

    bool has_main_screen() const override {
        return false;
    }

    void edit(Object* p_object) override;
    bool handles(Object* p_object) const override;
    void make_visible(bool p_visible) override;

    OccluderEditorPlugin(EditorNode* p_node);
    ~OccluderEditorPlugin() override;
};

#endif
