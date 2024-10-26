// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "project_dialog.h"

#include "core/io/zip_io.h"
#include "core/os/file_access.h"
#include "core/version.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "editor/editor_themes.h"
#include "project_list.h"
#include "scene/gui/check_box.h"
#include "scene/gui/separator.h"

#ifndef SERVER_ENABLED
#include "drivers/gles3/rasterizer_gles3.h"
#endif // SERVER_ENABLED

void ProjectDialog::set_zip_path(const String& p_path) {
    zip_path = p_path;
}

void ProjectDialog::set_zip_title(const String& p_title) {
    zip_title = p_title;
}

void ProjectDialog::set_mode(Mode p_mode) {
    mode = p_mode;
}

void ProjectDialog::set_project_path(const String& p_path) {
    project_path->set_text(p_path);
}

void ProjectDialog::show_dialog() {
    if (mode == MODE_RENAME) {
        project_path->set_editable(false);
        browse->hide();
        install_browse->hide();

        set_title(TTR("Rename Project"));
        get_ok()->set_text(TTR("Rename"));
        name_container->show();
        status_rect->hide();
        msg->hide();
        install_path_container->hide();
        install_status_rect->hide();
        rasterizer_container->hide();
        get_ok()->set_disabled(false);

        ProjectSettings* current = memnew(ProjectSettings);

        int err = current->setup(project_path->get_text(), "");
        if (err != OK) {
            _set_message(
                vformat(
                    TTR("Couldn't load project.rebel in project path "
                        "(error %d). It may be missing or corrupted."),
                    err
                ),
                MESSAGE_ERROR
            );
            status_rect->show();
            msg->show();
            get_ok()->set_disabled(true);
        } else if (current->has_setting("application/config/name")) {
            String proj = current->get("application/config/name");
            project_name->set_text(proj);
            _text_changed(proj);
        }

        project_name->call_deferred("grab_focus");

        create_dir->hide();

    } else {
        fav_dir = EditorSettings::get_singleton()->get(
            "filesystem/directories/default_project_path"
        );
        if (fav_dir != "") {
            project_path->set_text(fav_dir);
            fdialog->set_current_dir(fav_dir);
        } else {
            DirAccess* d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
            project_path->set_text(d->get_current_dir());
            fdialog->set_current_dir(d->get_current_dir());
            memdelete(d);
        }
        String proj = TTR("New Game Project");
        project_name->set_text(proj);
        _text_changed(proj);

        project_path->set_editable(true);
        browse->set_disabled(false);
        browse->show();
        install_browse->set_disabled(false);
        install_browse->show();
        create_dir->show();
        status_rect->show();
        install_status_rect->show();
        msg->show();

        if (mode == MODE_ADD) {
            set_title(TTR("Add Existing Project"));
            get_ok()->set_text(TTR("Add & Edit"));
            name_container->hide();
            install_path_container->hide();
            rasterizer_container->hide();
            project_path->grab_focus();

        } else if (mode == MODE_NEW) {
            set_title(TTR("Create New Project"));
            get_ok()->set_text(TTR("Create & Edit"));
            name_container->show();
            install_path_container->hide();
            rasterizer_container->show();
            project_name->call_deferred("grab_focus");
            project_name->call_deferred("select_all");

        } else if (mode == MODE_INSTALL) {
            set_title(TTR("Install Project:") + " " + zip_title);
            get_ok()->set_text(TTR("Install & Edit"));
            project_name->set_text(zip_title);
            name_container->show();
            install_path_container->hide();
            rasterizer_container->hide();
            project_path->grab_focus();
        }

        _test_path();
    }

    // Reset the dialog to its initial size. Otherwise, the dialog window
    // would be too large when opening a small dialog after closing a large
    // dialog.
    set_size(get_minimum_size());
    popup_centered_minsize(Size2(500, 0) * EDSCALE);
}

ProjectDialog::ProjectDialog() {
    VBoxContainer* vb = memnew(VBoxContainer);
    add_child(vb);

    name_container = memnew(VBoxContainer);
    vb->add_child(name_container);

    Label* l = memnew(Label);
    l->set_text(TTR("Project Name:"));
    name_container->add_child(l);

    HBoxContainer* pnhb = memnew(HBoxContainer);
    name_container->add_child(pnhb);

    project_name = memnew(LineEdit);
    project_name->set_h_size_flags(SIZE_EXPAND_FILL);
    pnhb->add_child(project_name);

    create_dir = memnew(Button);
    pnhb->add_child(create_dir);
    create_dir->set_text(TTR("Create Folder"));
    create_dir->connect("pressed", this, "_create_folder");

    path_container = memnew(VBoxContainer);
    vb->add_child(path_container);

    l = memnew(Label);
    l->set_text(TTR("Project Path:"));
    path_container->add_child(l);

    HBoxContainer* pphb = memnew(HBoxContainer);
    path_container->add_child(pphb);

    project_path = memnew(LineEdit);
    project_path->set_h_size_flags(SIZE_EXPAND_FILL);
    pphb->add_child(project_path);

    install_path_container = memnew(VBoxContainer);
    vb->add_child(install_path_container);

    l = memnew(Label);
    l->set_text(TTR("Project Installation Path:"));
    install_path_container->add_child(l);

    HBoxContainer* iphb = memnew(HBoxContainer);
    install_path_container->add_child(iphb);

    install_path = memnew(LineEdit);
    install_path->set_h_size_flags(SIZE_EXPAND_FILL);
    iphb->add_child(install_path);

    // status icon
    status_rect = memnew(TextureRect);
    status_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
    pphb->add_child(status_rect);

    browse = memnew(Button);
    browse->set_text(TTR("Browse"));
    browse->connect("pressed", this, "_browse_path");
    pphb->add_child(browse);

    // install status icon
    install_status_rect = memnew(TextureRect);
    install_status_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
    iphb->add_child(install_status_rect);

    install_browse = memnew(Button);
    install_browse->set_text(TTR("Browse"));
    install_browse->connect("pressed", this, "_browse_install_path");
    iphb->add_child(install_browse);

    msg = memnew(Label);
    msg->set_align(Label::ALIGN_CENTER);
    vb->add_child(msg);

    // rasterizer selection
    rasterizer_container = memnew(VBoxContainer);
    vb->add_child(rasterizer_container);
    l = memnew(Label);
    l->set_text(TTR("Renderer:"));
    rasterizer_container->add_child(l);
    Container* rshb = memnew(HBoxContainer);
    rasterizer_container->add_child(rshb);
    rasterizer_button_group.instance();

    // Enable GLES3 by default as it's the default value for the project
    // setting.
#ifndef SERVER_ENABLED
    bool gles3_viable = RasterizerGLES3::is_viable() == OK;
#else
    // Whatever, project manager isn't even used in headless builds.
    bool gles3_viable = false;
#endif

    Container* rvb = memnew(VBoxContainer);
    rvb->set_h_size_flags(SIZE_EXPAND_FILL);
    rshb->add_child(rvb);
    Button* rs_button = memnew(CheckBox);
    rs_button->set_button_group(rasterizer_button_group);
    rs_button->set_text(TTR("OpenGL ES 3.0"));
    rs_button->set_meta("driver_name", "GLES3");
    rvb->add_child(rs_button);
    if (gles3_viable) {
        rs_button->set_pressed(true);
    } else {
        // If GLES3 can't be used, don't let users shoot themselves in the
        // foot.
        rs_button->set_disabled(true);
        l = memnew(Label);
        l->set_text(TTR("Not supported by your GPU drivers."));
        rvb->add_child(l);
    }
    l = memnew(Label);
    l->set_text(
        TTR("Higher visual quality\nAll features available\nIncompatible "
            "with older hardware\nNot recommended for web games")
    );
    rvb->add_child(l);

    rshb->add_child(memnew(VSeparator));

    rvb = memnew(VBoxContainer);
    rvb->set_h_size_flags(SIZE_EXPAND_FILL);
    rshb->add_child(rvb);
    rs_button = memnew(CheckBox);
    rs_button->set_button_group(rasterizer_button_group);
    rs_button->set_text(TTR("OpenGL ES 2.0"));
    rs_button->set_meta("driver_name", "GLES2");
    rs_button->set_pressed(!gles3_viable);
    rvb->add_child(rs_button);
    l = memnew(Label);
    l->set_text(
        TTR("Lower visual quality\nSome features not available\nWorks on "
            "most hardware\nRecommended for web games")
    );
    rvb->add_child(l);

    l = memnew(Label);
    l->set_text(TTR(
        "Renderer can be changed later, but scenes may need to be adjusted."
    ));
    l->set_align(Label::ALIGN_CENTER);
    rasterizer_container->add_child(l);

    fdialog = memnew(FileDialog);
    fdialog->set_access(FileDialog::ACCESS_FILESYSTEM);
    fdialog_install = memnew(FileDialog);
    fdialog_install->set_access(FileDialog::ACCESS_FILESYSTEM);
    add_child(fdialog);
    add_child(fdialog_install);
    project_name->connect("text_changed", this, "_text_changed");
    project_path->connect("text_changed", this, "_path_text_changed");
    install_path->connect("text_changed", this, "_path_text_changed");
    fdialog->connect("dir_selected", this, "_path_selected");
    fdialog->connect("file_selected", this, "_file_selected");
    fdialog_install->connect("dir_selected", this, "_install_path_selected");
    fdialog_install->connect("file_selected", this, "_install_path_selected");

    set_hide_on_ok(false);
    mode = MODE_NEW;

    dialog_error = memnew(AcceptDialog);
    add_child(dialog_error);
}

void ProjectDialog::_bind_methods() {
    ClassDB::bind_method("_browse_path", &ProjectDialog::_browse_path);
    ClassDB::bind_method("_create_folder", &ProjectDialog::_create_folder);
    ClassDB::bind_method("_text_changed", &ProjectDialog::_text_changed);
    ClassDB::bind_method(
        "_path_text_changed",
        &ProjectDialog::_path_text_changed
    );
    ClassDB::bind_method("_path_selected", &ProjectDialog::_path_selected);
    ClassDB::bind_method("_file_selected", &ProjectDialog::_file_selected);
    ClassDB::bind_method(
        "_install_path_selected",
        &ProjectDialog::_install_path_selected
    );
    ClassDB::bind_method(
        "_browse_install_path",
        &ProjectDialog::_browse_install_path
    );
    ADD_SIGNAL(MethodInfo("project_created"));
    ADD_SIGNAL(MethodInfo("projects_updated"));
}

void ProjectDialog::_notification(int p_what) {
    if (p_what == MainLoop::NOTIFICATION_WM_QUIT_REQUEST) {
        _remove_created_folder();
    }
}

void ProjectDialog::_browse_install_path() {
    fdialog_install->set_current_dir(install_path->get_text());
    fdialog_install->set_mode(FileDialog::MODE_OPEN_DIR);
    fdialog_install->popup_centered_ratio();
}

void ProjectDialog::_browse_path() {
    fdialog->set_current_dir(project_path->get_text());

    if (mode == MODE_ADD) {
        fdialog->set_mode(FileDialog::MODE_OPEN_FILE);
        fdialog->clear_filters();
        fdialog->add_filter(
            vformat("project.rebel ; %s %s", VERSION_NAME, TTR("Project"))
        );
        fdialog->add_filter("*.zip ; " + TTR("ZIP File"));
    } else {
        fdialog->set_mode(FileDialog::MODE_OPEN_DIR);
    }
    fdialog->popup_centered_ratio();
}

void ProjectDialog::_cancel_pressed() {
    _remove_created_folder();

    project_path->clear();
    _path_text_changed("");
    project_name->clear();
    _text_changed("");

    if (status_rect->get_texture() == get_icon("StatusError", "EditorIcons")) {
        msg->show();
    }

    if (install_status_rect->get_texture()
        == get_icon("StatusError", "EditorIcons")) {
        msg->show();
    }
}

void ProjectDialog::_create_folder() {
    const String project_name_no_edges = project_name->get_text().strip_edges();
    if (project_name_no_edges == "" || created_folder_path != ""
        || project_name_no_edges.ends_with(".")) {
        _set_message(TTR("Invalid project name."), MESSAGE_WARNING);
        return;
    }

    DirAccess* d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    if (d->change_dir(project_path->get_text()) == OK) {
        if (!d->dir_exists(project_name_no_edges)) {
            if (d->make_dir(project_name_no_edges) == OK) {
                d->change_dir(project_name_no_edges);
                String dir_str = d->get_current_dir();
                project_path->set_text(dir_str);
                _path_text_changed(dir_str);
                created_folder_path = d->get_current_dir();
                create_dir->set_disabled(true);
            } else {
                dialog_error->set_text(TTR("Couldn't create folder."));
                dialog_error->popup_centered_minsize();
            }
        } else {
            dialog_error->set_text(
                TTR("There is already a folder in this path with the "
                    "specified name.")
            );
            dialog_error->popup_centered_minsize();
        }
    }

    memdelete(d);
}

void ProjectDialog::_file_selected(const String& p_path) {
    String p = p_path;
    if (mode == MODE_ADD) {
        if (p.ends_with("project.rebel")) {
            p = p.get_base_dir();
            install_path_container->hide();
            get_ok()->set_disabled(false);
        } else if (p.ends_with(".zip")) {
            install_path->set_text(p.get_base_dir());
            install_path_container->show();
            get_ok()->set_disabled(false);
        } else {
            _set_message(
                TTR("Please choose a \"project.rebel\" or \".zip\" file."),
                MESSAGE_ERROR
            );
            get_ok()->set_disabled(true);
            return;
        }
    }
    String sp = p.simplify_path();
    project_path->set_text(sp);
    _path_text_changed(sp);
    if (p.ends_with(".zip")) {
        install_path->call_deferred("grab_focus");
    } else {
        get_ok()->call_deferred("grab_focus");
    }
}

void ProjectDialog::_install_path_selected(const String& p_path) {
    String sp = p_path.simplify_path();
    install_path->set_text(sp);
    _path_text_changed(sp);
    get_ok()->call_deferred("grab_focus");
}

void ProjectDialog::_ok_pressed() {
    String dir = project_path->get_text();

    if (mode == MODE_RENAME) {
        String dir2 = _test_path();
        if (dir2 == "") {
            _set_message(
                TTR("Invalid project path (changed anything?)."),
                MESSAGE_ERROR
            );
            return;
        }

        ProjectSettings* current = memnew(ProjectSettings);

        int err = current->setup(dir2, "");
        if (err != OK) {
            _set_message(
                vformat(
                    TTR("Couldn't load project.rebel in project path "
                        "(error %d). It may be missing or corrupted."),
                    err
                ),
                MESSAGE_ERROR
            );
        } else {
            ProjectSettings::CustomMap edited_settings;
            edited_settings["application/config/name"] =
                project_name->get_text().strip_edges();

            if (current->save_custom(
                    dir2.plus_file("project.rebel"),
                    edited_settings,
                    Vector<String>(),
                    true
                )
                != OK) {
                _set_message(
                    TTR("Couldn't edit project.rebel in project path."),
                    MESSAGE_ERROR
                );
            }
        }

        hide();
        emit_signal("projects_updated");

    } else {
        if (mode == MODE_ADD) {
            if (project_path->get_text().ends_with(".zip")) {
                mode = MODE_INSTALL;
                _ok_pressed();

                return;
            }

        } else {
            if (mode == MODE_NEW) {
                ProjectSettings::CustomMap initial_settings;
                if (rasterizer_button_group->get_pressed_button()->get_meta(
                        "driver_name"
                    )
                    == "GLES3") {
                    initial_settings["rendering/quality/driver/driver_name"] =
                        "GLES3";
                } else {
                    initial_settings["rendering/quality/driver/driver_name"] =
                        "GLES2";
                    initial_settings["rendering/vram_compression/import_etc2"] =
                        false;
                    initial_settings["rendering/vram_compression/import_etc"] =
                        true;
                }
                initial_settings["application/config/name"] =
                    project_name->get_text().strip_edges();
                initial_settings["application/config/icon"] = "res://icon.png";
                initial_settings["rendering/environment/default_environment"] =
                    "res://default_env.tres";
                initial_settings["physics/common/enable_pause_aware_picking"] =
                    true;

                if (ProjectSettings::get_singleton()->save_custom(
                        dir.plus_file("project.rebel"),
                        initial_settings,
                        Vector<String>(),
                        false
                    )
                    != OK) {
                    _set_message(
                        TTR("Couldn't create project.rebel in project path."),
                        MESSAGE_ERROR
                    );
                } else {
                    ResourceSaver::save(
                        dir.plus_file("icon.png"),
                        create_unscaled_default_project_icon()
                    );

                    FileAccess* f = FileAccess::open(
                        dir.plus_file("default_env.tres"),
                        FileAccess::WRITE
                    );
                    if (!f) {
                        _set_message(
                            TTR("Couldn't create project.rebel in project "
                                "path."),
                            MESSAGE_ERROR
                        );
                    } else {
                        f->store_line(
                            "[gd_resource type=\"Environment\" "
                            "load_steps=2 format=2]"
                        );
                        f->store_line(
                            "[sub_resource type=\"ProceduralSky\" id=1]"
                        );
                        f->store_line("[resource]");
                        f->store_line("background_mode = 2");
                        f->store_line("background_sky = SubResource( 1 )");
                        memdelete(f);
                    }
                }

            } else if (mode == MODE_INSTALL) {
                if (project_path->get_text().ends_with(".zip")) {
                    dir      = install_path->get_text();
                    zip_path = project_path->get_text();
                }

                FileAccess* src_f    = nullptr;
                zlib_filefunc_def io = zipio_create_io_from_file(&src_f);

                unzFile pkg = unzOpen2(zip_path.utf8().get_data(), &io);
                if (!pkg) {
                    dialog_error->set_text(
                        TTR("Error opening package file, not in ZIP format.")
                    );
                    dialog_error->popup_centered_minsize();
                    return;
                }

                // Find the zip_root
                String zip_root;
                int ret = unzGoToFirstFile(pkg);
                while (ret == UNZ_OK) {
                    unz_file_info info;
                    char fname[16384];
                    unzGetCurrentFileInfo(
                        pkg,
                        &info,
                        fname,
                        16384,
                        nullptr,
                        0,
                        nullptr,
                        0
                    );

                    String name = String::utf8(fname);
                    if (name.ends_with("project.rebel")) {
                        zip_root = name.substr(0, name.rfind("project.rebel"));
                        break;
                    }

                    ret = unzGoToNextFile(pkg);
                }

                ret = unzGoToFirstFile(pkg);

                Vector<String> failed_files;

                while (ret == UNZ_OK) {
                    // get filename
                    unz_file_info info;
                    char fname[16384];
                    ret = unzGetCurrentFileInfo(
                        pkg,
                        &info,
                        fname,
                        16384,
                        nullptr,
                        0,
                        nullptr,
                        0
                    );

                    String path = String::utf8(fname);

                    if (path == String() || path == zip_root
                        || !zip_root.is_subsequence_of(path)) {
                        //
                    } else if (path.ends_with("/")) { // a dir

                        path            = path.substr(0, path.length() - 1);
                        String rel_path = path.substr(zip_root.length());

                        DirAccess* da =
                            DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
                        da->make_dir(dir.plus_file(rel_path));
                        memdelete(da);

                    } else {
                        Vector<uint8_t> data;
                        data.resize(info.uncompressed_size);
                        String rel_path = path.substr(zip_root.length());

                        // read
                        unzOpenCurrentFile(pkg);
                        unzReadCurrentFile(pkg, data.ptrw(), data.size());
                        unzCloseCurrentFile(pkg);

                        FileAccess* f = FileAccess::open(
                            dir.plus_file(rel_path),
                            FileAccess::WRITE
                        );

                        if (f) {
                            f->store_buffer(data.ptr(), data.size());
                            memdelete(f);
                        } else {
                            failed_files.push_back(rel_path);
                        }
                    }
                    ret = unzGoToNextFile(pkg);
                }

                unzClose(pkg);

                if (failed_files.size()) {
                    String msg = TTR("The following files failed "
                                     "extraction from package:")
                               + "\n\n";
                    for (int i = 0; i < failed_files.size(); i++) {
                        if (i > 15) {
                            msg += "\nAnd " + itos(failed_files.size() - i)
                                 + " more files.";
                            break;
                        }
                        msg += failed_files[i] + "\n";
                    }

                    dialog_error->set_text(msg);
                    dialog_error->popup_centered_minsize();

                } else if (!project_path->get_text().ends_with(".zip")) {
                    dialog_error->set_text(TTR("Package installed successfully!"
                    ));
                    dialog_error->popup_centered_minsize();
                }
            }
        }

        dir = dir.replace("\\", "/");
        if (dir.ends_with("/")) {
            dir = dir.substr(0, dir.length() - 1);
        }
        String proj = dir.replace("/", "::");
        EditorSettings::get_singleton()->set("projects/" + proj, dir);
        EditorSettings::get_singleton()->save();

        hide();
        emit_signal("project_created", dir);
    }
}

void ProjectDialog::_path_selected(const String& p_path) {
    String sp = p_path.simplify_path();
    project_path->set_text(sp);
    _path_text_changed(sp);
    get_ok()->call_deferred("grab_focus");
}

void ProjectDialog::_path_text_changed(const String& p_path) {
    String sp = _test_path();
    if (sp != "") {
        // If the project name is empty or default, infer the project name
        // from the selected folder name
        if (project_name->get_text().strip_edges() == ""
            || project_name->get_text().strip_edges()
                   == TTR("New Game Project")) {
            sp       = sp.replace("\\", "/");
            int lidx = sp.find_last("/");

            if (lidx != -1) {
                sp = sp.substr(lidx + 1, sp.length()).capitalize();
            }
            if (sp == "" && mode == MODE_ADD) {
                sp = TTR("Added Project");
            }

            project_name->set_text(sp);
            _text_changed(sp);
        }
    }

    if (created_folder_path != "" && created_folder_path != p_path) {
        _remove_created_folder();
    }
}

void ProjectDialog::_remove_created_folder() {
    if (created_folder_path != "") {
        DirAccess* d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
        d->remove(created_folder_path);
        memdelete(d);

        create_dir->set_disabled(false);
        created_folder_path = "";
    }
}

void ProjectDialog::_set_message(
    const String& p_msg,
    MessageType p_type,
    InputType input_type
) {
    msg->set_text(p_msg);
    Ref<Texture> current_path_icon    = status_rect->get_texture();
    Ref<Texture> current_install_icon = install_status_rect->get_texture();
    Ref<Texture> new_icon;

    switch (p_type) {
        case MESSAGE_ERROR: {
            msg->add_color_override(
                "font_color",
                get_color("error_color", "Editor")
            );
            msg->set_modulate(Color(1, 1, 1, 1));
            new_icon = get_icon("StatusError", "EditorIcons");

        } break;
        case MESSAGE_WARNING: {
            msg->add_color_override(
                "font_color",
                get_color("warning_color", "Editor")
            );
            msg->set_modulate(Color(1, 1, 1, 1));
            new_icon = get_icon("StatusWarning", "EditorIcons");

        } break;
        case MESSAGE_SUCCESS: {
            msg->set_modulate(Color(1, 1, 1, 0));
            new_icon = get_icon("StatusSuccess", "EditorIcons");

        } break;
    }

    if (current_path_icon != new_icon && input_type == PROJECT_PATH) {
        status_rect->set_texture(new_icon);
    } else if (current_install_icon != new_icon && input_type == INSTALL_PATH) {
        install_status_rect->set_texture(new_icon);
    }

    set_size(Size2(500, 0) * EDSCALE);
}

void ProjectDialog::_text_changed(const String& p_text) {
    if (mode != MODE_NEW) {
        return;
    }

    _test_path();

    if (p_text.strip_edges() == "") {
        _set_message(
            TTR("It would be a good idea to name your project."),
            MESSAGE_ERROR
        );
    }
}

String ProjectDialog::_test_path() {
    DirAccess* d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    String valid_path, valid_install_path;
    if (d->change_dir(project_path->get_text()) == OK) {
        valid_path = project_path->get_text();
    } else if (d->change_dir(project_path->get_text().strip_edges()) == OK) {
        valid_path = project_path->get_text().strip_edges();
    } else if (project_path->get_text().ends_with(".zip")) {
        if (d->file_exists(project_path->get_text())) {
            valid_path = project_path->get_text();
        }
    } else if (project_path->get_text().strip_edges().ends_with(".zip")) {
        if (d->file_exists(project_path->get_text().strip_edges())) {
            valid_path = project_path->get_text().strip_edges();
        }
    }

    if (valid_path == "") {
        _set_message(TTR("The path specified doesn't exist."), MESSAGE_ERROR);
        memdelete(d);
        get_ok()->set_disabled(true);
        return "";
    }

    if (mode == MODE_ADD && valid_path.ends_with(".zip")) {
        if (d->change_dir(install_path->get_text()) == OK) {
            valid_install_path = install_path->get_text();
        } else if (d->change_dir(install_path->get_text().strip_edges()) == OK) {
            valid_install_path = install_path->get_text().strip_edges();
        }

        if (valid_install_path == "") {
            _set_message(
                TTR("The path specified doesn't exist."),
                MESSAGE_ERROR,
                INSTALL_PATH
            );
            memdelete(d);
            get_ok()->set_disabled(true);
            return "";
        }
    }

    if (mode == MODE_ADD || mode == MODE_RENAME) {
        if (valid_path != "" && !d->file_exists("project.rebel")) {
            if (valid_path.ends_with(".zip")) {
                FileAccess* src_f    = nullptr;
                zlib_filefunc_def io = zipio_create_io_from_file(&src_f);

                unzFile pkg = unzOpen2(valid_path.utf8().get_data(), &io);
                if (!pkg) {
                    _set_message(
                        TTR("Error opening package file (it's not in ZIP "
                            "format)."),
                        MESSAGE_ERROR
                    );
                    memdelete(d);
                    get_ok()->set_disabled(true);
                    unzClose(pkg);
                    return "";
                }

                int ret = unzGoToFirstFile(pkg);
                while (ret == UNZ_OK) {
                    unz_file_info info;
                    char fname[16384];
                    ret = unzGetCurrentFileInfo(
                        pkg,
                        &info,
                        fname,
                        16384,
                        nullptr,
                        0,
                        nullptr,
                        0
                    );

                    if (String::utf8(fname).ends_with("project.rebel")) {
                        break;
                    }

                    ret = unzGoToNextFile(pkg);
                }

                if (ret == UNZ_END_OF_LIST_OF_FILE) {
                    _set_message(
                        TTR("Invalid \".zip\" project file; it doesn't "
                            "contain a \"project.rebel\" file."),
                        MESSAGE_ERROR
                    );
                    memdelete(d);
                    get_ok()->set_disabled(true);
                    unzClose(pkg);
                    return "";
                }

                unzClose(pkg);

                // check if the specified install folder is empty, even
                // though this is not an error, it is good to check here
                d->list_dir_begin();
                bool is_empty = true;
                String n      = d->get_next();
                while (n != String()) {
                    if (!n.begins_with(".")) {
                        // Allow `.`, `..` (reserved current/parent folder
                        // names) and hidden files/folders to be present.
                        // For instance, this lets users initialize a Git
                        // repository and still be able to create a project
                        // in the directory afterwards.
                        is_empty = false;
                        break;
                    }
                    n = d->get_next();
                }
                d->list_dir_end();

                if (!is_empty) {
                    _set_message(
                        TTR("Please choose an empty folder."),
                        MESSAGE_WARNING,
                        INSTALL_PATH
                    );
                    memdelete(d);
                    get_ok()->set_disabled(true);
                    return "";
                }

            } else {
                _set_message(
                    TTR("Please choose a \"project.rebel\" or \".zip\" "
                        "file."),
                    MESSAGE_ERROR
                );
                memdelete(d);
                install_path_container->hide();
                get_ok()->set_disabled(true);
                return "";
            }

        } else if (valid_path.ends_with("zip")) {
            _set_message(
                TTR("This directory already contains a Rebel project."),
                MESSAGE_ERROR,
                INSTALL_PATH
            );
            memdelete(d);
            get_ok()->set_disabled(true);
            return "";
        }

    } else {
        // check if the specified folder is empty, even though this is not
        // an error, it is good to check here
        d->list_dir_begin();
        bool is_empty = true;
        String n      = d->get_next();
        while (n != String()) {
            if (!n.begins_with(".")) {
                // Allow `.`, `..` (reserved current/parent folder names)
                // and hidden files/folders to be present.
                // For instance, this lets users initialize a Git repository
                // and still be able to create a project in the directory
                // afterwards.
                is_empty = false;
                break;
            }
            n = d->get_next();
        }
        d->list_dir_end();

        if (!is_empty) {
            _set_message(TTR("Please choose an empty folder."), MESSAGE_ERROR);
            memdelete(d);
            get_ok()->set_disabled(true);
            return "";
        }
    }

    _set_message("");
    _set_message("", MESSAGE_SUCCESS, INSTALL_PATH);
    memdelete(d);
    get_ok()->set_disabled(false);
    return valid_path;
}
