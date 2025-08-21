// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "editor_file_system.h"

#include "core/io/resource_importer.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/variant_parser.h"
#include "editor/editor_directory.h"
#include "editor/editor_file.h"
#include "editor/editor_node.h"
#include "editor/editor_resource_preview.h"
#include "editor/editor_settings.h"

EditorFileSystem* EditorFileSystem::singleton = nullptr;
// The version, to keep compatibility with different versions of Rebel Engine.
#define CACHE_FILE_NAME "filesystem_cache6"

void EditorFileSystem::_scan_filesystem() {
    ERR_FAIL_COND(!scanning || new_root_directory);

    // read .fscache
    String cpath;

    sources_changed.clear();
    file_cache.clear();

    String project = ProjectSettings::get_singleton()->get_resource_path();

    String fscache =
        EditorSettings::get_singleton()->get_project_settings_dir().plus_file(
            CACHE_FILE_NAME
        );
    FileAccess* f = FileAccess::open(fscache, FileAccess::READ);

    bool first = true;
    if (f) {
        // read the disk cache
        while (!f->eof_reached()) {
            String l = f->get_line().strip_edges();
            if (first) {
                if (first_scan) {
                    // only use this on first scan, afterwards it gets ignored
                    // this is so on first reimport we synchronize versions,
                    // then we don't care until editor restart. This is for
                    // usability mainly so your workflow is not killed after
                    // changing a setting by forceful reimporting everything
                    // there is.
                    filesystem_settings_version_for_import = l.strip_edges();
                    if (filesystem_settings_version_for_import
                        != ResourceFormatImporter::get_singleton()
                               ->get_import_settings_hash()) {
                        revalidate_import_files = true;
                    }
                }
                first = false;
                continue;
            }
            if (l == String()) {
                continue;
            }

            if (l.begins_with("::")) {
                Vector<String> split = l.split("::");
                ERR_CONTINUE(split.size() != 3);
                String name = split[1];

                cpath = name;

            } else {
                Vector<String> split = l.split("::");
                ERR_CONTINUE(split.size() != 8);
                String name = split[0];
                String file;

                file = name;
                name = cpath.plus_file(name);

                FileCache fc;
                fc.type                     = split[1];
                fc.modification_time        = split[2].to_int64();
                fc.import_modification_time = split[3].to_int64();
                fc.import_valid             = split[4].to_int64() != 0;
                fc.import_group_file        = split[5].strip_edges();
                fc.script_class_name        = split[6].get_slice("<>", 0);
                fc.script_class_extends     = split[6].get_slice("<>", 1);
                fc.script_class_icon_path   = split[6].get_slice("<>", 2);

                String deps = split[7].strip_edges();
                if (deps.length()) {
                    Vector<String> dp = deps.split("<>");
                    for (int i = 0; i < dp.size(); i++) {
                        String path = dp[i];
                        fc.deps.push_back(path);
                    }
                }

                file_cache[name] = fc;
            }
        }

        f->close();
        memdelete(f);
    }

    String update_cache =
        EditorSettings::get_singleton()->get_project_settings_dir().plus_file(
            "filesystem_update4"
        );

    if (FileAccess::exists(update_cache)) {
        {
            FileAccessRef f2 = FileAccess::open(update_cache, FileAccess::READ);
            String l         = f2->get_line().strip_edges();
            while (l != String()) {
                file_cache.erase(l); // erase cache for this, so it gets updated
                l = f2->get_line().strip_edges();
            }
        }

        DirAccessRef d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
        d->remove(update_cache); // bye bye update cache
    }

    EditorProgressBG scan_progress("efs", "ScanFS", 1000);

    ScanProgress sp;
    sp.low      = 0;
    sp.hi       = 1;
    sp.progress = &scan_progress;

    new_root_directory = memnew(EditorDirectory);

    DirAccess* d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
    d->change_dir("res://");
    _scan_new_dir(new_root_directory, d, sp);

    file_cache.clear(); // clear caches, no longer needed

    memdelete(d);

    if (!first_scan) {
        // on the first scan this is done from the main thread after
        // re-importing
        _save_filesystem_cache();
    }

    scanning = false;
}

void EditorFileSystem::_save_filesystem_cache() {
    group_file_cache.clear();

    String fscache =
        EditorSettings::get_singleton()->get_project_settings_dir().plus_file(
            CACHE_FILE_NAME
        );

    FileAccess* f = FileAccess::open(fscache, FileAccess::WRITE);
    ERR_FAIL_COND_MSG(
        !f,
        "Cannot create file '" + fscache + "'. Check user write permissions."
    );

    f->store_line(filesystem_settings_version_for_import);
    _save_filesystem_cache(root_directory, f);
    f->close();
    memdelete(f);
}

void EditorFileSystem::_thread_func(void* _userdata) {
    EditorFileSystem* sd = (EditorFileSystem*)_userdata;
    sd->_scan_filesystem();
}

bool EditorFileSystem::_test_for_reimport(
    const String& p_path,
    bool p_only_imported_files
) {
    if (!reimport_on_missing_imported_files && p_only_imported_files) {
        return false;
    }

    if (!FileAccess::exists(p_path + ".import")) {
        return true;
    }

    if (!ResourceFormatImporter::get_singleton()->are_import_settings_valid(
            p_path
        )) {
        // reimport settings are not valid, reimport
        return true;
    }

    Error err;
    FileAccess* f =
        FileAccess::open(p_path + ".import", FileAccess::READ, &err);

    if (!f) { // no import file, do reimport
        return true;
    }

    VariantParser::StreamFile stream;
    stream.f = f;

    String assign;
    Variant value;
    VariantParser::Tag next_tag;

    int lines = 0;
    String error_text;

    List<String> to_check;

    String importer_name;
    String source_file = "";
    String source_md5  = "";
    Vector<String> dest_files;
    String dest_md5 = "";

    while (true) {
        assign = Variant();
        next_tag.fields.clear();
        next_tag.name = String();

        err = VariantParser::parse_tag_assign_eof(
            &stream,
            lines,
            error_text,
            next_tag,
            assign,
            value,
            nullptr,
            true
        );
        if (err == ERR_FILE_EOF) {
            break;
        } else if (err != OK) {
            ERR_PRINT(
                "ResourceFormatImporter::load - '" + p_path
                + ".import:" + itos(lines) + "' error '" + error_text + "'."
            );
            memdelete(f);
            return false; // parse error, try reimport manually (Avoid reimport
                          // loop on broken file)
        }

        if (assign != String()) {
            if (assign.begins_with("path")) {
                to_check.push_back(value);
            } else if (assign == "files") {
                Array fa = value;
                for (int i = 0; i < fa.size(); i++) {
                    to_check.push_back(fa[i]);
                }
            } else if (assign == "importer") {
                importer_name = value;
            } else if (!p_only_imported_files) {
                if (assign == "source_file") {
                    source_file = value;
                } else if (assign == "dest_files") {
                    dest_files = value;
                }
            }

        } else if (next_tag.name != "remap" && next_tag.name != "deps") {
            break;
        }
    }

    memdelete(f);

    if (importer_name == "keep") {
        return false; // keep mode, do not reimport
    }

    // Read the md5's from a separate file (so the import parameters aren't
    // dependent on the file version
    String base_path =
        ResourceFormatImporter::get_singleton()->get_import_base_path(p_path);
    FileAccess* md5s =
        FileAccess::open(base_path + ".md5", FileAccess::READ, &err);
    if (!md5s) { // No md5's stored for this resource
        return true;
    }

    VariantParser::StreamFile md5_stream;
    md5_stream.f = md5s;

    while (true) {
        assign = Variant();
        next_tag.fields.clear();
        next_tag.name = String();

        err = VariantParser::parse_tag_assign_eof(
            &md5_stream,
            lines,
            error_text,
            next_tag,
            assign,
            value,
            nullptr,
            true
        );

        if (err == ERR_FILE_EOF) {
            break;
        } else if (err != OK) {
            ERR_PRINT(
                "ResourceFormatImporter::load - '" + p_path
                + ".import.md5:" + itos(lines) + "' error '" + error_text + "'."
            );
            memdelete(md5s);
            return false; // parse error
        }
        if (assign != String()) {
            if (!p_only_imported_files) {
                if (assign == "source_md5") {
                    source_md5 = value;
                } else if (assign == "dest_md5") {
                    dest_md5 = value;
                }
            }
        }
    }
    memdelete(md5s);

    // imported files are gone, reimport
    for (List<String>::Element* E = to_check.front(); E; E = E->next()) {
        if (!FileAccess::exists(E->get())) {
            return true;
        }
    }

    // check source md5 matching
    if (!p_only_imported_files) {
        if (source_file != String() && source_file != p_path) {
            return true; // file was moved, reimport
        }

        if (source_md5 == String()) {
            return true; // lacks md5, so just reimport
        }

        String md5 = FileAccess::get_md5(p_path);
        if (md5 != source_md5) {
            return true;
        }

        if (dest_files.size() && dest_md5 != String()) {
            md5 = FileAccess::get_multiple_md5(dest_files);
            if (md5 != dest_md5) {
                return true;
            }
        }
    }

    return false; // nothing changed
}

bool EditorFileSystem::_update_scan_actions() {
    sources_changed.clear();

    bool fs_changed = false;

    Vector<String> reimports;
    Vector<String> reloads;

    for (List<ItemAction>::Element* E = scan_actions.front(); E;
         E                            = E->next()) {
        ItemAction& ia = E->get();

        switch (ia.action) {
            case ItemAction::ACTION_NONE: {
            } break;
            case ItemAction::ACTION_DIR_ADD: {
                ia.dir->add_subdir(ia.new_dir);
                fs_changed = true;
            } break;
            case ItemAction::ACTION_DIR_REMOVE: {
                memdelete(ia.dir);
                fs_changed = true;
            } break;
            case ItemAction::ACTION_FILE_ADD: {
                ia.dir->add_file(ia.new_file);
                fs_changed = true;
            } break;
            case ItemAction::ACTION_FILE_REMOVE: {
                if (ia.dir->delete_file(ia.file)) {
                    _delete_internal_files(ia.file);
                    fs_changed = true;
                }
            } break;
            case ItemAction::ACTION_FILE_TEST_REIMPORT: {
                int idx = ia.dir->find_file_index(ia.file);
                ERR_CONTINUE(idx == -1);
                String full_path = ia.dir->get_file(idx)->get_path();
                if (_test_for_reimport(full_path, false)) {
                    // must reimport
                    reimports.push_back(full_path);
                    reimports.append_array(_get_dependencies(full_path));
                } else {
                    // must not reimport, all was good
                    // update modified times, to avoid reimport
                    ia.dir->get_file(idx)->set_modified_time(
                        FileAccess::get_modified_time(full_path)
                    );
                    ia.dir->get_file(idx)->set_import_modified_time(
                        FileAccess::get_modified_time(full_path + ".import")
                    );
                }
                fs_changed = true;
            } break;
            case ItemAction::ACTION_FILE_RELOAD: {
                int idx = ia.dir->find_file_index(ia.file);
                ERR_CONTINUE(idx == -1);
                String full_path = ia.dir->get_file(idx)->get_path();
                reloads.push_back(full_path);
            } break;
        }
    }

    if (reimports.size()) {
        reimport_files(reimports);
    }

    if (first_scan) {
        // only on first scan this is valid and updated, then settings changed.
        revalidate_import_files = false;
        filesystem_settings_version_for_import =
            ResourceFormatImporter::get_singleton()->get_import_settings_hash();
        _save_filesystem_cache();
    }

    if (reloads.size()) {
        emit_signal("resources_reload", reloads);
    }
    scan_actions.clear();

    return fs_changed;
}

void EditorFileSystem::scan() {
    if (scanning || scanning_changes || thread.is_started()) {
        return;
    }

    _update_extensions();

    set_process(true);
    Thread::Settings s;
    scanning   = true;
    scan_total = 0;
    s.priority = Thread::PRIORITY_LOW;
    thread.start(_thread_func, this, s);
}

void EditorFileSystem::ScanProgress::update(int p_current, int p_total) const {
    float ratio = low + ((hi - low) / p_total) * p_current;
    progress->step(ratio * 1000);
    EditorFileSystem::singleton->scan_total = ratio;
}

EditorFileSystem::ScanProgress EditorFileSystem::ScanProgress::get_sub(
    int p_current,
    int p_total
) const {
    ScanProgress sp  = *this;
    float slice      = (sp.hi - sp.low) / p_total;
    sp.low          += slice * p_current;
    sp.hi            = slice;
    return sp;
}

void EditorFileSystem::_scan_new_dir(
    EditorDirectory* p_dir,
    DirAccess* da,
    const ScanProgress& p_progress
) {
    List<String> dirs;
    List<String> files;

    String cd = da->get_current_dir();

    p_dir->set_modified_time(FileAccess::get_modified_time(cd));

    da->list_dir_begin();
    while (true) {
        String f = da->get_next();
        if (f == "") {
            break;
        }

        if (da->current_is_hidden()) {
            continue;
        }

        if (da->current_is_dir()) {
            if (f.begins_with(".")) { // Ignore special and . / ..
                continue;
            }

            if (_should_skip_directory(cd.plus_file(f))) {
                continue;
            }

            dirs.push_back(f);

        } else {
            files.push_back(f);
        }
    }

    da->list_dir_end();

    dirs.sort_custom<NaturalNoCaseComparator>();
    files.sort_custom<NaturalNoCaseComparator>();

    int total = dirs.size() + files.size();
    int idx   = 0;

    for (List<String>::Element* E = dirs.front(); E; E = E->next(), idx++) {
        if (da->change_dir(E->get()) == OK) {
            String d = da->get_current_dir();

            if (d == cd || !d.begins_with(cd)) {
                da->change_dir(cd); // avoid recursion
            } else {
                EditorDirectory* directory = memnew(EditorDirectory);

                directory->set_parent(p_dir);
                directory->set_name(E->get());

                _scan_new_dir(directory, da, p_progress.get_sub(idx, total));
                p_dir->add_subdir(directory);

                da->change_dir("..");
            }
        } else {
            ERR_PRINT("Cannot go into subdir '" + E->get() + "'.");
        }

        p_progress.update(idx, total);
    }

    for (List<String>::Element* E = files.front(); E; E = E->next(), idx++) {
        String ext = E->get().get_extension().to_lower();
        if (!valid_extensions.has(ext)) {
            continue; // invalid
        }

        EditorFile* fi = memnew(EditorFile);
        fi->set_name(E->get());

        String path = cd.plus_file(fi->get_name());

        FileCache* fc = file_cache.getptr(path);
        uint64_t mt   = FileAccess::get_modified_time(path);

        if (import_extensions.has(ext)) {
            // is imported
            uint64_t import_mt = 0;
            if (FileAccess::exists(path + ".import")) {
                import_mt = FileAccess::get_modified_time(path + ".import");
            }

            if (fc && fc->modification_time == mt
                && fc->import_modification_time == import_mt
                && !_test_for_reimport(path, true)) {
                fi->set_type(fc->type);
                fi->set_dependencies(fc->deps);
                fi->set_modified_time(fc->modification_time);
                fi->set_import_modified_time(fc->import_modification_time);
                fi->set_import_valid(fc->import_valid);
                fi->set_script_class_name(fc->script_class_name);
                fi->set_import_group_file(fc->import_group_file);
                fi->set_script_class_extends(fc->script_class_extends);
                fi->set_script_class_icon_path(fc->script_class_icon_path);

                if (revalidate_import_files
                    && !ResourceFormatImporter::get_singleton()
                            ->are_import_settings_valid(path)) {
                    ItemAction ia;
                    ia.action = ItemAction::ACTION_FILE_TEST_REIMPORT;
                    ia.dir    = p_dir;
                    ia.file   = E->get();
                    scan_actions.push_back(ia);
                }

                if (fc->type == String()) {
                    fi->set_type(ResourceLoader::get_resource_type(path));
                    fi->set_import_group_file(
                        ResourceLoader::get_import_group_file(path)
                    );
                }

            } else {
                fi->set_type(ResourceFormatImporter::get_singleton()
                                 ->get_resource_type(path));
                fi->set_import_group_file(ResourceFormatImporter::get_singleton(
                )
                                              ->get_import_group_file(path));
                String script_class_extends;
                String script_class_icon_path;
                fi->set_script_class_name(_get_global_script_class(
                    fi->get_type(),
                    path,
                    &script_class_extends,
                    &script_class_icon_path
                ));
                fi->set_script_class_extends(script_class_extends);
                fi->set_script_class_icon_path(script_class_icon_path);
                fi->set_modified_time(0);
                fi->set_import_modified_time(0);
                fi->set_import_valid(ResourceLoader::is_import_valid(path));

                ItemAction ia;
                ia.action = ItemAction::ACTION_FILE_TEST_REIMPORT;
                ia.dir    = p_dir;
                ia.file   = E->get();
                scan_actions.push_back(ia);
            }
        } else {
            if (fc && fc->modification_time == mt) {
                // not imported, so just update type if changed
                fi->set_type(fc->type);
                fi->set_modified_time(fc->modification_time);
                fi->set_dependencies(fc->deps);
                fi->set_import_modified_time(0);
                fi->set_import_valid(true);
                fi->set_script_class_name(fc->script_class_name);
                fi->set_script_class_extends(fc->script_class_extends);
                fi->set_script_class_icon_path(fc->script_class_icon_path);
            } else {
                // new or modified time
                fi->set_type(ResourceLoader::get_resource_type(path));
                String script_class_extends;
                String script_class_icon_path;
                fi->set_script_class_name(_get_global_script_class(
                    fi->get_type(),
                    path,
                    &script_class_extends,
                    &script_class_icon_path
                ));
                fi->set_script_class_extends(script_class_extends);
                fi->set_script_class_icon_path(script_class_icon_path);
                fi->set_dependencies(_get_dependencies(path));
                fi->set_modified_time(mt);
                fi->set_import_modified_time(0);
                fi->set_import_valid(true);
            }
        }

        p_dir->add_file(fi);
        p_progress.update(idx, total);
    }
}

void EditorFileSystem::_scan_fs_changes(
    EditorDirectory* p_dir,
    const ScanProgress& p_progress
) {
    uint64_t current_mtime = FileAccess::get_modified_time(p_dir->get_path());

    bool updated_dir = false;
    String cd        = p_dir->get_path();

    if (current_mtime != p_dir->get_modified_time() || using_fat32_or_exfat) {
        updated_dir = true;
        p_dir->set_modified_time(current_mtime);
        // ooooops, dir changed, see what's going on

        // first mark everything as veryfied

        for (int i = 0; i < p_dir->get_file_count(); i++) {
            p_dir->get_file(i)->set_verified(false);
        }

        for (int i = 0; i < p_dir->get_subdir_count(); i++) {
            p_dir->get_subdir(i)->set_verified(false);
        }

        // then scan files and directories and check what's different

        DirAccessRef da = DirAccess::create(DirAccess::ACCESS_RESOURCES);

        Error ret = da->change_dir(cd);
        ERR_FAIL_COND_MSG(ret != OK, "Cannot change to '" + cd + "' folder.");

        da->list_dir_begin();
        while (true) {
            String f = da->get_next();
            if (f == "") {
                break;
            }

            if (da->current_is_hidden()) {
                continue;
            }

            if (da->current_is_dir()) {
                if (f.begins_with(".")) { // Ignore special and . / ..
                    continue;
                }

                int idx = p_dir->find_dir_index(f);
                if (idx == -1) {
                    if (_should_skip_directory(cd.plus_file(f))) {
                        continue;
                    }

                    EditorDirectory* efd = memnew(EditorDirectory);

                    efd->set_parent(p_dir);
                    efd->set_name(f);
                    DirAccess* d =
                        DirAccess::create(DirAccess::ACCESS_RESOURCES);
                    d->change_dir(cd.plus_file(f));
                    _scan_new_dir(efd, d, p_progress.get_sub(1, 1));
                    memdelete(d);

                    ItemAction ia;
                    ia.action  = ItemAction::ACTION_DIR_ADD;
                    ia.dir     = p_dir;
                    ia.file    = f;
                    ia.new_dir = efd;
                    scan_actions.push_back(ia);
                } else {
                    p_dir->get_subdir(idx)->set_verified(true);
                }

            } else {
                String ext = f.get_extension().to_lower();
                if (!valid_extensions.has(ext)) {
                    continue; // invalid
                }

                int idx = p_dir->find_file_index(f);

                if (idx == -1) {
                    // never seen this file, add actition to add it
                    EditorFile* fi = memnew(EditorFile);
                    fi->set_name(f);

                    String path = cd.plus_file(f);
                    fi->set_modified_time(FileAccess::get_modified_time(path));
                    fi->set_import_modified_time(0);
                    fi->set_type(ResourceLoader::get_resource_type(path));
                    String script_class_extends;
                    String script_class_icon_path;
                    fi->set_script_class_name(_get_global_script_class(
                        fi->get_type(),
                        path,
                        &script_class_extends,
                        &script_class_icon_path
                    ));
                    fi->set_script_class_extends(script_class_extends);
                    fi->set_script_class_icon_path(script_class_icon_path);
                    fi->set_import_valid(ResourceLoader::is_import_valid(path));
                    fi->set_import_group_file(
                        ResourceLoader::get_import_group_file(path)
                    );

                    {
                        ItemAction ia;
                        ia.action   = ItemAction::ACTION_FILE_ADD;
                        ia.dir      = p_dir;
                        ia.file     = f;
                        ia.new_file = fi;
                        scan_actions.push_back(ia);
                    }

                    if (import_extensions.has(ext)) {
                        // if it can be imported, and it was added, it needs to
                        // be reimported
                        ItemAction ia;
                        ia.action = ItemAction::ACTION_FILE_TEST_REIMPORT;
                        ia.dir    = p_dir;
                        ia.file   = f;
                        scan_actions.push_back(ia);
                    }

                } else {
                    p_dir->get_file(idx)->set_verified(true);
                }
            }
        }

        da->list_dir_end();
    }

    for (int i = 0; i < p_dir->get_file_count(); i++) {
        if (updated_dir && !p_dir->get_file(i)->is_verified()) {
            // this file was removed, add action to remove it
            ItemAction ia;
            ia.action = ItemAction::ACTION_FILE_REMOVE;
            ia.dir    = p_dir;
            ia.file   = p_dir->get_file(i)->get_name();
            scan_actions.push_back(ia);
            continue;
        }

        String path = cd.plus_file(p_dir->get_file(i)->get_name());

        if (import_extensions.has(
                p_dir->get_file(i)->get_name().get_extension().to_lower()
            )) {
            // check here if file must be imported or not

            uint64_t mt = FileAccess::get_modified_time(path);

            bool reimport = false;

            if (mt != p_dir->get_file(i)->get_modified_time()) {
                reimport = true; // it was modified, must be reimported.
            } else if (!FileAccess::exists(path + ".import")) {
                reimport = true; // no .import file, obviously reimport
            } else {
                uint64_t import_mt =
                    FileAccess::get_modified_time(path + ".import");
                if (import_mt
                    != p_dir->get_file(i)->get_import_modified_time()) {
                    reimport = true;
                } else if (_test_for_reimport(path, true)) {
                    reimport = true;
                }
            }

            if (reimport) {
                ItemAction ia;
                ia.action = ItemAction::ACTION_FILE_TEST_REIMPORT;
                ia.dir    = p_dir;
                ia.file   = p_dir->get_file(i)->get_name();
                scan_actions.push_back(ia);
            }
        } else if (ResourceCache::has(path)) { // test for potential reload

            uint64_t mt = FileAccess::get_modified_time(path);

            if (mt != p_dir->get_file(i)->get_modified_time()) {
                p_dir->get_file(i)->set_modified_time(mt);

                ItemAction ia;
                ia.action = ItemAction::ACTION_FILE_RELOAD;
                ia.dir    = p_dir;
                ia.file   = p_dir->get_file(i)->get_name();
                scan_actions.push_back(ia);
            }
        }
    }

    for (int i = 0; i < p_dir->get_subdir_count(); i++) {
        if ((updated_dir && !p_dir->get_subdir(i)->is_verified())
            || _should_skip_directory(p_dir->get_subdir(i)->get_path())) {
            // this directory was removed or ignored, add action to remove it
            ItemAction ia;
            ia.action = ItemAction::ACTION_DIR_REMOVE;
            ia.dir    = p_dir->get_subdir(i);
            scan_actions.push_back(ia);
            continue;
        }
        _scan_fs_changes(p_dir->get_subdir(i), p_progress);
    }
}

void EditorFileSystem::_delete_internal_files(String p_file) {
    if (FileAccess::exists(p_file + ".import")) {
        List<String> paths;
        ResourceFormatImporter::get_singleton()
            ->get_internal_resource_path_list(p_file, &paths);
        DirAccess* da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
        for (List<String>::Element* E = paths.front(); E; E = E->next()) {
            da->remove(E->get());
        }
        da->remove(p_file + ".import");
        memdelete(da);
    }
}

void EditorFileSystem::_thread_func_sources(void* _userdata) {
    EditorFileSystem* efs = (EditorFileSystem*)_userdata;
    if (efs->root_directory) {
        EditorProgressBG pr("sources", TTR("ScanSources"), 1000);
        ScanProgress sp;
        sp.progress = &pr;
        sp.hi       = 1;
        sp.low      = 0;
        efs->_scan_fs_changes(efs->root_directory, sp);
    }
    efs->scanning_changes_done = true;
}

void EditorFileSystem::get_changed_sources(List<String>* r_changed) {
    *r_changed = sources_changed;
}

void EditorFileSystem::scan_changes() {
    if (first_scan || // Prevent a premature changes scan from inhibiting the
                      // first full scan
        scanning || scanning_changes || thread.is_started()) {
        scan_changes_pending = true;
        set_process(true);
        return;
    }

    _update_extensions();
    sources_changed.clear();
    scanning_changes      = true;
    scanning_changes_done = false;

    ERR_FAIL_COND(thread_sources.is_started());
    set_process(true);
    scan_total = 0;
    Thread::Settings s;
    s.priority = Thread::PRIORITY_LOW;
    thread_sources.start(_thread_func_sources, this, s);
}

void EditorFileSystem::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            call_deferred("scan"
            ); // this should happen after every editor node entered the tree

        } break;
        case NOTIFICATION_EXIT_TREE: {
            Thread& active_thread =
                thread.is_started() ? thread : thread_sources;
            if (active_thread.is_started()) {
                while (scanning) {
                    OS::get_singleton()->delay_usec(1000);
                }
                active_thread.wait_to_finish();
                set_process(false);
            }

            if (root_directory) {
                memdelete(root_directory);
            }
            if (new_root_directory) {
                memdelete(new_root_directory);
            }
            root_directory     = nullptr;
            new_root_directory = nullptr;

        } break;
        case NOTIFICATION_PROCESS: {
            if (scanning_changes) {
                if (scanning_changes_done) {
                    scanning_changes = false;

                    set_process(false);

                    thread_sources.wait_to_finish();
                    if (_update_scan_actions()) {
                        emit_signal("filesystem_changed");
                    }
                    emit_signal("sources_changed", sources_changed.size() > 0);
                    _queue_update_script_classes();
                    first_scan = false;
                }
            } else if (!scanning) {
                set_process(false);

                if (root_directory) {
                    memdelete(root_directory);
                }
                root_directory     = new_root_directory;
                new_root_directory = nullptr;
                thread.wait_to_finish();
                _update_scan_actions();
                emit_signal("filesystem_changed");
                emit_signal("sources_changed", sources_changed.size() > 0);
                _queue_update_script_classes();
                first_scan = false;
            }

            if (!is_processing() && scan_changes_pending) {
                scan_changes_pending = false;
                scan_changes();
            }
        } break;
    }
}

bool EditorFileSystem::is_scanning() const {
    return scanning || scanning_changes;
}

float EditorFileSystem::get_scanning_progress() const {
    return scan_total;
}

EditorDirectory* EditorFileSystem::get_filesystem() {
    return root_directory;
}

void EditorFileSystem::_save_filesystem_cache(
    EditorDirectory* p_dir,
    FileAccess* p_file
) {
    if (!p_dir) {
        return; // none
    }
    p_file->store_line(
        "::" + p_dir->get_path()
        + "::" + String::num(p_dir->get_modified_time())
    );

    for (int i = 0; i < p_dir->get_file_count(); i++) {
        EditorFile* file = p_dir->get_file(i);
        if (file->get_import_group_file() != String()) {
            group_file_cache.insert(file->get_import_group_file());
        }
        String line = file->get_name() + "::" + file->get_type()
                    + "::" + itos(file->get_modified_time())
                    + "::" + itos(file->get_import_modified_time())
                    + "::" + itos(file->is_import_valid())
                    + "::" + file->get_import_group_file()
                    + "::" + file->get_script_class_name() + "<>"
                    + file->get_script_class_extends() + "<>"
                    + file->get_script_class_icon_path() + "::";
        Vector<String> dependencies = file->get_dependencies();
        for (int j = 0; j < dependencies.size(); j++) {
            if (j > 0) {
                line += "<>";
            }
            line += dependencies[j];
        }
        p_file->store_line(line);
    }

    for (int i = 0; i < p_dir->get_subdir_count(); i++) {
        _save_filesystem_cache(p_dir->get_subdir(i), p_file);
    }
}

bool EditorFileSystem::_find_file(
    const String& p_file,
    EditorDirectory** r_d,
    int& r_file_pos
) const {
    // todo make faster

    if (!root_directory || scanning) {
        return false;
    }

    String f = ProjectSettings::get_singleton()->localize_path(p_file);

    if (!f.begins_with("res://")) {
        return false;
    }
    f = f.substr(6, f.length());
    f = f.replace("\\", "/");

    Vector<String> path = f.split("/");

    if (path.size() == 0) {
        return false;
    }
    String file = path[path.size() - 1];
    path.resize(path.size() - 1);

    EditorDirectory* directory = root_directory;

    for (int i = 0; i < path.size(); i++) {
        if (path[i].begins_with(".")) {
            return false;
        }

        int idx = -1;
        for (int j = 0; j < directory->get_subdir_count(); j++) {
            if (directory->get_subdir(j)->get_name() == path[i]) {
                idx = j;
                break;
            }
        }

        if (idx == -1) {
            EditorDirectory* new_subdirectory = memnew(EditorDirectory);
            new_subdirectory->set_name(path[i]);
            new_subdirectory->set_parent(directory);
            directory->add_subdir(new_subdirectory);
            directory = new_subdirectory;
        } else {
            directory = directory->get_subdir(idx);
        }
    }

    int cpos = -1;
    for (int i = 0; i < directory->get_file_count(); i++) {
        if (directory->get_file(i)->get_name() == file) {
            cpos = i;
            break;
        }
    }

    r_file_pos = cpos;
    *r_d       = directory;

    return cpos != -1;
}

String EditorFileSystem::get_file_type(const String& p_file) const {
    EditorDirectory* directory = nullptr;
    int cpos                   = -1;
    if (!_find_file(p_file, &directory, cpos)) {
        return "";
    }
    return directory->get_file(cpos)->get_type();
}

EditorDirectory* EditorFileSystem::find_file(const String& p_file, int* r_index)
    const {
    if (!root_directory || scanning) {
        return nullptr;
    }

    EditorDirectory* directory = nullptr;
    int cpos                   = -1;
    if (!_find_file(p_file, &directory, cpos)) {
        return nullptr;
    }

    if (r_index) {
        *r_index = cpos;
    }

    return directory;
}

EditorDirectory* EditorFileSystem::get_filesystem_path(const String& p_path) {
    if (!root_directory || scanning) {
        return nullptr;
    }

    String f = ProjectSettings::get_singleton()->localize_path(p_path);

    if (!f.begins_with("res://")) {
        return nullptr;
    }

    f = f.substr(6, f.length());
    f = f.replace("\\", "/");
    if (f == String()) {
        return root_directory;
    }

    if (f.ends_with("/")) {
        f = f.substr(0, f.length() - 1);
    }

    Vector<String> path = f.split("/");

    if (path.size() == 0) {
        return nullptr;
    }

    EditorDirectory* directory = root_directory;

    for (int i = 0; i < path.size(); i++) {
        int idx = -1;
        for (int j = 0; j < directory->get_subdir_count(); j++) {
            if (directory->get_subdir(j)->get_name() == path[i]) {
                idx = j;
                break;
            }
        }

        if (idx == -1) {
            return nullptr;
        } else {
            directory = directory->get_subdir(idx);
        }
    }

    return directory;
}

void EditorFileSystem::_save_late_updated_files() {
    // files that already existed, and were modified, need re-scanning for
    // dependencies upon project restart. This is done via saving this special
    // file
    String fscache =
        EditorSettings::get_singleton()->get_project_settings_dir().plus_file(
            "filesystem_update4"
        );
    FileAccessRef f = FileAccess::open(fscache, FileAccess::WRITE);
    ERR_FAIL_COND_MSG(
        !f,
        "Cannot create file '" + fscache + "'. Check user write permissions."
    );
    for (Set<String>::Element* E = late_update_files.front(); E;
         E                       = E->next()) {
        f->store_line(E->get());
    }
}

Vector<String> EditorFileSystem::_get_dependencies(const String& p_path) {
    List<String> deps;
    ResourceLoader::get_dependencies(p_path, &deps);

    Vector<String> ret;
    for (List<String>::Element* E = deps.front(); E; E = E->next()) {
        ret.push_back(E->get());
    }

    return ret;
}

String EditorFileSystem::_get_global_script_class(
    const String& p_type,
    const String& p_path,
    String* r_extends,
    String* r_icon_path
) const {
    for (int i = 0; i < ScriptServer::get_language_count(); i++) {
        if (ScriptServer::get_language(i)->handles_global_class_type(p_type)) {
            String global_name;
            String extends;
            String icon_path;

            global_name = ScriptServer::get_language(i)->get_global_class_name(
                p_path,
                &extends,
                &icon_path
            );
            *r_extends   = extends;
            *r_icon_path = icon_path;
            return global_name;
        }
    }
    *r_extends   = String();
    *r_icon_path = String();
    return String();
}

void EditorFileSystem::_scan_script_classes(EditorDirectory* p_dir) {
    for (int i = 0; i < p_dir->get_file_count(); i++) {
        EditorFile* this_file = p_dir->get_file(i);
        if (this_file->get_script_class_name() == String()) {
            continue;
        }

        String lang;
        for (int j = 0; j < ScriptServer::get_language_count(); j++) {
            if (ScriptServer::get_language(j)->handles_global_class_type(
                    this_file->get_type()
                )) {
                lang = ScriptServer::get_language(j)->get_name();
            }
        }
        ScriptServer::add_global_class(
            this_file->get_script_class_name(),
            this_file->get_script_class_extends(),
            lang,
            this_file->get_path()
        );
        EditorNode::get_editor_data().script_class_set_icon_path(
            this_file->get_script_class_name(),
            this_file->get_script_class_icon_path()
        );
        EditorNode::get_editor_data().script_class_set_name(
            this_file->get_name(),
            this_file->get_script_class_name()
        );
    }
    for (int i = 0; i < p_dir->get_subdir_count(); i++) {
        _scan_script_classes(p_dir->get_subdir(i));
    }
}

void EditorFileSystem::update_script_classes() {
    if (!update_script_classes_queued.is_set()) {
        return;
    }

    update_script_classes_queued.clear();
    ScriptServer::global_classes_clear();
    if (get_filesystem()) {
        _scan_script_classes(get_filesystem());
    }

    ScriptServer::save_global_classes();
    EditorNode::get_editor_data().script_class_save_icon_paths();

    // Rescan custom loaders and savers.
    // Doing the following here because the `filesystem_changed` signal fires
    // multiple times and isn't always followed by script classes update. So I
    // thought it's better to do this when script classes really get updated
    ResourceLoader::remove_custom_loaders();
    ResourceLoader::add_custom_loaders();
    ResourceSaver::remove_custom_savers();
    ResourceSaver::add_custom_savers();
}

void EditorFileSystem::_queue_update_script_classes() {
    if (update_script_classes_queued.is_set()) {
        return;
    }

    update_script_classes_queued.set();
    call_deferred("update_script_classes");
}

void EditorFileSystem::update_file(const String& p_file) {
    EditorDirectory* directory = nullptr;
    int cpos                   = -1;

    if (!_find_file(p_file, &directory, cpos)) {
        if (!directory) {
            return;
        }
    }

    if (!FileAccess::exists(p_file)) {
        // The file was removed.
        _delete_internal_files(p_file);
        if (cpos != -1) {
            directory->delete_file(p_file);
        }
        call_deferred("emit_signal", "filesystem_changed"); // update later
        _queue_update_script_classes();
        return;
    }

    EditorFile* file;
    if (cpos == -1) {
        // It's a new file.
        late_added_files.insert(p_file);
        file = memnew(EditorFile);
        file->set_name(p_file);
        file->set_import_valid(ResourceLoader::is_import_valid(p_file));
        directory->add_file(file);
    } else {
        // The file was updated.
        late_update_files.insert(p_file);
        _save_late_updated_files();
        file = directory->get_file(cpos);
    }

    String type = ResourceLoader::get_resource_type(p_file);
    file->set_type(type);
    String script_class_extends;
    String script_class_icon_path;
    file->set_script_class_name(_get_global_script_class(
        type,
        p_file,
        &script_class_extends,
        &script_class_icon_path
    ));
    file->set_script_class_extends(script_class_extends);
    file->set_script_class_icon_path(script_class_icon_path);
    file->set_import_group_file(ResourceLoader::get_import_group_file(p_file));
    file->set_modified_time(FileAccess::get_modified_time(p_file));
    file->set_dependencies(_get_dependencies(p_file));
    file->set_import_valid(ResourceLoader::is_import_valid(p_file));

    // Update preview
    EditorResourcePreview::get_singleton()->check_for_invalidation(p_file);

    call_deferred("emit_signal", "filesystem_changed"); // update later
    _queue_update_script_classes();
}

Set<String> EditorFileSystem::get_valid_extensions() const {
    return valid_extensions;
}

Error EditorFileSystem::_reimport_group(
    const String& p_group_file,
    const Vector<String>& p_files
) {
    String importer_name;

    Map<String, Map<StringName, Variant>> source_file_options;
    Map<String, String> base_paths;
    for (int i = 0; i < p_files.size(); i++) {
        Ref<ConfigFile> config;
        config.instance();
        Error err = config->load(p_files[i] + ".import");
        ERR_CONTINUE(err != OK);
        ERR_CONTINUE(!config->has_section_key("remap", "importer"));
        String file_importer_name = config->get_value("remap", "importer");
        ERR_CONTINUE(file_importer_name == String());

        if (importer_name != String() && importer_name != file_importer_name) {
            print_line(
                "one importer '" + importer_name + "' the other '"
                + file_importer_name + "'."
            );
            EditorNode::get_singleton()->show_warning(vformat(
                TTR("There are multiple importers for different types pointing "
                    "to file %s, import aborted"),
                p_group_file
            ));
            ERR_FAIL_V(ERR_FILE_CORRUPT);
        }

        source_file_options[p_files[i]] = Map<StringName, Variant>();
        importer_name                   = file_importer_name;

        if (importer_name == "keep") {
            continue; // do nothing
        }

        Ref<ResourceImporter> importer =
            ResourceFormatImporter::get_singleton()->get_importer_by_name(
                importer_name
            );
        ERR_FAIL_COND_V(!importer.is_valid(), ERR_FILE_CORRUPT);
        List<ResourceImporter::ImportOption> options;
        importer->get_import_options(&options);
        // set default values
        for (List<ResourceImporter::ImportOption>::Element* E = options.front();
             E;
             E = E->next()) {
            source_file_options[p_files[i]][E->get().option.name] =
                E->get().default_value;
        }

        if (config->has_section("params")) {
            List<String> sk;
            config->get_section_keys("params", &sk);
            for (List<String>::Element* E = sk.front(); E; E = E->next()) {
                String param  = E->get();
                Variant value = config->get_value("params", param);
                // override with whatever is in file
                source_file_options[p_files[i]][param] = value;
            }
        }

        base_paths[p_files[i]] =
            ResourceFormatImporter::get_singleton()->get_import_base_path(
                p_files[i]
            );
    }

    if (importer_name == "keep") {
        return OK; // (do nothing)
    }

    ERR_FAIL_COND_V(importer_name == String(), ERR_UNCONFIGURED);

    Ref<ResourceImporter> importer =
        ResourceFormatImporter::get_singleton()->get_importer_by_name(
            importer_name
        );

    Error err = importer->import_group_file(
        p_group_file,
        source_file_options,
        base_paths
    );

    // all went well, overwrite config files with proper remaps and md5s
    for (Map<String, Map<StringName, Variant>>::Element* E =
             source_file_options.front();
         E;
         E = E->next()) {
        const String& filename = E->key();
        String base_path =
            ResourceFormatImporter::get_singleton()->get_import_base_path(
                filename
            );
        FileAccessRef f =
            FileAccess::open(filename + ".import", FileAccess::WRITE);
        ERR_FAIL_COND_V_MSG(
            !f,
            ERR_FILE_CANT_OPEN,
            "Cannot open import file '" + filename + ".import'."
        );

        // write manually, as order matters ([remap] has to go first for
        // performance).
        f->store_line("[remap]");
        f->store_line("");
        f->store_line("importer=\"" + importer->get_importer_name() + "\"");
        if (importer->get_resource_type() != "") {
            f->store_line("type=\"" + importer->get_resource_type() + "\"");
        }

        Vector<String> dest_paths;

        if (err == OK) {
            String path = base_path + "." + importer->get_save_extension();
            f->store_line("path=\"" + path + "\"");
            dest_paths.push_back(path);
        }

        f->store_line(
            "group_file=" + Variant(p_group_file).get_construct_string()
        );

        if (err == OK) {
            f->store_line("valid=true");
        } else {
            f->store_line("valid=false");
        }
        f->store_line("[deps]\n");

        f->store_line("");

        f->store_line(
            "source_file=" + Variant(filename).get_construct_string()
        );
        if (dest_paths.size()) {
            Array dp;
            for (int i = 0; i < dest_paths.size(); i++) {
                dp.push_back(dest_paths[i]);
            }
            f->store_line(
                "dest_files=" + Variant(dp).get_construct_string() + "\n"
            );
        }
        f->store_line("[params]");
        f->store_line("");

        // store options in provided order, to avoid file changing. Order is
        // also important because first match is accepted first.

        List<ResourceImporter::ImportOption> options;
        importer->get_import_options(&options);
        // set default values
        for (List<ResourceImporter::ImportOption>::Element* F = options.front();
             F;
             F = F->next()) {
            String base = F->get().option.name;
            Variant v   = F->get().default_value;
            if (source_file_options[filename].has(base)) {
                v = source_file_options[filename][base];
            }
            String value;
            VariantWriter::write_to_string(v, value);
            f->store_line(base + "=" + value);
        }

        f->close();

        // Store the md5's of the various files. These are stored separately so
        // that the .import files can be version controlled.
        FileAccessRef md5s =
            FileAccess::open(base_path + ".md5", FileAccess::WRITE);
        ERR_FAIL_COND_V_MSG(
            !md5s,
            ERR_FILE_CANT_OPEN,
            "Cannot open MD5 file '" + base_path + ".md5'."
        );

        md5s->store_line(
            "source_md5=\"" + FileAccess::get_md5(filename) + "\""
        );
        if (dest_paths.size()) {
            md5s->store_line(
                "dest_md5=\"" + FileAccess::get_multiple_md5(dest_paths)
                + "\"\n"
            );
        }
        md5s->close();

        EditorDirectory* directory = nullptr;
        int cpos                   = -1;
        bool found                 = _find_file(filename, &directory, cpos);
        ERR_FAIL_COND_V_MSG(
            !found,
            ERR_UNCONFIGURED,
            "Can't find file '" + filename + "'."
        );

        // To avoid reimport, update modified times.
        EditorFile* file = directory->get_file(cpos);
        file->set_modified_time(FileAccess::get_modified_time(filename));
        file->set_import_modified_time(
            FileAccess::get_modified_time(filename + ".import")
        );
        file->set_dependencies(_get_dependencies(filename));
        file->set_type(importer->get_resource_type());
        file->set_import_valid(err == OK);

        // if file is currently up, maybe the source it was loaded from changed,
        // so import math must be updated for it to reload properly
        if (ResourceCache::has(filename)) {
            Resource* r = ResourceCache::get(filename);

            if (r->get_import_path() != String()) {
                String dst_path = ResourceFormatImporter::get_singleton()
                                      ->get_internal_resource_path(filename);
                r->set_import_path(dst_path);
                r->set_import_last_modified_time(0);
            }
        }

        EditorResourcePreview::get_singleton()->check_for_invalidation(filename
        );
    }

    return err;
}

void EditorFileSystem::_reimport_file(const String& p_file) {
    EditorDirectory* directory = nullptr;
    int cpos                   = -1;
    bool found                 = _find_file(p_file, &directory, cpos);
    ERR_FAIL_COND_MSG(!found, "Can't find file '" + p_file + "'.");

    // try to obtain existing params

    Map<StringName, Variant> params;
    String importer_name;

    if (FileAccess::exists(p_file + ".import")) {
        // use existing
        Ref<ConfigFile> cf;
        cf.instance();
        Error err = cf->load(p_file + ".import");
        if (err == OK) {
            if (cf->has_section("params")) {
                List<String> sk;
                cf->get_section_keys("params", &sk);
                for (List<String>::Element* E = sk.front(); E; E = E->next()) {
                    params[E->get()] = cf->get_value("params", E->get());
                }
            }
            if (cf->has_section("remap")) {
                importer_name = cf->get_value("remap", "importer");
            }
        }

    } else {
        late_added_files.insert(p_file
        ); // imported files do not call update_file(), but just in case..
        params["nodes/use_legacy_names"] = false;
    }

    if (importer_name == "keep") {
        EditorFile* file = directory->get_file(cpos);
        file->set_modified_time(FileAccess::get_modified_time(p_file));
        file->set_import_modified_time(
            FileAccess::get_modified_time(p_file + ".import")
        );
        file->set_dependencies(Vector<String>());
        file->set_type("");
        file->set_import_valid(false);
        EditorResourcePreview::get_singleton()->check_for_invalidation(p_file);
        return;
    }
    Ref<ResourceImporter> importer;
    bool load_default = false;
    // find the importer
    if (importer_name != "") {
        importer =
            ResourceFormatImporter::get_singleton()->get_importer_by_name(
                importer_name
            );
    }

    if (importer.is_null()) {
        // not found by name, find by extension
        importer =
            ResourceFormatImporter::get_singleton()->get_importer_by_extension(
                p_file.get_extension()
            );
        load_default = true;
        if (importer.is_null()) {
            ERR_PRINT("BUG: File queued for import, but can't be imported!");
            ERR_FAIL();
        }
    }

    // mix with default params, in case a parameter is missing

    List<ResourceImporter::ImportOption> opts;
    importer->get_import_options(&opts);
    for (List<ResourceImporter::ImportOption>::Element* E = opts.front(); E;
         E                                                = E->next()) {
        if (!params.has(E->get().option.name)) { // this one is not present
            params[E->get().option.name] = E->get().default_value;
        }
    }

    if (load_default
        && ProjectSettings::get_singleton()->has_setting(
            "importer_defaults/" + importer->get_importer_name()
        )) {
        // use defaults if exist
        Dictionary d = ProjectSettings::get_singleton()->get(
            "importer_defaults/" + importer->get_importer_name()
        );
        List<Variant> v;
        d.get_key_list(&v);

        for (List<Variant>::Element* E = v.front(); E; E = E->next()) {
            params[E->get()] = d[E->get()];
        }
    }

    // finally, perform import!!
    String base_path =
        ResourceFormatImporter::get_singleton()->get_import_base_path(p_file);

    List<String> import_variants;
    List<String> gen_files;
    Variant metadata;
    Error err = importer->import(
        p_file,
        base_path,
        params,
        &import_variants,
        &gen_files,
        &metadata
    );

    if (err != OK) {
        ERR_PRINT("Error importing '" + p_file + "'.");
    }

    // as import is complete, save the .import file

    FileAccess* f = FileAccess::open(p_file + ".import", FileAccess::WRITE);
    ERR_FAIL_COND_MSG(
        !f,
        "Cannot open file from path '" + p_file + ".import'."
    );

    // write manually, as order matters ([remap] has to go first for
    // performance).
    f->store_line("[remap]");
    f->store_line("");
    f->store_line("importer=\"" + importer->get_importer_name() + "\"");
    if (importer->get_resource_type() != "") {
        f->store_line("type=\"" + importer->get_resource_type() + "\"");
    }

    Vector<String> dest_paths;

    if (err == OK) {
        if (importer->get_save_extension() == "") {
            // no path
        } else if (import_variants.size()) {
            // import with variants
            for (List<String>::Element* E = import_variants.front(); E;
                 E                        = E->next()) {
                String path = base_path.c_escape() + "." + E->get() + "."
                            + importer->get_save_extension();

                f->store_line("path." + E->get() + "=\"" + path + "\"");
                dest_paths.push_back(path);
            }
        } else {
            String path = base_path + "." + importer->get_save_extension();
            f->store_line("path=\"" + path + "\"");
            dest_paths.push_back(path);
        }

    } else {
        f->store_line("valid=false");
    }

    if (metadata != Variant()) {
        f->store_line("metadata=" + metadata.get_construct_string());
    }

    f->store_line("");

    f->store_line("[deps]\n");

    if (gen_files.size()) {
        Array genf;
        for (List<String>::Element* E = gen_files.front(); E; E = E->next()) {
            genf.push_back(E->get());
            dest_paths.push_back(E->get());
        }

        String value;
        VariantWriter::write_to_string(genf, value);
        f->store_line("files=" + value);
        f->store_line("");
    }

    f->store_line("source_file=" + Variant(p_file).get_construct_string());

    if (dest_paths.size()) {
        Array dp;
        for (int i = 0; i < dest_paths.size(); i++) {
            dp.push_back(dest_paths[i]);
        }
        f->store_line(
            "dest_files=" + Variant(dp).get_construct_string() + "\n"
        );
    }

    f->store_line("[params]");
    f->store_line("");

    // store options in provided order, to avoid file changing. Order is also
    // important because first match is accepted first.

    for (List<ResourceImporter::ImportOption>::Element* E = opts.front(); E;
         E                                                = E->next()) {
        String base = E->get().option.name;
        String value;
        VariantWriter::write_to_string(params[base], value);
        f->store_line(base + "=" + value);
    }

    f->close();
    memdelete(f);

    // Store the md5's of the various files. These are stored separately so that
    // the .import files can be version controlled.
    FileAccess* md5s = FileAccess::open(base_path + ".md5", FileAccess::WRITE);
    ERR_FAIL_COND_MSG(!md5s, "Cannot open MD5 file '" + base_path + ".md5'.");

    md5s->store_line("source_md5=\"" + FileAccess::get_md5(p_file) + "\"");
    if (dest_paths.size()) {
        md5s->store_line(
            "dest_md5=\"" + FileAccess::get_multiple_md5(dest_paths) + "\"\n"
        );
    }
    md5s->close();
    memdelete(md5s);

    // update modified times, to avoid reimport
    EditorFile* file = directory->get_file(cpos);
    file->set_modified_time(FileAccess::get_modified_time(p_file));
    file->set_import_modified_time(
        FileAccess::get_modified_time(p_file + ".import")
    );
    file->set_dependencies(_get_dependencies(p_file));
    file->set_type(importer->get_resource_type());
    file->set_import_valid(ResourceLoader::is_import_valid(p_file));

    // if file is currently up, maybe the source it was loaded from changed, so
    // import math must be updated for it to reload properly
    if (ResourceCache::has(p_file)) {
        Resource* r = ResourceCache::get(p_file);

        if (r->get_import_path() != String()) {
            String dst_path = ResourceFormatImporter::get_singleton()
                                  ->get_internal_resource_path(p_file);
            r->set_import_path(dst_path);
            r->set_import_last_modified_time(0);
        }
    }

    EditorResourcePreview::get_singleton()->check_for_invalidation(p_file);
}

void EditorFileSystem::_find_group_files(
    EditorDirectory* directory,
    Map<String, Vector<String>>& group_files,
    Set<String>& groups_to_reimport
) {
    for (int i = 0; i < directory->get_file_count(); i++) {
        EditorFile* file = directory->get_file(i);
        if (groups_to_reimport.has(file->get_import_group_file())) {
            if (!group_files.has(file->get_import_group_file())) {
                group_files[file->get_import_group_file()] = Vector<String>();
            }
            group_files[file->get_import_group_file()].push_back(file->get_path(
            ));
        }
    }

    for (int i = 0; i < directory->get_subdir_count(); i++) {
        _find_group_files(
            directory->get_subdir(i),
            group_files,
            groups_to_reimport
        );
    }
}

void EditorFileSystem::_create_project_data_dir_if_necessary() {
    // Check that the project data directory exists
    DirAccess* da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
    String project_data_path =
        ProjectSettings::get_singleton()->get_project_data_path();
    if (da->change_dir(project_data_path) != OK) {
        Error err = da->make_dir(project_data_path);
        if (err) {
            memdelete(da);
            ERR_FAIL_MSG("Failed to create folder " + project_data_path);
        }
    }
    memdelete(da);

    // Check that the project data directory '.gdignore' file exists
    String project_data_gdignore_file_path =
        project_data_path.plus_file(".gdignore");
    if (!FileAccess::exists(project_data_gdignore_file_path)) {
        // Add an empty .gdignore file to avoid scan.
        FileAccessRef f = FileAccess::open(
            project_data_gdignore_file_path,
            FileAccess::WRITE
        );
        if (f) {
            f->store_line("");
            f->close();
        } else {
            ERR_FAIL_MSG(
                "Failed to create file " + project_data_gdignore_file_path
            );
        }
    }
}

void EditorFileSystem::reimport_files(const Vector<String>& p_files) {
    _create_project_data_dir_if_necessary();

    importing = true;
    EditorProgress pr("reimport", TTR("(Re)Importing Assets"), p_files.size());

    Vector<ImportFile> files;
    Set<String> groups_to_reimport;

    for (int i = 0; i < p_files.size(); i++) {
        String group_file =
            ResourceFormatImporter::get_singleton()->get_import_group_file(
                p_files[i]
            );

        if (group_file_cache.has(p_files[i])) {
            // maybe the file itself is a group!
            groups_to_reimport.insert(p_files[i]);
            // groups do not belong to grups
            group_file = String();
        } else if (group_file != String()) {
            // it's a group file, add group to import and skip this file
            groups_to_reimport.insert(group_file);
        } else {
            // it's a regular file
            ImportFile ifile;
            ifile.path = p_files[i];
            ifile.order =
                ResourceFormatImporter::get_singleton()->get_import_order(
                    p_files[i]
                );
            files.push_back(ifile);
        }

        // group may have changed, so also update group reference
        EditorDirectory* directory = nullptr;
        int cpos                   = -1;
        if (_find_file(p_files[i], &directory, cpos)) {
            directory->get_file(cpos)->set_import_group_file(group_file);
        }
    }

    files.sort();

    for (int i = 0; i < files.size(); i++) {
        pr.step(files[i].path.get_file(), i);
        _reimport_file(files[i].path);
    }

    // reimport groups

    if (groups_to_reimport.size()) {
        Map<String, Vector<String>> group_files;
        _find_group_files(root_directory, group_files, groups_to_reimport);
        for (Map<String, Vector<String>>::Element* E = group_files.front(); E;
             E                                       = E->next()) {
            Error err = _reimport_group(E->key(), E->get());
            if (err == OK) {
                _reimport_file(E->key());
            }
        }
    }

    _save_filesystem_cache();
    importing = false;
    if (!is_scanning()) {
        emit_signal("filesystem_changed");
    }

    emit_signal("resources_reimported", p_files);
}

Error EditorFileSystem::_resource_import(const String& p_path) {
    Vector<String> files;
    files.push_back(p_path);

    singleton->update_file(p_path);
    singleton->reimport_files(files);

    return OK;
}

bool EditorFileSystem::_should_skip_directory(const String& p_path) {
    String project_data_path =
        ProjectSettings::get_singleton()->get_project_data_path();
    if (p_path == project_data_path
        || p_path.begins_with(project_data_path + "/")) {
        return true;
    }

    if (FileAccess::exists(p_path.plus_file("project.rebel")
        )) { // skip if another project inside this
        return true;
    }

    if (FileAccess::exists(p_path.plus_file(".gdignore")
        )) { // skip if a `.gdignore` file is inside this
        return true;
    }

    return false;
}

bool EditorFileSystem::is_group_file(const String& p_path) const {
    return group_file_cache.has(p_path);
}

void EditorFileSystem::_move_group_files(
    EditorDirectory* directory,
    const String& p_group_file,
    const String& p_new_location
) {
    for (int i = 0; i < directory->get_file_count(); i++) {
        EditorFile* file = directory->get_file(i);
        if (file->get_import_group_file() == p_group_file) {
            file->set_import_group_file(p_new_location);

            Ref<ConfigFile> config;
            config.instance();
            String path = file->get_path() + ".import";
            Error err   = config->load(path);
            if (err != OK) {
                continue;
            }
            if (config->has_section_key("remap", "group_file")) {
                config->set_value("remap", "group_file", p_new_location);
            }

            List<String> sk;
            config->get_section_keys("params", &sk);
            for (List<String>::Element* E = sk.front(); E; E = E->next()) {
                // not very clean, but should work
                String param = E->get();
                String value = config->get_value("params", param);
                if (value == p_group_file) {
                    config->set_value("params", param, p_new_location);
                }
            }

            config->save(path);
        }
    }

    for (int i = 0; i < directory->get_subdir_count(); i++) {
        _move_group_files(
            directory->get_subdir(i),
            p_group_file,
            p_new_location
        );
    }
}

void EditorFileSystem::move_group_file(
    const String& p_path,
    const String& p_new_path
) {
    if (get_filesystem()) {
        _move_group_files(get_filesystem(), p_path, p_new_path);
        if (group_file_cache.has(p_path)) {
            group_file_cache.erase(p_path);
            group_file_cache.insert(p_new_path);
        }
    }
}

void EditorFileSystem::_bind_methods() {
    ClassDB::bind_method(
        D_METHOD("get_filesystem"),
        &EditorFileSystem::get_filesystem
    );
    ClassDB::bind_method(
        D_METHOD("is_scanning"),
        &EditorFileSystem::is_scanning
    );
    ClassDB::bind_method(
        D_METHOD("get_scanning_progress"),
        &EditorFileSystem::get_scanning_progress
    );
    ClassDB::bind_method(D_METHOD("scan"), &EditorFileSystem::scan);
    ClassDB::bind_method(
        D_METHOD("scan_sources"),
        &EditorFileSystem::scan_changes
    );
    ClassDB::bind_method(
        D_METHOD("update_file", "path"),
        &EditorFileSystem::update_file
    );
    ClassDB::bind_method(
        D_METHOD("get_filesystem_path", "path"),
        &EditorFileSystem::get_filesystem_path
    );
    ClassDB::bind_method(
        D_METHOD("get_file_type", "path"),
        &EditorFileSystem::get_file_type
    );
    ClassDB::bind_method(
        D_METHOD("update_script_classes"),
        &EditorFileSystem::update_script_classes
    );

    ADD_SIGNAL(MethodInfo("filesystem_changed"));
    ADD_SIGNAL(
        MethodInfo("sources_changed", PropertyInfo(Variant::BOOL, "exist"))
    );
    ADD_SIGNAL(MethodInfo(
        "resources_reimported",
        PropertyInfo(Variant::POOL_STRING_ARRAY, "resources")
    ));
    ADD_SIGNAL(MethodInfo(
        "resources_reload",
        PropertyInfo(Variant::POOL_STRING_ARRAY, "resources")
    ));
}

void EditorFileSystem::_update_extensions() {
    valid_extensions.clear();
    import_extensions.clear();

    List<String> extensionsl;
    ResourceLoader::get_recognized_extensions_for_type("", &extensionsl);
    for (List<String>::Element* E = extensionsl.front(); E; E = E->next()) {
        valid_extensions.insert(E->get());
    }

    extensionsl.clear();
    ResourceFormatImporter::get_singleton()->get_recognized_extensions(
        &extensionsl
    );
    for (List<String>::Element* E = extensionsl.front(); E; E = E->next()) {
        import_extensions.insert(E->get());
    }
}

EditorFileSystem::EditorFileSystem() {
    ResourceLoader::import = _resource_import;
    reimport_on_missing_imported_files =
        GLOBAL_DEF("editor/reimport_missing_imported_files", true);

    singleton          = this;
    root_directory     = memnew(EditorDirectory);
    scanning           = false;
    importing          = false;
    new_root_directory = nullptr;

    scanning_changes      = false;
    scanning_changes_done = false;

    _create_project_data_dir_if_necessary();

    // This should probably also work on Unix and use the string it returns for
    // FAT32 or exFAT
    DirAccess* da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
    using_fat32_or_exfat =
        (da->get_filesystem_type() == "FAT32"
         || da->get_filesystem_type() == "exFAT");
    memdelete(da);

    scan_total = 0;
    update_script_classes_queued.clear();
    first_scan              = true;
    scan_changes_pending    = false;
    revalidate_import_files = false;
}

EditorFileSystem::~EditorFileSystem() {}
