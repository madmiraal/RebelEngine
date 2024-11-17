// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "projects_manager.h"

#include "core/io/config_file.h"
#include "core/io/resource_saver.h"
#include "core/io/stream_peer_ssl.h"
#include "core/io/zip_io.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/translation.h"
#include "core/version.h"
#include "core/version_hash.gen.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "editor/editor_themes.h"
#include "projects_list_item.h"
#include "scene/gui/center_container.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/texture_rect.h"
#include "scene/gui/tool_button.h"

// Used to test for GLES3 support.
#ifndef SERVER_ENABLED
#include "drivers/gles3/rasterizer_gles3.h"
#endif

namespace {
void apply_editor_settings() {
    if (!EditorSettings::get_singleton()) {
        EditorSettings::create();
    }
    EditorSettings* editor_settings = EditorSettings::get_singleton();

    editor_settings->set_optimize_save(false);
    int display_scale = editor_settings->get("interface/editor/display_scale");
    float custom_display_scale =
        editor_settings->get("interface/editor/custom_display_scale");
    switch (display_scale) {
        case 0:
            // Try applying a suitable display scale automatically.
            editor_set_scale(editor_settings->get_auto_display_scale());
            break;
        case 1:
            editor_set_scale(0.75);
            break;
        case 2:
            editor_set_scale(1.0);
            break;
        case 3:
            editor_set_scale(1.25);
            break;
        case 4:
            editor_set_scale(1.5);
            break;
        case 5:
            editor_set_scale(1.75);
            break;
        case 6:
            editor_set_scale(2.0);
            break;
        default:
            editor_set_scale(custom_display_scale);
            break;
    }

    FileDialog::set_default_show_hidden_files(
        editor_settings->get("filesystem/file_dialog/show_hidden_files")
    );
}

void apply_window_settings() {
    OS* os = OS::get_singleton();
    os->set_min_window_size(Size2(750, 420) * EDSCALE);
    // TODO: Automatically resize hiDPI display windows on Windows and Linux.
    os->set_window_size(os->get_window_size() * MAX(1, EDSCALE));
    // TRANSLATORS: Projects Manager is the application used to manage projects.
    os->set_window_title(
        VERSION_NAME + String(" - ") + TTR("Projects Manager")
    );
    os->set_low_processor_usage_mode(true);
}

void create_project_directories() {
    DirAccessRef dir_access =
        DirAccess::create(DirAccess::AccessType::ACCESS_FILESYSTEM);

    String default_project_path = EditorSettings::get_singleton()->get(
        "filesystem/directories/default_project_path"
    );
    Error error = dir_access->make_dir_recursive(default_project_path);
    if (error != OK) {
        ERR_PRINT(
            "Could not create default project directory: "
            + default_project_path
        );
    }

    String autoscan_project_path = EditorSettings::get_singleton()->get(
        "filesystem/directories/autoscan_project_path"
    );
    error = dir_access->make_dir_recursive(autoscan_project_path);
    if (error != OK) {
        ERR_PRINT(
            "Could not create autoscan project directory: "
            + autoscan_project_path
        );
    }
}

void edit_project(const String& project_name, const String& project_folder) {
    print_line(vformat("Editing project: %s (%s)", project_name, project_folder)
    );

    OS* os = OS::get_singleton();
    List<String> args;
    args.push_back("--path");
    args.push_back(project_folder);
    args.push_back("--editor");
    if (os->is_stdout_debug_enabled()) {
        args.push_back("--debug");
    }
    if (os->is_stdout_verbose()) {
        args.push_back("--verbose");
    }
    if (os->is_disable_crash_handler()) {
        args.push_back("--disable-crash-handler");
    }
    String exec       = os->get_executable_path();
    OS::ProcessID pid = 0;
    Error error       = os->execute(exec, args, false, &pid);
    ERR_FAIL_COND(error);
}

void run_project(const String& project_name, const String& project_folder) {
    print_line(vformat("Running project: %s (%s)", project_name, project_folder)
    );
    List<String> args;
    args.push_back("--path");
    args.push_back(project_folder);
    if (OS::get_singleton()->is_disable_crash_handler()) {
        args.push_back("--disable-crash-handler");
    }
    String exec       = OS::get_singleton()->get_executable_path();
    OS::ProcessID pid = 0;
    Error error       = OS::get_singleton()->execute(exec, args, false, &pid);
    ERR_FAIL_COND(error);
}
} // namespace

ProjectsManager::ProjectsManager() {
    apply_editor_settings();
    apply_window_settings();
    create_project_directories();

    set_anchors_and_margins_preset(Control::PRESET_WIDE);
    set_theme(create_custom_theme());
    add_style_override("panel", get_stylebox("Background", "EditorStyles"));

    VBoxContainer* panel_container = memnew(VBoxContainer);
    panel_container->set_anchors_and_margins_preset(
        Control::PRESET_WIDE,
        Control::PRESET_MODE_MINSIZE,
        8 * EDSCALE
    );
    add_child(panel_container);

    Control* center_box = memnew(Control);
    center_box->set_v_size_flags(SIZE_EXPAND_FILL);
    panel_container->add_child(center_box);

    center_box->add_child(_create_tabs());
    center_box->add_child(_create_tools());

    _create_dialogs();

    _update_project_buttons();

    String autoscan_project_path = EditorSettings::get_singleton()->get(
        "filesystem/directories/autoscan_project_path"
    );
    if (!autoscan_project_path.empty()
        && DirAccess::exists(autoscan_project_path)) {
        _scan_begin(autoscan_project_path);
    }

    SceneTree::get_singleton()
        ->connect("files_dropped", this, "_files_dropped");
    SceneTree::get_singleton()
        ->connect("global_menu_action", this, "_global_menu_action");

    about = memnew(EditorAbout);
    add_child(about);

    scan_dir = memnew(FileDialog);
    scan_dir->set_access(FileDialog::ACCESS_FILESYSTEM);
    scan_dir->set_mode(FileDialog::MODE_OPEN_DIR);
    scan_dir->set_title(TTR("Select a Folder to Scan")
    ); // must be after mode or it's overridden
    scan_dir->set_current_dir(EditorSettings::get_singleton()->get(
        "filesystem/directories/default_project_path"
    ));
    add_child(scan_dir);
    scan_dir->connect("dir_selected", this, "_scan_begin");
}

ProjectsManager::~ProjectsManager() {
    if (EditorSettings::get_singleton()) {
        EditorSettings::destroy();
    }
}

void ProjectsManager::_bind_methods() {
    ClassDB::bind_method(
        "_on_about_button_pressed",
        &ProjectsManager::_on_about_button_pressed
    );
    ClassDB::bind_method(
        "_on_edit_button_pressed",
        &ProjectsManager::_on_edit_button_pressed
    );
    ClassDB::bind_method(
        "_on_edit_multiple_confirmed",
        &ProjectsManager::_on_edit_multiple_confirmed
    );
    ClassDB::bind_method(
        "_on_import_button_pressed",
        &ProjectsManager::_on_import_button_pressed
    );
    ClassDB::bind_method(
        "_on_item_double_clicked",
        &ProjectsManager::_on_item_double_clicked
    );
    ClassDB::bind_method(
        "_on_language_selected",
        &ProjectsManager::_on_language_selected
    );
    ClassDB::bind_method(
        "_on_open_asset_library_confirmed",
        &ProjectsManager::_on_open_asset_library_confirmed
    );
    ClassDB::bind_method(
        "_on_new_project_button_pressed",
        &ProjectsManager::_on_new_project_button_pressed
    );
    ClassDB::bind_method(
        "_on_rename_button_pressed",
        &ProjectsManager::_on_rename_button_pressed
    );
    ClassDB::bind_method(
        "_on_remove_button_pressed",
        &ProjectsManager::_on_remove_button_pressed
    );
    ClassDB::bind_method(
        "_on_remove_confirmed",
        &ProjectsManager::_on_remove_confirmed
    );
    ClassDB::bind_method(
        "_on_remove_missing_button_pressed",
        &ProjectsManager::_on_remove_missing_button_pressed
    );
    ClassDB::bind_method(
        "_on_remove_missing_confirmed",
        &ProjectsManager::_on_remove_missing_confirmed
    );
    ClassDB::bind_method(
        "_on_restart_confirmed",
        &ProjectsManager::_on_restart_confirmed
    );
    ClassDB::bind_method(
        "_on_run_button_pressed",
        &ProjectsManager::_on_run_button_pressed
    );
    ClassDB::bind_method(
        "_on_run_multiple_confirmed",
        &ProjectsManager::_on_run_multiple_confirmed
    );
    ClassDB::bind_method(
        "_on_scan_button_pressed",
        &ProjectsManager::_on_scan_button_pressed
    );
    ClassDB::bind_method(
        D_METHOD("_on_scan_multiple_folders_confirmed", "folders"),
        &ProjectsManager::_on_scan_multiple_folders_confirmed
    );
    ClassDB::bind_method(
        "_on_selection_changed",
        &ProjectsManager::_on_selection_changed
    );
    ClassDB::bind_method("_on_tab_changed", &ProjectsManager::_on_tab_changed);
    ClassDB::bind_method(
        "_on_upgrade_settings_confirmed",
        &ProjectsManager::_on_upgrade_settings_confirmed
    );
    ClassDB::bind_method(
        "_on_version_label_pressed",
        &ProjectsManager::_on_version_label_pressed
    );

    ClassDB::bind_method(
        D_METHOD("_global_menu_action"),
        &ProjectsManager::_global_menu_action,
        DEFVAL(Variant())
    );
    ClassDB::bind_method("_scan_begin", &ProjectsManager::_scan_begin);
    ClassDB::bind_method(
        "_on_projects_updated",
        &ProjectsManager::_on_projects_updated
    );
    ClassDB::bind_method(
        "_on_project_created",
        &ProjectsManager::_on_project_created
    );
    ClassDB::bind_method(
        "_unhandled_input",
        &ProjectsManager::_unhandled_input
    );
    ClassDB::bind_method(
        "_install_project",
        &ProjectsManager::_install_project
    );
    ClassDB::bind_method("_files_dropped", &ProjectsManager::_files_dropped);
}

void ProjectsManager::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            Engine::get_singleton()->set_editor_hint(false);
        } break;
        case NOTIFICATION_RESIZED: {
            if (open_asset_library_confirmation->is_visible()) {
                open_asset_library_confirmation->popup_centered_minsize();
            }
        } break;
        case NOTIFICATION_READY: {
            if (projects_list->get_project_count() == 0
                && StreamPeerSSL::is_available()) {
                open_asset_library_confirmation->popup_centered_minsize();
            }
        } break;
        case NOTIFICATION_VISIBILITY_CHANGED: {
            set_process_unhandled_input(is_visible_in_tree());
        } break;
        case NOTIFICATION_WM_QUIT_REQUEST: {
            _dim_window();
        } break;
        case NOTIFICATION_WM_ABOUT: {
            _show_about();
        } break;
    }
}

Control* ProjectsManager::_create_buttons() {
    VBoxContainer* buttons_container = memnew(VBoxContainer);
    buttons_container->set_custom_minimum_size(Size2(120, 120));

    edit_button = memnew(Button);
    edit_button->set_text(TTR("Edit"));
    edit_button->set_shortcut(ED_SHORTCUT(
        "projects_manager/edit_project",
        TTR("Edit Project"),
        KEY_MASK_CMD | KEY_E
    ));
    edit_button->connect("pressed", this, "_on_edit_button_pressed");
    buttons_container->add_child(edit_button);

    run_button = memnew(Button);
    run_button->set_text(TTR("Run"));
    run_button->set_shortcut(ED_SHORTCUT(
        "projects_manager/run_project",
        TTR("Run Project"),
        KEY_MASK_CMD | KEY_R
    ));
    run_button->connect("pressed", this, "_on_run_button_pressed");
    buttons_container->add_child(run_button);

    buttons_container->add_child(memnew(HSeparator));

    Button* scan_button = memnew(Button);
    scan_button->set_text(TTR("Scan"));
    scan_button->set_shortcut(ED_SHORTCUT(
        "projects_manager/scan_projects",
        TTR("Scan Projects"),
        KEY_MASK_CMD | KEY_S
    ));
    scan_button->connect("pressed", this, "_on_scan_button_pressed");
    buttons_container->add_child(scan_button);

    buttons_container->add_child(memnew(HSeparator));

    Button* new_project_button = memnew(Button);
    new_project_button->set_text(TTR("New Project"));
    new_project_button->set_shortcut(ED_SHORTCUT(
        "projects_manager/new_project",
        TTR("New Project"),
        KEY_MASK_CMD | KEY_N
    ));
    new_project_button
        ->connect("pressed", this, "_on_new_project_button_pressed");
    buttons_container->add_child(new_project_button);

    Button* import_button = memnew(Button);
    import_button->set_text(TTR("Import"));
    import_button->set_shortcut(ED_SHORTCUT(
        "projects_manager/import_project",
        TTR("Import existing project"),
        KEY_MASK_CMD | KEY_I
    ));
    import_button->connect("pressed", this, "_on_import_button_pressed");
    buttons_container->add_child(import_button);

    rename_button = memnew(Button);
    rename_button->set_text(TTR("Rename"));
    rename_button->set_shortcut(ED_SHORTCUT(
        "projects_manager/rename_project",
        TTR("Rename Project"),
        KEY_F2
    ));
    rename_button->connect("pressed", this, "_on_rename_button_pressed");
    buttons_container->add_child(rename_button);

    remove_button = memnew(Button);
    remove_button->set_text(TTR("Remove"));
    remove_button->set_shortcut(ED_SHORTCUT(
        "projects_manager/remove_project",
        TTR("Remove Project"),
        KEY_DELETE
    ));
    remove_button->connect("pressed", this, "_on_remove_button_pressed");
    buttons_container->add_child(remove_button);

    remove_missing_button = memnew(Button);
    remove_missing_button->set_text(TTR("Remove Missing"));
    remove_missing_button
        ->connect("pressed", this, "_on_remove_missing_button_pressed");
    buttons_container->add_child(remove_missing_button);

    buttons_container->add_spacer();

    Button* about_button = memnew(Button);
    about_button->set_text(TTR("About"));
    about_button->connect("pressed", this, "_on_about_button_pressed");
    buttons_container->add_child(about_button);

    return buttons_container;
}

void ProjectsManager::_create_dialogs() {
    add_child(_create_edit_multiple_confirmation());
    add_child(_create_open_asset_library_confirmation());
    add_child(_create_remove_confirmation());
    add_child(_create_remove_missing_confirmation());
    add_child(_create_restart_confirmation());
    add_child(_create_run_multiple_confirmation());
    add_child(_create_scan_multiple_folders_confirmation());
    add_child(_create_upgrade_settings_confirmation());

    add_child(_create_newer_settings_file_version_error());
    add_child(_create_no_assets_folder_error());
    add_child(_create_no_main_scene_defined_error());
    add_child(_create_no_settings_file_error());

    add_child(_create_projects_dialog());
}

Control* ProjectsManager::_create_edit_multiple_confirmation() {
    edit_multiple_confirmation = memnew(ConfirmationDialog);
    edit_multiple_confirmation->set_text(
        TTR("Are you sure you want to edit more than one project?")
    );
    edit_multiple_confirmation->get_ok()->set_text(TTR("Edit"));
    edit_multiple_confirmation->get_ok()
        ->connect("pressed", this, "_on_edit_multiple_confirmed");
    return edit_multiple_confirmation;
}

Control* ProjectsManager::_create_language_options() {
    language_options = memnew(OptionButton);
    language_options->set_flat(true);
    language_options->set_focus_mode(Control::FOCUS_NONE);
    language_options->set_icon(get_icon("Environment", "EditorIcons"));
    language_options->connect("item_selected", this, "_on_language_selected");

    Vector<String> language_codes;
    List<PropertyInfo> properties_list;
    EditorSettings::get_singleton()->get_property_list(&properties_list);
    for (List<PropertyInfo>::Element* E = properties_list.front(); E;
         E                              = E->next()) {
        PropertyInfo& property_info = E->get();
        if (property_info.name == "interface/editor/editor_language") {
            language_codes = property_info.hint_string.split(",");
        }
    }

    String current_language_code =
        EditorSettings::get_singleton()->get("interface/editor/editor_language"
        );
    for (int i = 0; i < language_codes.size(); i++) {
        const String& language_code = language_codes[i];
        String language_name =
            TranslationServer::get_singleton()->get_locale_name(language_code);
        language_options->add_item(
            language_name + " [" + language_code + "]",
            i
        );
        language_options->set_item_metadata(i, language_code);
        if (current_language_code == language_code) {
            language_options->select(i);
            language_options->set_text(language_code);
        }
    }

    return language_options;
}

Control* ProjectsManager::_create_newer_settings_file_version_error() {
    newer_settings_file_version_error = memnew(AcceptDialog);
    return newer_settings_file_version_error;
}

Control* ProjectsManager::_create_no_assets_folder_error() {
    no_assets_folder_error = memnew(AcceptDialog);
    no_assets_folder_error->set_title(TTR("Can't run project"));
    no_assets_folder_error->set_text(
        TTR("Can't run project: Assets need to be imported.\n"
            "Please edit the project to trigger the initial import.")
    );
    return no_assets_folder_error;
}

Control* ProjectsManager::_create_no_main_scene_defined_error() {
    no_main_scene_defined_error = memnew(AcceptDialog);
    no_main_scene_defined_error->set_title(TTR("Can't run project"));
    no_main_scene_defined_error->set_text(
        TTR("Can't run project: no main scene defined.\n"
            "Please edit the project and set the main scene in the Project "
            "Settings under the \"Application\" category.")
    );
    return no_main_scene_defined_error;
}

Control* ProjectsManager::_create_no_settings_file_error() {
    no_settings_file_error = memnew(AcceptDialog);
    return no_settings_file_error;
}

Control* ProjectsManager::_create_open_asset_library_confirmation() {
    open_asset_library_confirmation = memnew(ConfirmationDialog);
    open_asset_library_confirmation->set_text(
        TTR("You currently don't have any projects.\n"
            "Would you like to explore official example projects in the Asset "
            "Library?")
    );
    open_asset_library_confirmation->get_ok()->set_text(TTR("Open Asset Library"
    ));
    open_asset_library_confirmation
        ->connect("confirmed", this, "_on_open_asset_library_confirmed");
    return open_asset_library_confirmation;
}

Control* ProjectsManager::_create_projects_dialog() {
    projects_dialog = memnew(ProjectsDialog);
    projects_dialog->connect("projects_updated", this, "_on_projects_updated");
    projects_dialog->connect("project_created", this, "_on_project_created");
    return projects_dialog;
}

Control* ProjectsManager::_create_projects_tab() {
    HBoxContainer* projects_tab_container = memnew(HBoxContainer);
    projects_tab_container->set_name(TTR("Local Projects"));

    projects_list = memnew(ProjectsList);
    projects_list->connect("selection_changed", this, "_on_selection_changed");
    projects_list
        ->connect("item_double_clicked", this, "_on_item_double_clicked");
    projects_tab_container->add_child(projects_list);

    projects_tab_container->add_child(_create_buttons());

    return projects_tab_container;
}

Control* ProjectsManager::_create_remove_confirmation() {
    remove_confirmation = memnew(ConfirmationDialog);
    remove_confirmation->get_ok()->set_text(TTR("Remove"));
    remove_confirmation->get_ok()
        ->connect("pressed", this, "_on_remove_confirmed");

    VBoxContainer* remove_confirmation_vb = memnew(VBoxContainer);
    remove_confirmation->add_child(remove_confirmation_vb);

    remove_confirmation_label = memnew(Label);
    remove_confirmation_vb->add_child(remove_confirmation_label);

    delete_project_contents = memnew(CheckBox);
    delete_project_contents->set_text(TTR("Also delete project contents?"));
    remove_confirmation_vb->add_child(delete_project_contents);

    return remove_confirmation;
}

Control* ProjectsManager::_create_remove_missing_confirmation() {
    remove_missing_confirmation = memnew(ConfirmationDialog);
    remove_missing_confirmation->set_text(
        TTR("Remove all missing projects from the list?\n"
            "The project folders' contents won't be modified.")
    );
    remove_missing_confirmation->get_ok()->set_text(TTR("Remove All"));
    remove_missing_confirmation->get_ok()
        ->connect("pressed", this, "_on_remove_missing_confirmed");
    return remove_missing_confirmation;
}

Control* ProjectsManager::_create_restart_confirmation() {
    restart_confirmation = memnew(ConfirmationDialog);
    restart_confirmation->set_text(
        TTR("Language changed.\n"
            "The interface will be updated after restarting Projects Manager.")
    );
    restart_confirmation->get_ok()->set_text(TTR("Restart Now"));
    restart_confirmation->get_ok()
        ->connect("pressed", this, "_on_restart_confirmed");
    restart_confirmation->get_cancel()->set_text(TTR("Continue"));
    return restart_confirmation;
}

Control* ProjectsManager::_create_run_multiple_confirmation() {
    run_multiple_confirmation = memnew(ConfirmationDialog);
    run_multiple_confirmation->get_ok()->set_text(TTR("Run"));
    run_multiple_confirmation->get_ok()
        ->connect("pressed", this, "_on_run_multiple_confirmed");
    return run_multiple_confirmation;
}

Control* ProjectsManager::_create_scan_multiple_folders_confirmation() {
    scan_multiple_folders_confirmation = memnew(ConfirmationDialog);
    scan_multiple_folders_confirmation->get_ok()->set_text(TTR("Scan"));
    return scan_multiple_folders_confirmation;
}

Control* ProjectsManager::_create_tabs() {
    tabs = memnew(TabContainer);
    tabs->set_anchors_and_margins_preset(Control::PRESET_WIDE);
    tabs->set_tab_align(TabContainer::ALIGN_LEFT);
    tabs->connect("tab_changed", this, "_on_tab_changed");

    tabs->add_child(_create_projects_tab());
    if (StreamPeerSSL::is_available()) {
        asset_library = memnew(EditorAssetLibrary(true));
        asset_library->set_name(TTR("Asset Library Projects"));
        asset_library->connect("install_asset", this, "_install_project");
        tabs->add_child(asset_library);
    } else {
        WARN_PRINT("Asset Library not available, as it requires SSL to work.");
    }
    tabs->set_current_tab(0);

    return tabs;
}

Control* ProjectsManager::_create_tools() {
    HBoxContainer* tools_container = memnew(HBoxContainer);
    tools_container->set_anchors_and_margins_preset(Control::PRESET_TOP_RIGHT);
    tools_container->set_alignment(BoxContainer::ALIGN_END);
    tools_container->set_h_grow_direction(Control::GROW_DIRECTION_BEGIN);

    // Clickable version label.
    VBoxContainer* version_container = memnew(VBoxContainer);
    // Pushes the version label down.
    Control* version_spacer          = memnew(Control);
    version_container->add_child(version_spacer);
    version_label       = memnew(LinkButton);
    String version_hash = String(VERSION_HASH);
    if (!version_hash.empty()) {
        version_hash = vformat(" [%s]", version_hash.left(9));
    }
    version_label->set_text(vformat("v%s%s", VERSION_FULL_BUILD, version_hash));
    version_label->set_self_modulate(Color(1, 1, 1, 0.6));
    version_label->set_underline_mode(LinkButton::UNDERLINE_MODE_ON_HOVER);
    version_label->set_tooltip(TTR("Click to copy."));
    version_label->connect("pressed", this, "_on_version_label_pressed");
    version_container->add_child(version_label);
    tools_container->add_child(version_container);

    tools_container->add_spacer();

    tools_container->add_child(_create_language_options());

    return tools_container;
}

Control* ProjectsManager::_create_upgrade_settings_confirmation() {
    upgrade_settings_confirmation = memnew(ConfirmationDialog);
    upgrade_settings_confirmation->get_ok()
        ->connect("pressed", this, "_on_upgrade_settings_confirmed");
    return upgrade_settings_confirmation;
}

void ProjectsManager::_dim_window() {
    // This method must be called before calling `get_tree()->quit()`.
    // Otherwise, its effect won't be visible

    // Dim the Projects Manager window while it's quitting to make it clearer
    // that it's busy. No transition is applied, as the effect needs to be
    // visible immediately
    float c         = 0.5f;
    Color dim_color = Color(c, c, c);
    set_modulate(dim_color);
}

void ProjectsManager::_files_dropped(
    const PoolStringArray& p_files,
    int p_screen
) {
    if (p_files.size() == 1 && p_files[0].ends_with(".zip")) {
        const String file = p_files[0].get_file();
        _install_project(
            p_files[0],
            file.substr(0, file.length() - 4).capitalize()
        );
        return;
    }
    Set<String> folders_set;
    DirAccess* da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    for (int i = 0; i < p_files.size(); i++) {
        String file = p_files[i];
        folders_set.insert(da->dir_exists(file) ? file : file.get_base_dir());
    }
    memdelete(da);
    if (!folders_set.empty()) {
        PoolStringArray folders;
        for (Set<String>::Element* E = folders_set.front(); E; E = E->next()) {
            folders.append(E->get());
        }

        bool confirm = true;
        if (folders.size() == 1) {
            DirAccess* dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
            if (dir->change_dir(folders[0]) == OK) {
                dir->list_dir_begin();
                String file = dir->get_next();
                while (confirm && !file.empty()) {
                    if (!dir->current_is_dir()
                        && file.ends_with("project.rebel")) {
                        confirm = false;
                    }
                    file = dir->get_next();
                }
                dir->list_dir_end();
            }
            memdelete(dir);
        }
        if (confirm) {
            scan_multiple_folders_confirmation->get_ok()->disconnect(
                "pressed",
                this,
                "_on_scan_multiple_folders_confirmed"
            );
            scan_multiple_folders_confirmation->get_ok()->connect(
                "pressed",
                this,
                "_on_scan_multiple_folders_confirmed",
                varray(folders)
            );
            scan_multiple_folders_confirmation->set_text(vformat(
                TTR("Are you sure to scan %s folders for existing Rebel "
                    "projects?\n"
                    "This could take a while."),
                folders.size()
            ));
            scan_multiple_folders_confirmation->popup_centered_minsize();
        } else {
            _scan_multiple_folders(folders);
        }
    }
}

void ProjectsManager::_global_menu_action(
    const Variant& p_id,
    const Variant& p_meta
) {
    int id = (int)p_id;
    if (id == ProjectsList::GLOBAL_NEW_WINDOW) {
        List<String> args;
        args.push_back("-p");
        String exec = OS::get_singleton()->get_executable_path();

        OS::ProcessID pid = 0;
        OS::get_singleton()->execute(exec, args, false, &pid);
    } else if (id == ProjectsList::GLOBAL_OPEN_PROJECT) {
        String conf = (String)p_meta;

        if (!conf.empty()) {
            List<String> args;
            args.push_back(conf);
            String exec = OS::get_singleton()->get_executable_path();

            OS::ProcessID pid = 0;
            OS::get_singleton()->execute(exec, args, false, &pid);
        }
    }
}

void ProjectsManager::_install_project(
    const String& p_zip_path,
    const String& p_title
) {
    projects_dialog->set_mode(ProjectsDialog::MODE_INSTALL);
    projects_dialog->set_zip_path(p_zip_path);
    projects_dialog->set_zip_title(p_title);
    projects_dialog->show_dialog();
}

void ProjectsManager::_on_about_button_pressed() {
    _show_about();
}

void ProjectsManager::_on_edit_button_pressed() {
    _open_selected_projects_ask();
}

void ProjectsManager::_on_edit_multiple_confirmed() {
    _open_selected_projects();
}

void ProjectsManager::_on_import_button_pressed() {
    projects_dialog->set_mode(ProjectsDialog::MODE_IMPORT);
    projects_dialog->show_dialog();
}

void ProjectsManager::_on_item_double_clicked() {
    _open_selected_projects_ask();
}

void ProjectsManager::_on_language_selected(int p_id) {
    String language_code = language_options->get_item_metadata(p_id);
    EditorSettings::get_singleton()->set(
        "interface/editor/editor_language",
        language_code
    );
    language_options->set_text(language_code);
    language_options->set_icon(get_icon("Environment", "EditorIcons"));
    restart_confirmation->popup_centered();
}

void ProjectsManager::_on_new_project_button_pressed() {
    projects_dialog->set_mode(ProjectsDialog::MODE_NEW);
    projects_dialog->show_dialog();
}

void ProjectsManager::_on_open_asset_library_confirmed() {
    _open_asset_library();
}

void ProjectsManager::_on_project_created(const String& project_key) {
    projects_list->add_project(project_key);
    _open_selected_projects_ask();
}

void ProjectsManager::_on_projects_updated() {
    projects_list->refresh_selected_projects();
}

void ProjectsManager::_on_rename_button_pressed() {
    const Set<String>& selected_project_keys =
        projects_list->get_selected_project_keys();

    if (selected_project_keys.empty()) {
        return;
    }

    for (Set<String>::Element* E = selected_project_keys.front(); E;
         E                       = E->next()) {
        const String& selected = E->get();
        String path =
            EditorSettings::get_singleton()->get("projects/" + selected);
        projects_dialog->set_project_path(path);
        projects_dialog->set_mode(ProjectsDialog::MODE_RENAME);
        projects_dialog->show_dialog();
    }
}

void ProjectsManager::_on_remove_button_pressed() {
    const Set<String>& selected_project_keys =
        projects_list->get_selected_project_keys();

    if (selected_project_keys.empty()) {
        return;
    }

    String confirm_message;
    if (selected_project_keys.size() > 1) {
        confirm_message = vformat(
            TTR("Remove %d projects from the list?"),
            selected_project_keys.size()
        );
    } else {
        confirm_message = TTR("Remove selected project from the list?");
    }

    remove_confirmation_label->set_text(confirm_message);
    delete_project_contents->set_pressed(false);
    remove_confirmation->popup_centered_minsize();
}

void ProjectsManager::_on_remove_confirmed() {
    projects_list->remove_selected_projects(delete_project_contents->is_pressed(
    ));
    _update_project_buttons();
}

void ProjectsManager::_on_remove_missing_button_pressed() {
    remove_missing_confirmation->popup_centered_minsize();
}

void ProjectsManager::_on_remove_missing_confirmed() {
    projects_list->remove_missing_projects();
    _update_project_buttons();
}

void ProjectsManager::_on_restart_confirmed() {
    OS* os            = OS::get_singleton();
    List<String> args = os->get_cmdline_args();
    String exec       = os->get_executable_path();
    OS::ProcessID pid = 0;
    Error err         = os->execute(exec, args, false, &pid);
    ERR_FAIL_COND(err);

    _dim_window();
    get_tree()->quit();
}

void ProjectsManager::_on_run_button_pressed() {
    const Set<String>& selected_project_keys =
        projects_list->get_selected_project_keys();

    if (selected_project_keys.empty()) {
        return;
    }

    if (selected_project_keys.size() > 1) {
        run_multiple_confirmation->set_text(vformat(
            TTR("Are you sure to run %d projects at once?"),
            selected_project_keys.size()
        ));
        run_multiple_confirmation->popup_centered_minsize();
    } else {
        _run_selected();
    }
}

void ProjectsManager::_on_run_multiple_confirmed() {
    _run_selected();
}

void ProjectsManager::_on_scan_button_pressed() {
    scan_dir->popup_centered_ratio();
}

void ProjectsManager::_on_scan_multiple_folders_confirmed(
    const PoolStringArray& p_folders
) {
    _scan_multiple_folders(p_folders);
}

void ProjectsManager::_on_selection_changed() {
    _update_project_buttons();
}

void ProjectsManager::_on_tab_changed(int p_tab) {
    if (p_tab == 0) { // Projects
        projects_list->set_search_focus();
    }

    // The Templates tab's search field is focused on display in the asset
    // library editor plugin code.
}

void ProjectsManager::_on_upgrade_settings_confirmed() {
    _open_selected_projects();
}

void ProjectsManager::_on_version_label_pressed() {
    OS::get_singleton()->set_clipboard(version_label->get_text());
}

void ProjectsManager::_open_asset_library() {
    asset_library->disable_community_support();
    tabs->set_current_tab(1);
}

void ProjectsManager::_open_selected_projects_ask() {
    const Set<String>& selected_project_keys =
        projects_list->get_selected_project_keys();

    if (selected_project_keys.empty()) {
        return;
    }

    if (selected_project_keys.size() > 1) {
        edit_multiple_confirmation->popup_centered_minsize();
        return;
    }

    const ProjectsListItem* selected_project =
        projects_list->get_selected_project_items()[0];
    if (selected_project->missing) {
        return;
    }

    int config_version           = selected_project->version;
    String project_folder        = selected_project->project_folder;
    String project_settings_file = project_folder.plus_file("project.rebel");
    if (config_version < ProjectSettings::CONFIG_VERSION) {
        _popup_upgrade_settings_confirmation(project_settings_file);
        return;
    }
    if (config_version > ProjectSettings::CONFIG_VERSION) {
        _popup_newer_settings_file_version_error(project_settings_file);
        return;
    }

    _open_selected_projects();
}

void ProjectsManager::_open_selected_projects() {
    projects_list->show_loading();

    Vector<ProjectsListItem*> selected_project_items =
        projects_list->get_selected_project_items();
    for (int i = 0; i < selected_project_items.size(); i++) {
        const ProjectsListItem* project = selected_project_items[i];
        String project_folder           = project->project_folder;
        String project_settings_file =
            project_folder.plus_file("project.rebel");
        if (!FileAccess::exists(project_settings_file)) {
            _popup_no_settings_file_error(project_settings_file);
            return;
        }
        String project_name = project->project_name;
        edit_project(project_name, project_folder);
    }

    _dim_window();
    get_tree()->quit();
}

void ProjectsManager::_popup_newer_settings_file_version_error(
    const String& p_file
) {
    String first_line = TTR("Can't open project settings file ");
    String second_line =
        TTR("The project settings were created with a newer version of Rebel "
            "Engine.\n");
    newer_settings_file_version_error->set_text(
        first_line + p_file + "\n" + second_line
    );
    newer_settings_file_version_error->popup_centered_minsize();
}

void ProjectsManager::_popup_no_settings_file_error(const String& p_file) {
    no_settings_file_error->set_text(
        vformat(TTR("Can't open project at '%s'."), p_file)
    );
    no_settings_file_error->popup_centered_minsize();
}

void ProjectsManager::_popup_upgrade_settings_confirmation(const String& p_file
) {
    upgrade_settings_confirmation->set_text(vformat(
        TTR("The following project settings file %s was created with an older "
            "version of Rebel Editor.\n"
            "Do you want to upgrade the settings file?\n"
            "Warning: You will not be able to open the project with the "
            "previous version of Rebel Editor again."),
        p_file
    ));
    upgrade_settings_confirmation->popup_centered_minsize();
}

void ProjectsManager::_run_selected() {
    Vector<ProjectsListItem*> selected_project_items =
        projects_list->get_selected_project_items();

    for (int i = 0; i < selected_project_items.size(); ++i) {
        const ProjectsListItem* project = selected_project_items[i];
        const String& main_scene        = project->main_scene;
        if (main_scene.empty()) {
            no_main_scene_defined_error->popup_centered();
            continue;
        }

        const String& project_folder = project->project_folder;
        const String& project_data_dir_name =
            ProjectSettings::get_singleton()->get_project_data_dir_name();
        if (!DirAccess::exists(project_folder + "/" + project_data_dir_name)) {
            no_assets_folder_error->popup_centered();
            continue;
        }

        const String& project_name = project->project_name;
        run_project(project_name, project_folder);
    }
}

void ProjectsManager::_scan_begin(const String& p_base) {
    print_line("Scanning projects at: " + p_base);
    List<String> projects;
    _scan_dir(p_base, &projects);
    print_line("Found " + itos(projects.size()) + " projects.");

    for (List<String>::Element* E = projects.front(); E; E = E->next()) {
        String project_key = E->get().replace("/", "::");
        EditorSettings::get_singleton()->set(
            "projects/" + project_key,
            E->get()
        );
        projects_list->add_project(project_key);
    }
    EditorSettings::get_singleton()->save();
}

void ProjectsManager::_scan_dir(const String& path, List<String>* r_projects) {
    DirAccessRef da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    Error error     = da->change_dir(path);
    ERR_FAIL_COND_MSG(error != OK, "Could not scan directory at: " + path);
    da->list_dir_begin();
    String n = da->get_next();
    while (!n.empty()) {
        if (da->current_is_dir() && !n.begins_with(".")) {
            _scan_dir(da->get_current_dir().plus_file(n), r_projects);
        } else if (n == "project.rebel") {
            r_projects->push_back(da->get_current_dir());
        }
        n = da->get_next();
    }
    da->list_dir_end();
}

void ProjectsManager::_scan_multiple_folders(const PoolStringArray& p_files) {
    for (int i = 0; i < p_files.size(); i++) {
        _scan_begin(p_files.get(i));
    }
}

void ProjectsManager::_show_about() {
    about->popup_centered(Size2(780, 500) * EDSCALE);
}

void ProjectsManager::_unhandled_input(const Ref<InputEvent>& p_event) {
    Ref<InputEventKey> key_event = p_event;
    if (!key_event.is_valid() || !key_event->is_pressed()) {
        return;
    }

    // Pressing Command + Q quits the Projects Manager.
    // This is handled by the platform implementation on macOS,
    // so only define the shortcut on other platforms
#ifndef MACOS_ENABLED
    if (key_event->get_scancode_with_modifiers() == (KEY_MASK_CMD | KEY_Q)) {
        _dim_window();
        get_tree()->quit();
    }
#endif

    if (tabs->get_current_tab() != 0) {
        return;
    }

    bool scancode_handled = false;
    switch (key_event->get_scancode()) {
        case KEY_ENTER: {
            _open_selected_projects_ask();
            scancode_handled = true;
        } break;
        default: {
            scancode_handled = projects_list->key_pressed(key_event);
        }
    }

    if (scancode_handled) {
        _update_project_buttons();
        accept_event();
    }
}

void ProjectsManager::_update_project_buttons() {
    Vector<ProjectsListItem*> selected_projects =
        projects_list->get_selected_project_items();
    bool empty_selection = selected_projects.empty();

    bool is_missing_project_selected = false;
    for (int i = 0; i < selected_projects.size(); ++i) {
        if (selected_projects[i]->missing) {
            is_missing_project_selected = true;
            break;
        }
    }

    remove_button->set_disabled(empty_selection);
    edit_button->set_disabled(empty_selection || is_missing_project_selected);
    rename_button->set_disabled(empty_selection || is_missing_project_selected);
    run_button->set_disabled(empty_selection || is_missing_project_selected);

    remove_missing_button->set_disabled(!projects_list->is_any_project_missing()
    );
}
