// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "editor_directory.h"

#include "editor/editor_file.h"

EditorDirectory::~EditorDirectory() {
    for (int i = 0; i < files.size(); i++) {
        memdelete(files[i]);
    }
    for (int i = 0; i < subdirectories.size(); i++) {
        memdelete(subdirectories[i]);
    }
    if (parent_directory) {
        parent_directory->subdirectories.erase(this);
    }
}

String EditorDirectory::get_name() const {
    return name;
}

void EditorDirectory::set_name(const String& new_name) {
    name = new_name;
}

String EditorDirectory::get_path() const {
    const EditorDirectory* directory = this;
    String path;
    while (directory->parent_directory) {
        path      = directory->name.plus_file(path);
        directory = directory->parent_directory;
    }
    return "res://" + path;
}

EditorDirectory* EditorDirectory::get_parent() const {
    return parent_directory;
}

void EditorDirectory::set_parent(EditorDirectory* new_parent) {
    parent_directory = new_parent;
}

int EditorDirectory::get_subdir_count() const {
    return subdirectories.size();
}

int EditorDirectory::find_dir_index(const String& directory_name) const {
    for (int i = 0; i < subdirectories.size(); i++) {
        if (subdirectories[i]->name == directory_name) {
            return i;
        }
    }
    return -1;
}

EditorDirectory* EditorDirectory::get_subdir(int index) const {
    ERR_FAIL_INDEX_V(index, subdirectories.size(), nullptr);
    return subdirectories[index];
}

void EditorDirectory::add_subdir(EditorDirectory* new_directory) {
    for (int index = 0; index < subdirectories.size(); index++) {
        EditorDirectory* this_subdir = subdirectories[index];
        if (new_directory->name < this_subdir->name) {
            subdirectories.insert(index, new_directory);
            return;
        }
    }
    subdirectories.push_back(new_directory);
}

int EditorDirectory::get_file_count() const {
    return files.size();
}

int EditorDirectory::find_file_index(const String& filename) const {
    for (int i = 0; i < files.size(); i++) {
        if (files[i]->get_name() == filename) {
            return i;
        }
    }
    return -1;
}

EditorFile* EditorDirectory::get_file(int index) const {
    ERR_FAIL_INDEX_V(index, files.size(), nullptr);
    return files[index];
}

void EditorDirectory::add_file(EditorFile* new_file) {
    for (int index = 0; index < files.size(); index++) {
        EditorFile* this_file = files[index];
        if (new_file->get_name() < this_file->get_name()) {
            files.insert(index, new_file);
            return;
        }
    }
    files.push_back(new_file);
}

void EditorDirectory::remove_file(EditorFile* file) {
    files.erase(file);
}

bool EditorDirectory::delete_file(const String& filename) {
    int index = find_file_index(filename);
    ERR_FAIL_COND_V_MSG(
        index == -1,
        false,
        vformat("Cannot remove file %s: File not found.", filename)
    );
    memdelete(files[index]);
    return true;
}

uint64_t EditorDirectory::get_modified_time() const {
    return modified_time;
}

void EditorDirectory::set_modified_time(uint64_t new_time) {
    modified_time = new_time;
}

bool EditorDirectory::is_verified() const {
    return verified;
}

void EditorDirectory::set_verified(bool new_verified) {
    verified = new_verified;
}

void EditorDirectory::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_name"), &EditorDirectory::get_name);
    ClassDB::bind_method(D_METHOD("get_path"), &EditorDirectory::get_path);
    ClassDB::bind_method(D_METHOD("get_parent"), &EditorDirectory::get_parent);
    ClassDB::bind_method(
        D_METHOD("get_subdir_count"),
        &EditorDirectory::get_subdir_count
    );
    ClassDB::bind_method(
        D_METHOD("find_dir_index", "name"),
        &EditorDirectory::find_dir_index
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
        D_METHOD("find_file_index", "name"),
        &EditorDirectory::find_file_index
    );
    ClassDB::bind_method(
        D_METHOD("get_file", "idx"),
        &EditorDirectory::get_file
    );
}
