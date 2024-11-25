// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "import_project_dialog.h"

#include "core/io/zip_io.h"
#include "core/os/file_access.h"
#include "core/version.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "editor/editor_themes.h"
#include "projects_list.h"
#include "scene/gui/check_box.h"
#include "scene/gui/separator.h"

#ifndef SERVER_ENABLED
#include "drivers/gles3/rasterizer_gles3.h"
#endif // SERVER_ENABLED

ImportProjectDialog::ImportProjectDialog() {
    VBoxContainer* dialog_container = memnew(VBoxContainer);
    add_child(dialog_container);

    name_container = memnew(VBoxContainer);
    dialog_container->add_child(name_container);

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
    dialog_container->add_child(path_container);

    l = memnew(Label);
    l->set_text(TTR("Project Path:"));
    path_container->add_child(l);

    HBoxContainer* pphb = memnew(HBoxContainer);
    path_container->add_child(pphb);

    project_path = memnew(LineEdit);
    project_path->set_h_size_flags(SIZE_EXPAND_FILL);
    pphb->add_child(project_path);

    install_path_container = memnew(VBoxContainer);
    dialog_container->add_child(install_path_container);

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
    dialog_container->add_child(msg);

    // rasterizer selection
    rasterizer_container = memnew(VBoxContainer);
    dialog_container->add_child(rasterizer_container);
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
    // Projects Manager isn't used in headless builds.
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

    dialog_error = memnew(AcceptDialog);
    add_child(dialog_error);
}

void ImportProjectDialog::show_dialog() {
    fav_dir = EditorSettings::get_singleton()->get(
        "filesystem/directories/default_project_path"
    );
    if (!fav_dir.empty()) {
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

    set_title(TTR("Import Existing Project"));
    get_ok()->set_text(TTR("Import & Edit"));
    name_container->hide();
    install_path_container->hide();
    rasterizer_container->hide();
    project_path->grab_focus();

    _test_path();

    // Reset the dialog to its initial size. Otherwise, the dialog window
    // would be too large when opening a small dialog after closing a large
    // dialog.
    set_size(get_minimum_size());
    popup_centered_minsize(Size2(500, 0) * EDSCALE);
}

void ImportProjectDialog::_bind_methods() {
    ClassDB::bind_method("_browse_path", &ImportProjectDialog::_browse_path);
    ClassDB::bind_method(
        "_create_folder",
        &ImportProjectDialog::_create_folder
    );
    ClassDB::bind_method("_text_changed", &ImportProjectDialog::_text_changed);
    ClassDB::bind_method(
        "_path_text_changed",
        &ImportProjectDialog::_path_text_changed
    );
    ClassDB::bind_method(
        "_path_selected",
        &ImportProjectDialog::_path_selected
    );
    ClassDB::bind_method(
        "_file_selected",
        &ImportProjectDialog::_file_selected
    );
    ClassDB::bind_method(
        "_install_path_selected",
        &ImportProjectDialog::_install_path_selected
    );
    ClassDB::bind_method(
        "_browse_install_path",
        &ImportProjectDialog::_browse_install_path
    );

    ADD_SIGNAL(MethodInfo("add_zip_file"));
    ADD_SIGNAL(MethodInfo("project_added"));
}

void ImportProjectDialog::_notification(int p_what) {
    if (p_what == MainLoop::NOTIFICATION_WM_QUIT_REQUEST) {
        _remove_created_folder();
    }
}

void ImportProjectDialog::ok_pressed() {
    String dir = project_path->get_text();

    if (dir.ends_with("zip")) {
        emit_signal("add_zip_file", dir);
    } else {
        emit_signal("project_added", dir);
    }
}

void ImportProjectDialog::_browse_path() {
    fdialog->set_current_dir(project_path->get_text());
    fdialog->set_mode(FileDialog::MODE_OPEN_FILE);
    fdialog->clear_filters();
    fdialog->add_filter(
        vformat("project.rebel ; %s %s", VERSION_NAME, TTR("Project"))
    );
    fdialog->add_filter("*.zip ; " + TTR("ZIP File"));
    fdialog->popup_centered_ratio();
}

void ImportProjectDialog::_browse_install_path() {
    fdialog_install->set_current_dir(install_path->get_text());
    fdialog_install->set_mode(FileDialog::MODE_OPEN_DIR);
    fdialog_install->popup_centered_ratio();
}

void ImportProjectDialog::_cancel_pressed() {
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

void ImportProjectDialog::_create_folder() {
    const String project_name_no_edges = project_name->get_text().strip_edges();
    if (project_name_no_edges.empty() || !created_folder_path.empty()
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

void ImportProjectDialog::_file_selected(const String& p_path) {
    String p = p_path;
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
    String sp = p.simplify_path();
    project_path->set_text(sp);
    _path_text_changed(sp);
    if (p.ends_with(".zip")) {
        install_path->call_deferred("grab_focus");
    } else {
        get_ok()->call_deferred("grab_focus");
    }
}

void ImportProjectDialog::_install_path_selected(const String& p_path) {
    String sp = p_path.simplify_path();
    install_path->set_text(sp);
    _path_text_changed(sp);
    get_ok()->call_deferred("grab_focus");
}

void ImportProjectDialog::_path_selected(const String& p_path) {
    String sp = p_path.simplify_path();
    project_path->set_text(sp);
    _path_text_changed(sp);
    get_ok()->call_deferred("grab_focus");
}

void ImportProjectDialog::_path_text_changed(const String& p_path) {
    String sp = _test_path();
    if (!sp.empty()) {
        // If the project name is empty or default, infer the project name
        // from the selected folder name
        if (project_name->get_text().strip_edges().empty()
            || project_name->get_text().strip_edges()
                   == TTR("New Game Project")) {
            sp       = sp.replace("\\", "/");
            int lidx = sp.find_last("/");

            if (lidx != -1) {
                sp = sp.substr(lidx + 1, sp.length()).capitalize();
            }
            if (sp.empty()) {
                sp = TTR("Imported Project");
            }

            project_name->set_text(sp);
            _text_changed(sp);
        }
    }

    if (!created_folder_path.empty() && created_folder_path != p_path) {
        _remove_created_folder();
    }
}

void ImportProjectDialog::_remove_created_folder() {
    if (!created_folder_path.empty()) {
        DirAccess* d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
        d->remove(created_folder_path);
        memdelete(d);

        create_dir->set_disabled(false);
        created_folder_path = "";
    }
}

void ImportProjectDialog::_set_message(
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

String ImportProjectDialog::_test_path() {
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

    if (valid_path.empty()) {
        _set_message(TTR("The path specified doesn't exist."), MESSAGE_ERROR);
        memdelete(d);
        get_ok()->set_disabled(true);
        return "";
    }

    if (valid_path.ends_with(".zip")) {
        if (d->change_dir(install_path->get_text()) == OK) {
            valid_install_path = install_path->get_text();
        } else if (d->change_dir(install_path->get_text().strip_edges()) == OK) {
            valid_install_path = install_path->get_text().strip_edges();
        }

        if (valid_install_path.empty()) {
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

    if (!valid_path.empty() && !d->file_exists("project.rebel")) {
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
            while (!n.empty()) {
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

    _set_message("");
    _set_message("", MESSAGE_SUCCESS, INSTALL_PATH);
    memdelete(d);
    get_ok()->set_disabled(false);
    return valid_path;
}

void ImportProjectDialog::_text_changed(const String& p_text) {
    return;
}
