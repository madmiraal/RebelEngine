// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "editor_directory.h"

#include "editor/editor_file.h"

void EditorDirectory::sort_files() {
    files.sort_custom<EditorFileSort>();
}

int EditorDirectory::find_file_index(const String& p_file) const {
    for (int i = 0; i < files.size(); i++) {
        if (files[i]->get_name() == p_file) {
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

EditorDirectory* EditorDirectory::get_subdir(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, subdirs.size(), nullptr);
    return subdirs[p_idx];
}

int EditorDirectory::get_file_count() const {
    return files.size();
}

EditorFile* EditorDirectory::get_file(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, files.size(), nullptr);
    return files[p_idx];
}

void EditorDirectory::remove_file(EditorFile* file) {
    files.erase(file);
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

String EditorDirectory::get_name() const {
    return name;
}

EditorDirectory* EditorDirectory::get_parent() const {
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
