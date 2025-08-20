// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef EDITOR_FILE_INFO_H
#define EDITOR_FILE_INFO_H

#include "core/ustring.h"
#include "core/vector.h"

struct EditorFileInfo {
    String file;
    StringName type;
    uint64_t modified_time;
    uint64_t import_modified_time;
    bool import_valid;
    String import_group_file;
    Vector<String> deps;
    bool verified; // used for checking changes
    String script_class_name;
    String script_class_extends;
    String script_class_icon_path;
};

struct EditorFileInfoSort {
    bool operator()(const EditorFileInfo* left, const EditorFileInfo* right)
        const {
        return left->file < right->file;
    }
};

#endif // EDITOR_FILE_INFO_H
