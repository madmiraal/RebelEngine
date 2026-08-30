// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef CPU_PARTICLES_EDITOR_PLUGIN_H
#define CPU_PARTICLES_EDITOR_PLUGIN_H

#include "editor/plugins/particles_editor_plugin.h"
#include "scene/3d/cpu_particles.h"

class CPUParticlesEditor : public ParticlesEditorBase {
    GDCLASS(CPUParticlesEditor, ParticlesEditorBase);

    enum Menu {
        MENU_OPTION_CREATE_EMISSION_VOLUME_FROM_NODE,
        MENU_OPTION_CREATE_EMISSION_VOLUME_FROM_MESH,
        MENU_OPTION_CLEAR_EMISSION_VOLUME,
        MENU_OPTION_RESTART
    };

    CPUParticles* node;

    void _menu_option(int);

    friend class CPUParticlesEditorPlugin;

    void _generate_emission_points() override;

protected:
    void _notification(int p_notification) override;
    void _node_removed(Node* p_node);
    static void _bind_methods();

public:
    void edit(CPUParticles* p_particles);
    CPUParticlesEditor();
};

class CPUParticlesEditorPlugin : public EditorPlugin {
    GDCLASS(CPUParticlesEditorPlugin, EditorPlugin);

    CPUParticlesEditor* particles_editor;
    EditorNode* editor;

public:
    String get_name() const override {
        return "CPUParticles";
    }

    bool has_main_screen() const override {
        return false;
    }

    void edit(Object* p_object) override;
    bool handles(Object* p_object) const override;
    void make_visible(bool p_visible) override;

    CPUParticlesEditorPlugin(EditorNode* p_node);
    ~CPUParticlesEditorPlugin() override;
};

#endif // CPU_PARTICLES_EDITOR_PLUGIN_H
