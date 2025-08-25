// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef EDITOR_FILE_SYSTEM_H
#define EDITOR_FILE_SYSTEM_H

#include "core/hash_map.h"
#include "core/list.h"
#include "core/os/thread.h"
#include "core/set.h"
#include "core/ustring.h"
#include "core/vector.h"
#include "scene/main/node.h"

class DirAccess;
class EditorDirectory;
class EditorFile;
class FileAccess;
struct EditorProgressBG;

class EditorFileSystem : public Node {
    GDCLASS(EditorFileSystem, Node);
    _THREAD_SAFE_CLASS_

public:
    EditorFileSystem();

    EditorDirectory* get_filesystem();
    EditorDirectory* get_filesystem_path(const String& path);
    EditorDirectory* find_file(const String& p_file, int* r_index) const;

    String get_file_type(const String& p_file) const;
    Set<String> get_valid_extensions() const;
    float get_scanning_progress() const;
    void get_changed_sources(List<String>* r_changed);
    bool is_scanning() const;
    bool is_importing() const;
    bool is_group_file(const String& p_path) const;

    void scan();
    void scan_changes();
    void update_file(const String& p_file);
    void update_script_classes();
    void reimport_files(const Vector<String>& p_files);
    void move_group_file(const String& p_path, const String& p_new_path);

    static bool _should_skip_directory(const String& p_path);

    static EditorFileSystem* get_singleton() {
        return singleton;
    }

private:
    struct ItemAction {
        enum Action {
            ACTION_NONE,
            ACTION_DIR_ADD,
            ACTION_DIR_REMOVE,
            ACTION_FILE_ADD,
            ACTION_FILE_REMOVE,
            ACTION_FILE_TEST_REIMPORT,
            ACTION_FILE_RELOAD
        };

        Action action = ACTION_NONE;
        String file;
        EditorDirectory* dir     = nullptr;
        EditorDirectory* new_dir = nullptr;
        EditorFile* new_file     = nullptr;
    };

    struct ImportFile {
        String path;
        int order;

        bool operator<(const ImportFile& p_if) const {
            return order < p_if.order;
        }
    };

    struct FileCache {
        String type;
        uint64_t modification_time;
        uint64_t import_modification_time;
        Vector<String> deps;
        bool import_valid;
        String import_group_file;
        String script_class_name;
        String script_class_extends;
        String script_class_icon_path;
    };

    struct ScanProgress {
        float low;
        float hi;
        EditorProgressBG* progress;
        void update(int p_current, int p_total) const;
        ScanProgress get_sub(int p_current, int p_total) const;
    };

    Thread thread;
    static void _thread_func(void* userdata);
    Thread thread_sources;
    static void _thread_func_sources(void* userdata);

    List<String> sources_changed;
    List<ItemAction> scan_actions;

    Set<String> late_added_files;
    Set<String> late_update_files;
    Set<String> group_file_cache;
    Set<String> valid_extensions;
    Set<String> import_extensions;

    HashMap<String, FileCache> file_cache;

    String filesystem_settings_version_for_import;

    SafeFlag update_script_classes_queued;

    EditorDirectory* root_directory     = nullptr;
    EditorDirectory* new_root_directory = nullptr;

    float scan_total = 0.f;

    bool first_scan = true;
    bool importing  = false;
    bool reimport_on_missing_imported_files;
    bool revalidate_import_files = false;
    bool scan_changes_pending    = false;
    bool scanning                = false;
    bool scanning_changes        = false;
    bool scanning_changes_done   = false;
    bool using_fat32_or_exfat;

    void _scan_filesystem();
    void _scan_fs_changes(
        EditorDirectory* p_dir,
        const ScanProgress& p_progress
    );
    void _scan_new_dir(
        EditorDirectory* p_dir,
        DirAccess* da,
        const ScanProgress& p_progress
    );
    void _scan_script_classes(EditorDirectory* p_dir);

    void _save_late_updated_files();
    void _save_filesystem_cache();
    void _save_filesystem_cache(EditorDirectory* p_dir, FileAccess* p_file);

    void _create_project_data_dir_if_necessary();
    void _delete_internal_files(String p_file);

    bool _update_scan_actions();
    void _update_extensions();
    void _queue_update_script_classes();

    bool _test_for_reimport(const String& p_path, bool p_only_imported_files);
    void _reimport_file(const String& p_file);
    Error _reimport_group(
        const String& p_group_file,
        const Vector<String>& p_files
    );

    bool _find_file(
        const String& p_file,
        EditorDirectory** r_d,
        int& r_file_pos
    ) const;
    void _find_group_files(
        EditorDirectory* efd,
        Map<String, Vector<String>>& group_files,
        Set<String>& groups_to_reimport
    );
    void _move_group_files(
        EditorDirectory* efd,
        const String& p_group_file,
        const String& p_new_location
    );

    Vector<String> _get_dependencies(const String& p_path);
    String _get_global_script_class(
        const String& p_type,
        const String& p_path,
        String* r_extends,
        String* r_icon_path
    ) const;

    void _notification(int p_what);

    static EditorFileSystem* singleton;
    static Error _resource_import(const String& p_path);
    static void _bind_methods();
};

#endif // EDITOR_FILE_SYSTEM_H
