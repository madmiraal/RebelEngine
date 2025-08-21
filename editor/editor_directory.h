// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef EDITOR_DIRECTORY_H
#define EDITOR_DIRECTORY_H

#include "core/object.h"
#include "core/string_name.h"
#include "core/ustring.h"
#include "core/vector.h"

class EditorFile;

class EditorDirectory : public Object {
    GDCLASS(EditorDirectory, Object);

    String name;
    uint64_t modified_time;
    bool verified; // used for checking changes

    EditorDirectory* parent;
    Vector<EditorDirectory*> subdirs;

    void sort_files();

    Vector<EditorFile*> files;

    static void _bind_methods();

    friend class EditorFileSystem;

public:
    String get_name() const;
    String get_path() const;

    int get_subdir_count() const;
    EditorDirectory* get_subdir(int p_idx) const;
    int get_file_count() const;
    EditorFile* get_file(int p_idx) const;
    void remove_file(EditorFile* file);

    EditorDirectory* get_parent() const;

    int find_file_index(const String& p_file) const;
    int find_dir_index(const String& p_dir) const;

    void force_update();

    EditorDirectory();
    ~EditorDirectory();
};

#endif // EDITOR_DIRECTORY_H
