// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef WINDOWS_DIR_ACCESS_H
#define WINDOWS_DIR_ACCESS_H

#include "core/os/dir_access.h"

struct WindowsPrivateDirAccess;

class WindowsDirAccess : public DirAccess {
    enum {
        MAX_DRIVES = 26
    };

    WindowsPrivateDirAccess* p;

    char drives[MAX_DRIVES]; // a-z:
    int drive_count;

    String current_dir;

    bool _cisdir;
    bool _cishidden;

public:
    Error list_dir_begin() override;
    String get_next() override;
    bool current_is_dir() const override;
    bool current_is_hidden() const override;
    void list_dir_end() override;

    int get_drive_count() override;
    String get_drive(int p_drive) override;

    Error change_dir(String p_dir) override;
    String get_current_dir() override;
    String get_current_dir_without_drive() override;

    bool file_exists(String p_file) override;
    bool dir_exists(String p_dir) override;

    Error make_dir(String p_dir) override;

    Error rename(String p_path, String p_new_path) override;
    Error remove(String p_path) override;

    bool is_link(String p_file) override {
        return false;
    };

    String read_link(String p_file) override {
        return p_file;
    };

    Error create_link(String p_source, String p_target) override {
        return FAILED;
    };

    uint64_t get_space_left() override;

    String get_filesystem_type() const override;

    WindowsDirAccess();
    ~WindowsDirAccess() override;
};

#endif // WINDOWS_DIR_ACCESS_H
