// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef UNIX_DIR_ACCESS_H
#define UNIX_DIR_ACCESS_H

#if defined(UNIX_ENABLED) || defined(LIBC_FILEIO_ENABLED)

#include "core/os/dir_access.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

class UnixDirAccess : public DirAccess {
    DIR* dir_stream;

    static DirAccess* create_fs();

    String current_dir;
    bool _cisdir;
    bool _cishidden;

protected:
    String fix_unicode_name(const char* p_name) const {
        return String::utf8(p_name);
    }

    bool is_hidden(const String& p_name);

public:
    Error list_dir_begin() override;
    String get_next() override;
    bool current_is_dir() const override;
    bool current_is_hidden() const override;

    void list_dir_end() override;

    int get_drive_count() override;
    String get_drive(int p_drive) override;
    bool drives_are_shortcuts() override;

    Error change_dir(String p_dir) override;
    String get_current_dir() override;
    Error make_dir(String p_dir) override;

    bool file_exists(String p_file) override;
    bool dir_exists(String p_dir) override;

    uint64_t get_modified_time(String p_file);

    Error rename(String p_path, String p_new_path) override;
    Error remove(String p_path) override;

    bool is_link(String p_file) override;
    String read_link(String p_file) override;
    Error create_link(String p_source, String p_target) override;

    uint64_t get_space_left() override;

    String get_filesystem_type() const override;

    UnixDirAccess();
    ~UnixDirAccess() override;
};

#endif // UNIX_ENABLED

#endif // UNIX_DIR_ACCESS_H
