// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef EDITOR_FILE_SYSTEM_DIRECTORY_H
#define EDITOR_FILE_SYSTEM_DIRECTORY_H

#include "core/object.h"
#include "core/string_name.h"
#include "core/ustring.h"
#include "core/vector.h"

struct EditorFileInfo;

class EditorFileSystemDirectory : public Object {
    GDCLASS(EditorFileSystemDirectory, Object);

    String name;
    uint64_t modified_time;
    bool verified; // used for checking changes

    EditorFileSystemDirectory* parent;
    Vector<EditorFileSystemDirectory*> subdirs;

    void sort_files();

    Vector<EditorFileInfo*> files;

    static void _bind_methods();

    friend class EditorFileSystem;

public:
    String get_name();
    String get_path() const;

    int get_subdir_count() const;
    EditorFileSystemDirectory* get_subdir(int p_idx);
    int get_file_count() const;
    String get_file(int p_idx) const;
    String get_file_path(int p_idx) const;
    StringName get_file_type(int p_idx) const;
    Vector<String> get_file_deps(int p_idx) const;
    bool get_file_import_is_valid(int p_idx) const;
    uint64_t get_file_modified_time(int p_idx) const;
    String get_file_script_class_name(int p_idx) const;      // used for scripts
    String get_file_script_class_extends(int p_idx) const;   // used for scripts
    String get_file_script_class_icon_path(int p_idx) const; // used for scripts

    EditorFileSystemDirectory* get_parent();

    int find_file_index(const String& p_file) const;
    int find_dir_index(const String& p_dir) const;

    void force_update();

    EditorFileSystemDirectory();
    ~EditorFileSystemDirectory();
};

#endif // EDITOR_FILE_SYSTEM_DIRECTORY_H
