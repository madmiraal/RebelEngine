// SPDX-FileCopyrightText: 2025 Rebel Engine contributors
//
// SPDX-License-Identifier: MIT

#include "editor_file.h"

#include "editor/editor_directory.h"

EditorFile::~EditorFile() {
    if (directory) {
        directory->remove_file(this);
    }
}

EditorDirectory* EditorFile::get_directory() const {
    return directory;
}

String EditorFile::get_name() const {
    return name;
}

StringName EditorFile::get_type() const {
    return type;
}

String EditorFile::get_path() const {
    if (directory) {
        return directory->get_path() + name;
    }
    return "res://" + name;
}

String EditorFile::get_import_group_file() const {
    return import_group_file;
}

String EditorFile::get_script_class_name() const {
    return script_class_name;
}

String EditorFile::get_script_class_extends() const {
    return script_class_extends;
}

String EditorFile::get_script_class_icon_path() const {
    return script_class_icon_path;
}

Vector<String> EditorFile::get_dependencies() const {
    return dependencies;
}

uint64_t EditorFile::get_modified_time() const {
    return modified_time;
}

uint64_t EditorFile::get_import_modified_time() const {
    return import_modified_time;
}

bool EditorFile::is_import_valid() const {
    return import_valid;
}

bool EditorFile::is_verified() const {
    return verified;
}

void EditorFile::set_name(const String& new_name) {
    name = new_name;
}

void EditorFile::set_type(const StringName& new_type) {
    type = new_type;
}

void EditorFile::set_import_group_file(const String& new_import_group_file) {
    import_group_file = new_import_group_file;
}

void EditorFile::set_script_class_name(const String& new_script_class_name) {
    script_class_name = new_script_class_name;
}

void EditorFile::set_script_class_extends(const String& new_script_class_extends
) {
    script_class_extends = new_script_class_extends;
}

void EditorFile::set_script_class_icon_path(
    const String& new_script_class_icon_path
) {
    script_class_icon_path = new_script_class_icon_path;
}

void EditorFile::set_dependencies(const Vector<String>& new_dependencies) {
    dependencies = new_dependencies;
}

void EditorFile::set_modified_time(uint64_t new_time) {
    modified_time = new_time;
}

void EditorFile::set_import_modified_time(uint64_t new_time) {
    import_modified_time = new_time;
}

void EditorFile::set_import_valid(bool new_import_valid) {
    import_valid = new_import_valid;
}

void EditorFile::set_verified(bool new_verified) {
    verified = new_verified;
}

void EditorFile::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_path"), &EditorFile::get_path);
    ClassDB::bind_method(D_METHOD("get_type"), &EditorFile::get_type);
    ClassDB::bind_method(
        D_METHOD("get_script_class_name"),
        &EditorFile::get_script_class_name
    );
    ClassDB::bind_method(
        D_METHOD("get_script_class_extends"),
        &EditorFile::get_script_class_extends
    );
    ClassDB::bind_method(
        D_METHOD("is_import_valid"),
        &EditorFile::is_import_valid
    );
}
