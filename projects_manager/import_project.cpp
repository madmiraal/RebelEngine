// SPDX-FileCopyrightText: 2025 Rebel Engine contributors
//
// SPDX-License-Identifier: MIT

#include "import_project.h"

#include "core/io/config_file.h"
#include "core/os/dir_access.h"

namespace {
bool create_rebel_project_from_godot_project(
    const String& project_file,
    const String& destination_folder
) {
    DirAccess* dir_access = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    ERR_FAIL_COND_V_MSG(
        !dir_access->file_exists(project_file),
        false,
        "Source project file does not exist."
    );
    const String source_folder = project_file.get_base_dir();
    if (!dir_access->dir_exists(destination_folder)) {
        Error error = dir_access->make_dir_recursive(destination_folder);
        ERR_FAIL_COND_V_MSG(
            error != OK,
            false,
            "Could not create destination folder: " + destination_folder
        );
    }
    dir_access->change_dir(destination_folder);

    // Copy project contents.
    Error error = dir_access->copy_dir(source_folder, destination_folder);
    ERR_FAIL_COND_V_MSG(
        error != OK,
        false,
        "Could not copy " + source_folder + " to " + destination_folder
    );

    // Rename project settings file.
    error = dir_access->rename("project.godot", "project.rebel");
    ERR_FAIL_COND_V_MSG(
        error != OK,
        false,
        "Could not rename project.godot to project.rebel"
    );
    memdelete(dir_access);
    return true;
}

void rename_rebel_physics(const String& project_file) {
    const String physics_section = "physics";
    ConfigFile config_file(project_file);
    if (!config_file.has_section(physics_section) {
        return;
    }

    const String physics_2d_engine_key = "2d/physics_engine";
    const String physics_3d_engine_key = "3d/physics_engine";
    const String old_value = "GodotPhysics";
    const String new_value = "RebelPhysics";
    List<String> physics_keys;
    config_file.get_section_keys(physics_section, physics_keys);
    if (physics_keys.has(physics_2d_engine_key)
        && config_file.get_value(physics_section, physics_2d_engine_key) == old_value) {
        config_file.set_value(physics_section, physics_2d_engne_key, new_value);
    }
    if (physics_keys.has(physics_2d_engine_key)
        && config_file.get_value(physics_section, physics_2d_engine_key) == old_value) {
        config_file.set_value(physics_section, physics_2d_engne_key, new_value);
    }

    config_file.save();
}
} // namespace

namespace ImportProject {
bool import_godot_project(
    const String& project_file,
    const String& destination_folder
) {
    ERR_FAIL_COND_V(
        !create_rebel_project_from_godot_project(
            project_file,
            destination_folder
        ),
        false
    );
    const String = project_file = destination_folder.plus_file("project.rebel");
    rename_rebel_physics(project_file);
    return true;
}
} // namespace ImportProject
