// SPDX-FileCopyrightText: 2025 Rebel Engine contributors
//
// SPDX-License-Identifier: MIT

#include "import_project.h"

#include "core/os/dir_access.h"

namespace {
bool create_rebel_project(
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
} // namespace

namespace ImportProject {
bool import_godot_project(
    const String& project_file,
    const String& destination_folder
) {
    ERR_FAIL_COND_V(
        !create_rebel_project(project_file, destination_folder),
        false
    );
    return true;
}
} // namespace ImportProject
