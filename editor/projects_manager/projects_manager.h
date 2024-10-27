// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef PROJECTS_MANAGER_H
#define PROJECTS_MANAGER_H

#include "core/list.h"
#include "core/os/input_event.h"
#include "core/ustring.h"
#include "core/variant.h"
#include "editor/editor_about.h"
#include "editor/plugins/asset_library_editor_plugin.h"
#include "projects_dialog.h"
#include "projects_list.h"
#include "projects_list_filter.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/control.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/label.h"
#include "scene/gui/link_button.h"
#include "scene/gui/option_button.h"
#include "scene/gui/tab_container.h"

class ProjectsManager : public Control {
    GDCLASS(ProjectsManager, Control);

public:
    ProjectsManager();
    ~ProjectsManager();

protected:
    static void _bind_methods();
    void _notification(int p_what);

private:
    AcceptDialog* dialog_error;
    AcceptDialog* run_error_diag;

    Button* about_btn;
    Button* erase_btn;
    Button* erase_missing_btn;
    Button* open_btn;
    Button* rename_btn;
    Button* run_btn;

    CheckBox* delete_project_contents;

    ConfirmationDialog* ask_update_settings;
    ConfirmationDialog* erase_ask;
    ConfirmationDialog* erase_missing_ask;
    ConfirmationDialog* language_restart_ask;
    ConfirmationDialog* multi_open_ask;
    ConfirmationDialog* multi_run_ask;
    ConfirmationDialog* multi_scan_ask;
    ConfirmationDialog* open_templates;

    EditorAbout* about;
    EditorAssetLibrary* asset_library;

    FileDialog* scan_dir;

    Label* erase_ask_label;

    LinkButton* version_btn;

    OptionButton* language_btn;

    ProjectsDialog* npdialog;
    ProjectsList* projects_list;

    TabContainer* tabs;

    void _confirm_update_settings();
    void _dim_window();
    void _erase_missing_projects();
    void _erase_missing_projects_confirm();
    void _erase_project();
    void _erase_project_confirm();
    void _files_dropped(const PoolStringArray& p_files, int p_screen);
    void _global_menu_action(const Variant& p_id, const Variant& p_meta);
    void _import_project();
    void _install_project(const String& p_zip_path, const String& p_title);
    void _language_selected(int p_id);
    void _new_project();
    void _on_project_created(const String& dir);
    void _on_projects_updated();
    void _on_tab_changed(int p_tab);
    void _open_asset_library();
    void _open_selected_projects_ask();
    void _open_selected_projects();
    void _rename_project();
    void _restart_confirm();
    void _run_project_confirm();
    void _run_project();
    void _scan_begin(const String& p_base);
    void _scan_dir(const String& path, List<String>* r_projects);
    void _scan_multiple_folders(const PoolStringArray& p_files);
    void _scan_projects();
    void _show_about();
    void _unhandled_input(const Ref<InputEvent>& p_ev);
    void _update_project_buttons();
    void _version_button_pressed();
};

#endif // PROJECTS_MANAGER_H
