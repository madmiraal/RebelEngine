// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "editor_file_system_directory.h"

#include "editor/editor_file_info.h"

void EditorFileSystemDirectory::sort_files() {
    files.sort_custom<EditorFileInfoSort>();
}

int EditorFileSystemDirectory::find_file_index(const String& p_file) const {
    for (int i = 0; i < files.size(); i++) {
        if (files[i]->file == p_file) {
            return i;
        }
    }
    return -1;
}

int EditorFileSystemDirectory::find_dir_index(const String& p_dir) const {
    for (int i = 0; i < subdirs.size(); i++) {
        if (subdirs[i]->name == p_dir) {
            return i;
        }
    }

    return -1;
}

void EditorFileSystemDirectory::force_update() {
    // We set modified_time to 0 to force `EditorFileSystem::_scan_fs_changes`
    // to search changes in the directory
    modified_time = 0;
}

int EditorFileSystemDirectory::get_subdir_count() const {
    return subdirs.size();
}

EditorFileSystemDirectory* EditorFileSystemDirectory::get_subdir(int p_idx) {
    ERR_FAIL_INDEX_V(p_idx, subdirs.size(), nullptr);
    return subdirs[p_idx];
}

int EditorFileSystemDirectory::get_file_count() const {
    return files.size();
}

String EditorFileSystemDirectory::get_file(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, files.size(), "");

    return files[p_idx]->file;
}

String EditorFileSystemDirectory::get_path() const {
    String p;
    const EditorFileSystemDirectory* d = this;
    while (d->parent) {
        p = d->name.plus_file(p);
        d = d->parent;
    }

    return "res://" + p;
}

String EditorFileSystemDirectory::get_file_path(int p_idx) const {
    String file                        = get_file(p_idx);
    const EditorFileSystemDirectory* d = this;
    while (d->parent) {
        file = d->name.plus_file(file);
        d    = d->parent;
    }

    return "res://" + file;
}

Vector<String> EditorFileSystemDirectory::get_file_deps(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, files.size(), Vector<String>());
    return files[p_idx]->deps;
}

bool EditorFileSystemDirectory::get_file_import_is_valid(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, files.size(), false);
    return files[p_idx]->import_valid;
}

uint64_t EditorFileSystemDirectory::get_file_modified_time(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, files.size(), 0);
    return files[p_idx]->modified_time;
}

String EditorFileSystemDirectory::get_file_script_class_name(int p_idx) const {
    return files[p_idx]->script_class_name;
}

String EditorFileSystemDirectory::get_file_script_class_extends(int p_idx
) const {
    return files[p_idx]->script_class_extends;
}

String EditorFileSystemDirectory::get_file_script_class_icon_path(int p_idx
) const {
    return files[p_idx]->script_class_icon_path;
}

StringName EditorFileSystemDirectory::get_file_type(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, files.size(), "");
    return files[p_idx]->type;
}

String EditorFileSystemDirectory::get_name() {
    return name;
}

EditorFileSystemDirectory* EditorFileSystemDirectory::get_parent() {
    return parent;
}

void EditorFileSystemDirectory::_bind_methods() {
    ClassDB::bind_method(
        D_METHOD("get_subdir_count"),
        &EditorFileSystemDirectory::get_subdir_count
    );
    ClassDB::bind_method(
        D_METHOD("get_subdir", "idx"),
        &EditorFileSystemDirectory::get_subdir
    );
    ClassDB::bind_method(
        D_METHOD("get_file_count"),
        &EditorFileSystemDirectory::get_file_count
    );
    ClassDB::bind_method(
        D_METHOD("get_file", "idx"),
        &EditorFileSystemDirectory::get_file
    );
    ClassDB::bind_method(
        D_METHOD("get_file_path", "idx"),
        &EditorFileSystemDirectory::get_file_path
    );
    ClassDB::bind_method(
        D_METHOD("get_file_type", "idx"),
        &EditorFileSystemDirectory::get_file_type
    );
    ClassDB::bind_method(
        D_METHOD("get_file_script_class_name", "idx"),
        &EditorFileSystemDirectory::get_file_script_class_name
    );
    ClassDB::bind_method(
        D_METHOD("get_file_script_class_extends", "idx"),
        &EditorFileSystemDirectory::get_file_script_class_extends
    );
    ClassDB::bind_method(
        D_METHOD("get_file_import_is_valid", "idx"),
        &EditorFileSystemDirectory::get_file_import_is_valid
    );
    ClassDB::bind_method(
        D_METHOD("get_name"),
        &EditorFileSystemDirectory::get_name
    );
    ClassDB::bind_method(
        D_METHOD("get_path"),
        &EditorFileSystemDirectory::get_path
    );
    ClassDB::bind_method(
        D_METHOD("get_parent"),
        &EditorFileSystemDirectory::get_parent
    );
    ClassDB::bind_method(
        D_METHOD("find_file_index", "name"),
        &EditorFileSystemDirectory::find_file_index
    );
    ClassDB::bind_method(
        D_METHOD("find_dir_index", "name"),
        &EditorFileSystemDirectory::find_dir_index
    );
}

EditorFileSystemDirectory::EditorFileSystemDirectory() {
    modified_time = 0;
    parent        = nullptr;
    verified      = false;
}

EditorFileSystemDirectory::~EditorFileSystemDirectory() {
    for (int i = 0; i < files.size(); i++) {
        memdelete(files[i]);
    }

    for (int i = 0; i < subdirs.size(); i++) {
        memdelete(subdirs[i]);
    }
}
