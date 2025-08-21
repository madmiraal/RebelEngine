// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef EDITOR_DIRECTORY_H
#define EDITOR_DIRECTORY_H

#include "core/object.h"
#include "core/ustring.h"
#include "core/vector.h"

class EditorFile;

class EditorDirectory : public Object {
    GDCLASS(EditorDirectory, Object);

public:
    ~EditorDirectory();

    String get_name() const;
    void set_name(const String& new_name);
    String get_path() const;
    EditorDirectory* get_parent() const;
    void set_parent(EditorDirectory* new_parent);

    int get_subdir_count() const;
    int find_dir_index(const String& directory_name) const;
    EditorDirectory* get_subdir(int index) const;
    void add_subdir(EditorDirectory* new_directory);

    int get_file_count() const;
    int find_file_index(const String& filename) const;
    EditorFile* get_file(int index) const;
    void add_file(EditorFile* new_file);
    void remove_file(EditorFile* file);
    bool delete_file(const String& filename);

    uint64_t get_modified_time() const;
    void set_modified_time(uint64_t new_time);
    bool is_verified() const;
    void set_verified(bool new_verified);

private:
    String name;
    EditorDirectory* parent_directory = nullptr;
    Vector<EditorDirectory*> subdirectories;
    Vector<EditorFile*> files;
    uint64_t modified_time = 0;
    bool verified          = false;

    static void _bind_methods();
};

#endif // EDITOR_DIRECTORY_H
