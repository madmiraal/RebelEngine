// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "editor_directory.h"

#include "editor/editor_file_info.h"

void EditorDirectory::sort_files() {
    files.sort_custom<EditorFileInfoSort>();
}

int EditorDirectory::find_file_index(const String& p_file) const {
    for (int i = 0; i < files.size(); i++) {
        if (files[i]->file == p_file) {
            return i;
        }
    }
    return -1;
}

int EditorDirectory::find_dir_index(const String& p_dir) const {
    for (int i = 0; i < subdirs.size(); i++) {
        if (subdirs[i]->name == p_dir) {
            return i;
        }
    }

    return -1;
}

void EditorDirectory::force_update() {
    // We set modified_time to 0 to force `EditorFileSystem::_scan_fs_changes`
    // to search changes in the directory
    modified_time = 0;
}

int EditorDirectory::get_subdir_count() const {
    return subdirs.size();
}

EditorDirectory* EditorDirectory::get_subdir(int p_idx) {
    ERR_FAIL_INDEX_V(p_idx, subdirs.size(), nullptr);
    return subdirs[p_idx];
}

int EditorDirectory::get_file_count() const {
    return files.size();
}

String EditorDirectory::get_file(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, files.size(), "");

    return files[p_idx]->file;
}

String EditorDirectory::get_path() const {
    String p;
    const EditorDirectory* d = this;
    while (d->parent) {
        p = d->name.plus_file(p);
        d = d->parent;
    }

    return "res://" + p;
}

String EditorDirectory::get_file_path(int p_idx) const {
    String file              = get_file(p_idx);
    const EditorDirectory* d = this;
    while (d->parent) {
        file = d->name.plus_file(file);
        d    = d->parent;
    }

    return "res://" + file;
}

Vector<String> EditorDirectory::get_file_deps(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, files.size(), Vector<String>());
    return files[p_idx]->deps;
}

bool EditorDirectory::get_file_import_is_valid(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, files.size(), false);
    return files[p_idx]->import_valid;
}

uint64_t EditorDirectory::get_file_modified_time(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, files.size(), 0);
    return files[p_idx]->modified_time;
}

String EditorDirectory::get_file_script_class_name(int p_idx) const {
    return files[p_idx]->script_class_name;
}

String EditorDirectory::get_file_script_class_extends(int p_idx) const {
    return files[p_idx]->script_class_extends;
}

String EditorDirectory::get_file_script_class_icon_path(int p_idx) const {
    return files[p_idx]->script_class_icon_path;
}

StringName EditorDirectory::get_file_type(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, files.size(), "");
    return files[p_idx]->type;
}

String EditorDirectory::get_name() {
    return name;
}

EditorDirectory* EditorDirectory::get_parent() {
    return parent;
}

void EditorDirectory::_bind_methods() {
    ClassDB::bind_method(
        D_METHOD("get_subdir_count"),
        &EditorDirectory::get_subdir_count
    );
    ClassDB::bind_method(
        D_METHOD("get_subdir", "idx"),
        &EditorDirectory::get_subdir
    );
    ClassDB::bind_method(
        D_METHOD("get_file_count"),
        &EditorDirectory::get_file_count
    );
    ClassDB::bind_method(
        D_METHOD("get_file", "idx"),
        &EditorDirectory::get_file
    );
    ClassDB::bind_method(
        D_METHOD("get_file_path", "idx"),
        &EditorDirectory::get_file_path
    );
    ClassDB::bind_method(
        D_METHOD("get_file_type", "idx"),
        &EditorDirectory::get_file_type
    );
    ClassDB::bind_method(
        D_METHOD("get_file_script_class_name", "idx"),
        &EditorDirectory::get_file_script_class_name
    );
    ClassDB::bind_method(
        D_METHOD("get_file_script_class_extends", "idx"),
        &EditorDirectory::get_file_script_class_extends
    );
    ClassDB::bind_method(
        D_METHOD("get_file_import_is_valid", "idx"),
        &EditorDirectory::get_file_import_is_valid
    );
    ClassDB::bind_method(D_METHOD("get_name"), &EditorDirectory::get_name);
    ClassDB::bind_method(D_METHOD("get_path"), &EditorDirectory::get_path);
    ClassDB::bind_method(D_METHOD("get_parent"), &EditorDirectory::get_parent);
    ClassDB::bind_method(
        D_METHOD("find_file_index", "name"),
        &EditorDirectory::find_file_index
    );
    ClassDB::bind_method(
        D_METHOD("find_dir_index", "name"),
        &EditorDirectory::find_dir_index
    );
}

EditorDirectory::EditorDirectory() {
    modified_time = 0;
    parent        = nullptr;
    verified      = false;
}

EditorDirectory::~EditorDirectory() {
    for (int i = 0; i < files.size(); i++) {
        memdelete(files[i]);
    }

    for (int i = 0; i < subdirs.size(); i++) {
        memdelete(subdirs[i]);
    }
}
