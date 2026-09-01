// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef ANDROID_JNI_DIR_ACCESS
#define ANDROID_JNI_DIR_ACCESS

#include "android_jni.h"
#include "core/os/dir_access.h"

#include <stdio.h>

class AndroidJNIDirAccess : public DirAccess {
    static jobject io;
    static jclass cls;

    static jmethodID _dir_open;
    static jmethodID _dir_next;
    static jmethodID _dir_close;
    static jmethodID _dir_is_dir;

    int id;

    String current_dir;
    String current;

    static DirAccess* create_fs();

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

    bool file_exists(String p_file) override;
    bool dir_exists(String p_dir) override;

    Error make_dir(String p_dir) override;

    Error rename(String p_from, String p_to) override;
    Error remove(String p_name) override;

    bool is_link(String p_file) override {
        return false;
    }

    String read_link(String p_file) override {
        return p_file;
    }

    Error create_link(String p_source, String p_target) override {
        return FAILED;
    }

    String get_filesystem_type() const override;

    uint64_t get_space_left() override;

    static void setup(jobject p_io);

    AndroidJNIDirAccess();
    ~AndroidJNIDirAccess() override;
};

#endif // ANDROID_JNI_DIR_ACCESS
