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

        Action action;
        EditorDirectory* dir;
        String file;
        EditorDirectory* new_dir;
        EditorFile* new_file;

        ItemAction() {
            action   = ACTION_NONE;
            dir      = nullptr;
            new_dir  = nullptr;
            new_file = nullptr;
        }
    };

    Thread thread;
    static void _thread_func(void* _userdata);

    EditorDirectory* new_root_directory;

    bool scanning;
    bool importing;
    bool first_scan;
    bool scan_changes_pending;
    float scan_total;
    String filesystem_settings_version_for_import;
    bool revalidate_import_files;

    void _scan_filesystem();

    Set<String> late_added_files; // keep track of files that were added, these
                                  // will be re-scanned
    Set<String> late_update_files;

    void _save_late_updated_files();

    EditorDirectory* root_directory;

    static EditorFileSystem* singleton;

    /* Used for reading the filesystem cache file */
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

    HashMap<String, FileCache> file_cache;

    struct ScanProgress {
        float low;
        float hi;
        mutable EditorProgressBG* progress;
        void update(int p_current, int p_total) const;
        ScanProgress get_sub(int p_current, int p_total) const;
    };

    void _save_filesystem_cache();
    void _save_filesystem_cache(EditorDirectory* p_dir, FileAccess* p_file);

    bool _find_file(
        const String& p_file,
        EditorDirectory** r_d,
        int& r_file_pos
    ) const;

    void _scan_fs_changes(
        EditorDirectory* p_dir,
        const ScanProgress& p_progress
    );

    void _create_project_data_dir_if_necessary();
    void _delete_internal_files(String p_file);

    Set<String> valid_extensions;
    Set<String> import_extensions;

    void _scan_new_dir(
        EditorDirectory* p_dir,
        DirAccess* da,
        const ScanProgress& p_progress
    );

    Thread thread_sources;
    bool scanning_changes;
    bool scanning_changes_done;

    static void _thread_func_sources(void* _userdata);

    List<String> sources_changed;
    List<ItemAction> scan_actions;

    bool _update_scan_actions();

    void _update_extensions();

    void _reimport_file(const String& p_file);
    Error _reimport_group(
        const String& p_group_file,
        const Vector<String>& p_files
    );

    bool _test_for_reimport(const String& p_path, bool p_only_imported_files);

    bool reimport_on_missing_imported_files;

    Vector<String> _get_dependencies(const String& p_path);

    struct ImportFile {
        String path;
        int order;

        bool operator<(const ImportFile& p_if) const {
            return order < p_if.order;
        }
    };

    void _scan_script_classes(EditorDirectory* p_dir);
    SafeFlag update_script_classes_queued;
    void _queue_update_script_classes();

    String _get_global_script_class(
        const String& p_type,
        const String& p_path,
        String* r_extends,
        String* r_icon_path
    ) const;

    static Error _resource_import(const String& p_path);

    bool using_fat32_or_exfat; // Workaround for projects in FAT32 or exFAT
                               // filesystem (pendrives, most of the time)

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

    Set<String> group_file_cache;

protected:
    void _notification(int p_what);
    static void _bind_methods();

public:
    static EditorFileSystem* get_singleton() {
        return singleton;
    }

    EditorDirectory* get_filesystem();
    bool is_scanning() const;

    bool is_importing() const {
        return importing;
    }

    float get_scanning_progress() const;
    void scan();
    void scan_changes();
    void get_changed_sources(List<String>* r_changed);
    void update_file(const String& p_file);
    Set<String> get_valid_extensions() const;

    EditorDirectory* get_filesystem_path(const String& p_path);
    String get_file_type(const String& p_file) const;
    EditorDirectory* find_file(const String& p_file, int* r_index) const;

    void reimport_files(const Vector<String>& p_files);

    void update_script_classes();

    bool is_group_file(const String& p_path) const;
    void move_group_file(const String& p_path, const String& p_new_path);

    static bool _should_skip_directory(const String& p_path);

    EditorFileSystem();
    ~EditorFileSystem();
};

#endif // EDITOR_FILE_SYSTEM_H
