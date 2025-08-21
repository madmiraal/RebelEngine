// SPDX-FileCopyrightText: 2025 Rebel Engine contributors
//
// SPDX-License-Identifier: MIT

#ifndef EDITOR_FILE_H
#define EDITOR_FILE_H

#include "core/object.h"
#include "core/ustring.h"
#include "core/vector.h"

class EditorDirectory;

class EditorFile : public Object {
    GDCLASS(EditorFile, Object);

public:
    ~EditorFile();

    EditorDirectory* get_directory() const;
    String get_name() const;
    StringName get_type() const;
    String get_path() const;
    String get_import_group_file() const;
    String get_script_class_name() const;
    String get_script_class_extends() const;
    String get_script_class_icon_path() const;
    Vector<String> get_dependencies() const;
    uint64_t get_modified_time() const;
    uint64_t get_import_modified_time() const;
    bool is_import_valid() const;
    bool is_verified() const;

    void set_name(const String& new_name);
    void set_type(const StringName& new_type);
    void set_import_group_file(const String& new_import_group_file);
    void set_script_class_name(const String& new_script_class_name);
    void set_script_class_extends(const String& new_script_class_extends);
    void set_script_class_icon_path(const String& new_script_class_icon_path);
    void set_dependencies(const Vector<String>& new_dependencies);
    void set_modified_time(uint64_t new_time);
    void set_import_modified_time(uint64_t new_time);
    void set_import_valid(bool new_import_valid);
    void set_verified(bool new_verified);

private:
    EditorDirectory* directory = nullptr;
    String name;
    StringName type;
    String import_group_file;
    String script_class_name;
    String script_class_extends;
    String script_class_icon_path;
    Vector<String> dependencies;
    uint64_t modified_time        = 0;
    uint64_t import_modified_time = 0;
    bool import_valid             = false;
    bool verified                 = false;

    static void _bind_methods();
};

struct EditorFileSort {
    bool operator()(const EditorFile* left, const EditorFile* right) const {
        return left->get_name() < right->get_name();
    }
};

#endif // EDITOR_FILE_H
