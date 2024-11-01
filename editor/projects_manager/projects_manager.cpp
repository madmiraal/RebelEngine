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
}
} // namespace

ProjectsManager::ProjectsManager() {
    apply_editor_settings();
    apply_window_settings();

    set_anchors_and_margins_preset(Control::PRESET_WIDE);
    set_theme(create_custom_theme());

    Panel* panel = memnew(Panel);
    add_child(panel);
    panel->set_anchors_and_margins_preset(Control::PRESET_WIDE);
    panel->add_style_override(
        "panel",
        get_stylebox("Background", "EditorStyles")
    );

    VBoxContainer* panel_container = memnew(VBoxContainer);
    panel->add_child(panel_container);
    panel_container->set_anchors_and_margins_preset(
        Control::PRESET_WIDE,
        Control::PRESET_MODE_MINSIZE,
        8 * EDSCALE
    );

    Control* center_box = memnew(Control);
    center_box->set_v_size_flags(SIZE_EXPAND_FILL);
    panel_container->add_child(center_box);

    tabs = memnew(TabContainer);
    tabs->set_anchors_and_margins_preset(Control::PRESET_WIDE);
    tabs->set_tab_align(TabContainer::ALIGN_LEFT);
    tabs->connect("tab_changed", this, "_on_tab_changed");
    center_box->add_child(tabs);

    HBoxContainer* projects_tab_container = memnew(HBoxContainer);
    projects_tab_container->set_name(TTR("Local Projects"));
    tabs->add_child(projects_tab_container);

    projects_list = memnew(ProjectsList);
    projects_list->connect(
        ProjectsList::SIGNAL_SELECTION_CHANGED,
        this,
        "_update_project_buttons"
    );
    projects_list->connect(
        ProjectsList::SIGNAL_PROJECT_ASK_OPEN,
        this,
        "_open_selected_projects_ask"
    );
    projects_tab_container->add_child(projects_list);

    VBoxContainer* tree_vb = memnew(VBoxContainer);
    tree_vb->set_custom_minimum_size(Size2(120, 120));
    projects_tab_container->add_child(tree_vb);

    Button* open = memnew(Button);
    open->set_text(TTR("Edit"));
    open->set_shortcut(ED_SHORTCUT(
        "project_manager/edit_project",
        TTR("Edit Project"),
        KEY_MASK_CMD | KEY_E
    ));
    tree_vb->add_child(open);
    open->connect("pressed", this, "_open_selected_projects_ask");
    open_btn = open;

    Button* run = memnew(Button);
    run->set_text(TTR("Run"));
    run->set_shortcut(ED_SHORTCUT(
        "project_manager/run_project",
        TTR("Run Project"),
        KEY_MASK_CMD | KEY_R
    ));
    tree_vb->add_child(run);
    run->connect("pressed", this, "_run_project");
    run_btn = run;

    tree_vb->add_child(memnew(HSeparator));

    Button* scan = memnew(Button);
    scan->set_text(TTR("Scan"));
    scan->set_shortcut(ED_SHORTCUT(
        "project_manager/scan_projects",
        TTR("Scan Projects"),
        KEY_MASK_CMD | KEY_S
    ));
    tree_vb->add_child(scan);
    scan->connect("pressed", this, "_scan_projects");

    tree_vb->add_child(memnew(HSeparator));

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

    Button* create = memnew(Button);
    create->set_text(TTR("New Project"));
    create->set_shortcut(ED_SHORTCUT(
        "project_manager/new_project",
        TTR("New Project"),
        KEY_MASK_CMD | KEY_N
    ));
    tree_vb->add_child(create);
    create->connect("pressed", this, "_new_project");

    Button* import = memnew(Button);
    import->set_text(TTR("Import"));
    import->set_shortcut(ED_SHORTCUT(
        "project_manager/import_project",
        TTR("Import exsiting project"),
        KEY_MASK_CMD | KEY_I
    ));
    tree_vb->add_child(import);
    import->connect("pressed", this, "_import_project");

    Button* rename = memnew(Button);
    rename->set_text(TTR("Rename"));
    rename->set_shortcut(ED_SHORTCUT(
        "project_manager/rename_project",
        TTR("Rename Project"),
        KEY_F2
    ));
    tree_vb->add_child(rename);
    rename->connect("pressed", this, "_rename_project");
    rename_btn = rename;

    Button* erase = memnew(Button);
    erase->set_text(TTR("Remove"));
    erase->set_shortcut(ED_SHORTCUT(
        "project_manager/remove_project",
        TTR("Remove Project"),
        KEY_DELETE
    ));
    tree_vb->add_child(erase);
    erase->connect("pressed", this, "_erase_project");
    erase_btn = erase;

    Button* erase_missing = memnew(Button);
    erase_missing->set_text(TTR("Remove Missing"));
    tree_vb->add_child(erase_missing);
    erase_missing->connect("pressed", this, "_erase_missing_projects");
    erase_missing_btn = erase_missing;

    tree_vb->add_spacer();

    about_btn = memnew(Button);
    about_btn->set_text(TTR("About"));
    about_btn->connect("pressed", this, "_show_about");
    tree_vb->add_child(about_btn);

    if (StreamPeerSSL::is_available()) {
        asset_library = memnew(EditorAssetLibrary(true));
        asset_library->set_name(TTR("Asset Library Projects"));
        tabs->add_child(asset_library);
        asset_library->connect("install_asset", this, "_install_project");
    } else {
        WARN_PRINT("Asset Library not available, as it requires SSL to work.");
    }

    HBoxContainer* settings_hb = memnew(HBoxContainer);
    settings_hb->set_alignment(BoxContainer::ALIGN_END);
    settings_hb->set_h_grow_direction(Control::GROW_DIRECTION_BEGIN);

    // A VBoxContainer that contains a dummy Control node to adjust the
    // LinkButton's vertical position.
    VBoxContainer* spacer_vb = memnew(VBoxContainer);
    settings_hb->add_child(spacer_vb);

    Control* v_spacer = memnew(Control);
    spacer_vb->add_child(v_spacer);

    version_btn = memnew(LinkButton);
    String hash = String(VERSION_HASH);
    if (!hash.empty()) {
        hash = " " + vformat("[%s]", hash.left(9));
    }
    version_btn->set_text("v" VERSION_FULL_BUILD + hash);
    // Fade the version label to be less prominent, but still readable.
    version_btn->set_self_modulate(Color(1, 1, 1, 0.6));
    version_btn->set_underline_mode(LinkButton::UNDERLINE_MODE_ON_HOVER);
    version_btn->set_tooltip(TTR("Click to copy."));
    version_btn->connect("pressed", this, "_version_button_pressed");
    spacer_vb->add_child(version_btn);

    // Add a small horizontal spacer between the version and language buttons
    // to distinguish them.
    Control* h_spacer = memnew(Control);
    settings_hb->add_child(h_spacer);

    language_btn = memnew(OptionButton);
    language_btn->set_flat(true);
    language_btn->set_focus_mode(Control::FOCUS_NONE);

    Vector<String> editor_languages;
    List<PropertyInfo> editor_settings_properties;
    EditorSettings::get_singleton()->get_property_list(
        &editor_settings_properties
    );
    for (List<PropertyInfo>::Element* E = editor_settings_properties.front(); E;
         E                              = E->next()) {
        PropertyInfo& pi = E->get();
        if (pi.name == "interface/editor/editor_language") {
            editor_languages = pi.hint_string.split(",");
        }
    }
    String current_lang =
        EditorSettings::get_singleton()->get("interface/editor/editor_language"
        );
    for (int i = 0; i < editor_languages.size(); i++) {
        const String& lang = editor_languages[i];
        String lang_name =
            TranslationServer::get_singleton()->get_locale_name(lang);
        language_btn->add_item(lang_name + " [" + lang + "]", i);
        language_btn->set_item_metadata(i, lang);
        if (current_lang == lang) {
            language_btn->select(i);
            language_btn->set_text(lang);
        }
    }
    language_btn->set_icon(get_icon("Environment", "EditorIcons"));

    settings_hb->add_child(language_btn);
    language_btn->connect("item_selected", this, "_language_selected");

    center_box->add_child(settings_hb);
    settings_hb->set_anchors_and_margins_preset(Control::PRESET_TOP_RIGHT);

    //////////////////////////////////////////////////////////////

    language_restart_ask = memnew(ConfirmationDialog);
    language_restart_ask->get_ok()->set_text(TTR("Restart Now"));
    language_restart_ask->get_ok()
        ->connect("pressed", this, "_restart_confirm");
    language_restart_ask->get_cancel()->set_text(TTR("Continue"));
    add_child(language_restart_ask);

    erase_missing_ask = memnew(ConfirmationDialog);
    erase_missing_ask->get_ok()->set_text(TTR("Remove All"));
    erase_missing_ask->get_ok()
        ->connect("pressed", this, "_erase_missing_projects_confirm");
    add_child(erase_missing_ask);

    erase_ask = memnew(ConfirmationDialog);
    erase_ask->get_ok()->set_text(TTR("Remove"));
    erase_ask->get_ok()->connect("pressed", this, "_erase_project_confirm");
    add_child(erase_ask);

    VBoxContainer* erase_ask_vb = memnew(VBoxContainer);
    erase_ask->add_child(erase_ask_vb);

    erase_ask_label = memnew(Label);
    erase_ask_vb->add_child(erase_ask_label);

    delete_project_contents = memnew(CheckBox);
    delete_project_contents->set_text(
        TTR("Also delete project contents (no undo!)")
    );
    erase_ask_vb->add_child(delete_project_contents);

    multi_open_ask = memnew(ConfirmationDialog);
    multi_open_ask->get_ok()->set_text(TTR("Edit"));
    multi_open_ask->get_ok()
        ->connect("pressed", this, "_open_selected_projects");
    add_child(multi_open_ask);

    multi_run_ask = memnew(ConfirmationDialog);
    multi_run_ask->get_ok()->set_text(TTR("Run"));
    multi_run_ask->get_ok()->connect("pressed", this, "_run_project_confirm");
    add_child(multi_run_ask);

    multi_scan_ask = memnew(ConfirmationDialog);
    multi_scan_ask->get_ok()->set_text(TTR("Scan"));
    add_child(multi_scan_ask);

    ask_update_settings = memnew(ConfirmationDialog);
    ask_update_settings->get_ok()
        ->connect("pressed", this, "_confirm_update_settings");
    add_child(ask_update_settings);

    OS::get_singleton()->set_low_processor_usage_mode(true);

    npdialog = memnew(ProjectsDialog);
    add_child(npdialog);

    npdialog->connect("projects_updated", this, "_on_projects_updated");
    npdialog->connect("project_created", this, "_on_project_created");

    _update_project_buttons();
    tabs->set_current_tab(0);

    DirAccessRef dir_access =
        DirAccess::create(DirAccess::AccessType::ACCESS_FILESYSTEM);

    String default_project_path = EditorSettings::get_singleton()->get(
        "filesystem/directories/default_project_path"
    );
    if (!dir_access->dir_exists(default_project_path)) {
        Error error = dir_access->make_dir_recursive(default_project_path);
        if (error != OK) {
            ERR_PRINT(
                "Could not create default project directory at: "
                + default_project_path
            );
        }
    }

    String autoscan_path = EditorSettings::get_singleton()->get(
        "filesystem/directories/autoscan_project_path"
    );
    if (!autoscan_path.empty()) {
        if (dir_access->dir_exists(autoscan_path)) {
            _scan_begin(autoscan_path);
        } else {
            Error error = dir_access->make_dir_recursive(autoscan_path);
            if (error != OK) {
                ERR_PRINT(
                    "Could not create project autoscan directory at: "
                    + autoscan_path
                );
            }
        }
    }

    SceneTree::get_singleton()
        ->connect("files_dropped", this, "_files_dropped");
    SceneTree::get_singleton()
        ->connect("global_menu_action", this, "_global_menu_action");

    run_error_diag = memnew(AcceptDialog);
    add_child(run_error_diag);
    run_error_diag->set_title(TTR("Can't run project"));

    dialog_error = memnew(AcceptDialog);
    add_child(dialog_error);

    open_templates = memnew(ConfirmationDialog);
    open_templates->set_text(
        TTR("You currently don't have any projects.\nWould you like to explore "
            "official example projects in the Asset Library?")
    );
    open_templates->get_ok()->set_text(TTR("Open Asset Library"));
    open_templates->connect("confirmed", this, "_open_asset_library");
    add_child(open_templates);

    about = memnew(EditorAbout);
    add_child(about);
}

ProjectsManager::~ProjectsManager() {
    if (EditorSettings::get_singleton()) {
        EditorSettings::destroy();
    }
}

void ProjectsManager::_bind_methods() {
    ClassDB::bind_method(
        "_open_selected_projects_ask",
        &ProjectsManager::_open_selected_projects_ask
    );
    ClassDB::bind_method(
        "_open_selected_projects",
        &ProjectsManager::_open_selected_projects
    );
    ClassDB::bind_method(
        D_METHOD("_global_menu_action"),
        &ProjectsManager::_global_menu_action,
        DEFVAL(Variant())
    );
    ClassDB::bind_method("_run_project", &ProjectsManager::_run_project);
    ClassDB::bind_method(
        "_run_project_confirm",
        &ProjectsManager::_run_project_confirm
    );
    ClassDB::bind_method("_scan_projects", &ProjectsManager::_scan_projects);
    ClassDB::bind_method("_scan_begin", &ProjectsManager::_scan_begin);
    ClassDB::bind_method("_import_project", &ProjectsManager::_import_project);
    ClassDB::bind_method("_new_project", &ProjectsManager::_new_project);
    ClassDB::bind_method("_rename_project", &ProjectsManager::_rename_project);
    ClassDB::bind_method("_erase_project", &ProjectsManager::_erase_project);
    ClassDB::bind_method(
        "_erase_missing_projects",
        &ProjectsManager::_erase_missing_projects
    );
    ClassDB::bind_method(
        "_erase_project_confirm",
        &ProjectsManager::_erase_project_confirm
    );
    ClassDB::bind_method(
        "_erase_missing_projects_confirm",
        &ProjectsManager::_erase_missing_projects_confirm
    );
    ClassDB::bind_method("_show_about", &ProjectsManager::_show_about);
    ClassDB::bind_method(
        "_version_button_pressed",
        &ProjectsManager::_version_button_pressed
    );
    ClassDB::bind_method(
        "_language_selected",
        &ProjectsManager::_language_selected
    );
    ClassDB::bind_method(
        "_restart_confirm",
        &ProjectsManager::_restart_confirm
    );
    ClassDB::bind_method("_on_tab_changed", &ProjectsManager::_on_tab_changed);
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
    ClassDB::bind_method(
        "_open_asset_library",
        &ProjectsManager::_open_asset_library
    );
    ClassDB::bind_method(
        "_confirm_update_settings",
        &ProjectsManager::_confirm_update_settings
    );
    ClassDB::bind_method(
        "_update_project_buttons",
        &ProjectsManager::_update_project_buttons
    );
    ClassDB::bind_method(
        D_METHOD("_scan_multiple_folders", "files"),
        &ProjectsManager::_scan_multiple_folders
    );
}

void ProjectsManager::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            Engine::get_singleton()->set_editor_hint(false);
        } break;
        case NOTIFICATION_RESIZED: {
            if (open_templates->is_visible()) {
                open_templates->popup_centered_minsize();
            }
        } break;
        case NOTIFICATION_READY: {
            if (projects_list->get_project_count() == 0
                && StreamPeerSSL::is_available()) {
                open_templates->popup_centered_minsize();
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

void ProjectsManager::_confirm_update_settings() {
    _open_selected_projects();
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

void ProjectsManager::_erase_missing_projects() {
    erase_missing_ask->set_text(
        TTR("Remove all missing projects from the list?\n"
            "The project folders' contents won't be modified.")
    );
    erase_missing_ask->popup_centered_minsize();
}

void ProjectsManager::_erase_missing_projects_confirm() {
    projects_list->erase_missing_projects();
    _update_project_buttons();
}

void ProjectsManager::_erase_project() {
    const Set<String>& selected_list =
        projects_list->get_selected_project_keys();

    if (selected_list.empty()) {
        return;
    }

    String confirm_message;
    if (selected_list.size() >= 2) {
        confirm_message = vformat(
            TTR("Remove %d projects from the list?"),
            selected_list.size()
        );
    } else {
        confirm_message = TTR("Remove this project from the list?");
    }

    erase_ask_label->set_text(confirm_message);
    delete_project_contents->set_pressed(false);
    erase_ask->popup_centered_minsize();
}

void ProjectsManager::_erase_project_confirm() {
    projects_list->erase_selected_projects(delete_project_contents->is_pressed()
    );
    _update_project_buttons();
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
            multi_scan_ask->get_ok()
                ->disconnect("pressed", this, "_scan_multiple_folders");
            multi_scan_ask->get_ok()->connect(
                "pressed",
                this,
                "_scan_multiple_folders",
                varray(folders)
            );
            multi_scan_ask->set_text(vformat(
                TTR("Are you sure to scan %s folders for existing Rebel "
                    "projects?\n"
                    "This could take a while."),
                folders.size()
            ));
            multi_scan_ask->popup_centered_minsize();
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

void ProjectsManager::_import_project() {
    npdialog->set_mode(ProjectsDialog::MODE_IMPORT);
    npdialog->show_dialog();
}

void ProjectsManager::_install_project(
    const String& p_zip_path,
    const String& p_title
) {
    npdialog->set_mode(ProjectsDialog::MODE_INSTALL);
    npdialog->set_zip_path(p_zip_path);
    npdialog->set_zip_title(p_title);
    npdialog->show_dialog();
}

void ProjectsManager::_language_selected(int p_id) {
    String lang = language_btn->get_item_metadata(p_id);
    EditorSettings::get_singleton()->set(
        "interface/editor/editor_language",
        lang
    );
    language_btn->set_text(lang);
    language_btn->set_icon(get_icon("Environment", "EditorIcons"));

    language_restart_ask->set_text(
        TTR("Language changed.\n"
            "The interface will update after restarting Rebel Editor or "
            "Projects Manager")
    );
    language_restart_ask->popup_centered();
}

void ProjectsManager::_new_project() {
    npdialog->set_mode(ProjectsDialog::MODE_NEW);
    npdialog->show_dialog();
}

void ProjectsManager::_on_project_created(const String& dir) {
    projects_list->project_created(dir);
    _open_selected_projects_ask();
}

void ProjectsManager::_on_projects_updated() {
    Vector<ProjectsListItem*> selected_projects =
        projects_list->get_selected_projects();
    int index = 0;
    for (int i = 0; i < selected_projects.size(); ++i) {
        index = projects_list->refresh_project(selected_projects[i]->path);
    }
    if (index != -1) {
        projects_list->ensure_project_visible(index);
    }

    projects_list->update_dock_menu();
}

void ProjectsManager::_on_tab_changed(int p_tab) {
    if (p_tab == 0) { // Projects
        projects_list->set_search_focus();
    }

    // The Templates tab's search field is focused on display in the asset
    // library editor plugin code.
}

void ProjectsManager::_open_asset_library() {
    asset_library->disable_community_support();
    tabs->set_current_tab(1);
}

void ProjectsManager::_open_selected_projects_ask() {
    const Set<String>& selected_list =
        projects_list->get_selected_project_keys();

    if (selected_list.size() < 1) {
        return;
    }

    if (selected_list.size() > 1) {
        multi_open_ask->set_text(
            TTR("Are you sure to open more than one project?")
        );
        multi_open_ask->popup_centered_minsize();
        return;
    }

    const ProjectsListItem* project = projects_list->get_selected_projects()[0];
    if (project->missing) {
        return;
    }

    // Update the project settings or don't open
    String conf        = project->path.plus_file("project.rebel");
    int config_version = project->version;

    // Check if the config_version property was empty or 0
    if (config_version == 0) {
        ask_update_settings->set_text(vformat(
            TTR("The following project settings file does not specify the "
                "version of Rebel used to create it.\n\n%s\n\n"
                "If you proceed with opening it, it will be converted to "
                "Rebel's current configuration file format.\n"
                "Warning: You won't be able to open the project with previous "
                "versions of the engine anymore."),
            conf
        ));
        ask_update_settings->popup_centered_minsize();
        return;
    }
    // Check if we need to convert project settings from an earlier engine
    // version
    if (config_version < ProjectSettings::CONFIG_VERSION) {
        ask_update_settings->set_text(vformat(
            TTR("The following project settings file was generated by an older "
                "engine version, and needs to be converted for this "
                "version:\n\n%s\n\nDo you want to convert it?\nWarning: You "
                "won't be able to open the project with previous versions of "
                "the engine anymore."),
            conf
        ));
        ask_update_settings->popup_centered_minsize();
        return;
    }
    // Check if the file was generated by a newer, incompatible engine version
    if (config_version > ProjectSettings::CONFIG_VERSION) {
        dialog_error->set_text(vformat(
            TTR("Can't open project at '%s'.") + "\n"
                + TTR("The project settings were created by a newer engine "
                      "version, whose settings are not compatible with this "
                      "version."),
            project->path
        ));
        dialog_error->popup_centered_minsize();
        return;
    }

    // Open if the project is up-to-date
    _open_selected_projects();
}

void ProjectsManager::_open_selected_projects() {
    // Show loading text to tell the user that the Projects Manager is busy
    // loading. This is especially important for the Web Projects Manager.
    projects_list->set_loading();

    const Set<String>& selected_list =
        projects_list->get_selected_project_keys();

    for (const Set<String>::Element* E = selected_list.front(); E;
         E                             = E->next()) {
        const String& selected = E->get();
        String path =
            EditorSettings::get_singleton()->get("projects/" + selected);
        String conf = path.plus_file("project.rebel");

        if (!FileAccess::exists(conf)) {
            dialog_error->set_text(
                vformat(TTR("Can't open project at '%s'."), path)
            );
            dialog_error->popup_centered_minsize();
            return;
        }

        print_line("Editing project: " + path + " (" + selected + ")");

        List<String> args;

        args.push_back("--path");
        args.push_back(path);

        args.push_back("--editor");

        if (OS::get_singleton()->is_stdout_debug_enabled()) {
            args.push_back("--debug");
        }

        if (OS::get_singleton()->is_stdout_verbose()) {
            args.push_back("--verbose");
        }

        if (OS::get_singleton()->is_disable_crash_handler()) {
            args.push_back("--disable-crash-handler");
        }

        String exec = OS::get_singleton()->get_executable_path();

        OS::ProcessID pid = 0;
        Error err = OS::get_singleton()->execute(exec, args, false, &pid);
        ERR_FAIL_COND(err);
    }

    _dim_window();
    get_tree()->quit();
}

void ProjectsManager::_rename_project() {
    const Set<String>& selected_list =
        projects_list->get_selected_project_keys();

    if (selected_list.empty()) {
        return;
    }

    for (Set<String>::Element* E = selected_list.front(); E; E = E->next()) {
        const String& selected = E->get();
        String path =
            EditorSettings::get_singleton()->get("projects/" + selected);
        npdialog->set_project_path(path);
        npdialog->set_mode(ProjectsDialog::MODE_RENAME);
        npdialog->show_dialog();
    }
}

void ProjectsManager::_restart_confirm() {
    List<String> args = OS::get_singleton()->get_cmdline_args();
    String exec       = OS::get_singleton()->get_executable_path();
    OS::ProcessID pid = 0;
    Error err         = OS::get_singleton()->execute(exec, args, false, &pid);
    ERR_FAIL_COND(err);

    _dim_window();
    get_tree()->quit();
}

void ProjectsManager::_run_project_confirm() {
    Vector<ProjectsListItem*> selected_list =
        projects_list->get_selected_projects();

    for (int i = 0; i < selected_list.size(); ++i) {
        const String& selected_main = selected_list[i]->main_scene;
        if (selected_main.empty()) {
            run_error_diag->set_text(
                TTR("Can't run project: no main scene defined.\nPlease edit "
                    "the project and set the main scene in the Project "
                    "Settings under the \"Application\" category.")
            );
            run_error_diag->popup_centered();
            continue;
        }

        const String& selected = selected_list[i]->project_key;
        String path =
            EditorSettings::get_singleton()->get("projects/" + selected);

        String project_data_dir_name =
            ProjectSettings::get_singleton()->get_project_data_dir_name();
        if (!DirAccess::exists(path + "/" + project_data_dir_name)) {
            run_error_diag->set_text(
                TTR("Can't run project: Assets need to be imported.\n"
                    "Please edit the project to trigger the initial import.")
            );
            run_error_diag->popup_centered();
            continue;
        }

        print_line("Running project: " + path + " (" + selected + ")");

        List<String> args;

        args.push_back("--path");
        args.push_back(path);

        if (OS::get_singleton()->is_disable_crash_handler()) {
            args.push_back("--disable-crash-handler");
        }

        String exec = OS::get_singleton()->get_executable_path();

        OS::ProcessID pid = 0;
        Error err = OS::get_singleton()->execute(exec, args, false, &pid);
        ERR_FAIL_COND(err);
    }
}

// When you press the "Run" button
void ProjectsManager::_run_project() {
    const Set<String>& selected_list =
        projects_list->get_selected_project_keys();

    if (selected_list.size() < 1) {
        return;
    }

    if (selected_list.size() > 1) {
        multi_run_ask->set_text(vformat(
            TTR("Are you sure to run %d projects at once?"),
            selected_list.size()
        ));
        multi_run_ask->popup_centered_minsize();
    } else {
        _run_project_confirm();
    }
}

void ProjectsManager::_scan_begin(const String& p_base) {
    print_line("Scanning projects at: " + p_base);
    List<String> projects;
    _scan_dir(p_base, &projects);
    print_line("Found " + itos(projects.size()) + " projects.");

    for (List<String>::Element* E = projects.front(); E; E = E->next()) {
        String proj = E->get().replace("/", "::");
        EditorSettings::get_singleton()->set("projects/" + proj, E->get());
    }
    EditorSettings::get_singleton()->save();
    projects_list->load_projects();
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

void ProjectsManager::_scan_projects() {
    scan_dir->popup_centered_ratio();
}

void ProjectsManager::_show_about() {
    about->popup_centered(Size2(780, 500) * EDSCALE);
}

void ProjectsManager::_unhandled_input(const Ref<InputEvent>& p_ev) {
    Ref<InputEventKey> k = p_ev;

    if (k.is_valid()) {
        if (!k->is_pressed()) {
            return;
        }

        // Pressing Command + Q quits the Projects Manager
        // This is handled by the platform implementation on macOS,
        // so only define the shortcut on other platforms
#ifndef MACOS_ENABLED
        if (k->get_scancode_with_modifiers() == (KEY_MASK_CMD | KEY_Q)) {
            _dim_window();
            get_tree()->quit();
        }
#endif

        if (tabs->get_current_tab() != 0) {
            return;
        }

        bool scancode_handled = true;

        switch (k->get_scancode()) {
            case KEY_ENTER: {
                _open_selected_projects_ask();
            } break;
            case KEY_HOME: {
                if (projects_list->get_project_count() > 0) {
                    projects_list->select_project(0);
                    _update_project_buttons();
                }

            } break;
            case KEY_END: {
                if (projects_list->get_project_count() > 0) {
                    projects_list->select_project(
                        projects_list->get_project_count() - 1
                    );
                    _update_project_buttons();
                }

            } break;
            case KEY_UP: {
                if (k->get_shift()) {
                    break;
                }

                int index = projects_list->get_single_selected_index();
                if (index > 0) {
                    projects_list->select_project(index - 1);
                    projects_list->ensure_project_visible(index - 1);
                    _update_project_buttons();
                }

                break;
            }
            case KEY_DOWN: {
                if (k->get_shift()) {
                    break;
                }

                int index = projects_list->get_single_selected_index();
                if (index + 1 < projects_list->get_project_count()) {
                    projects_list->select_project(index + 1);
                    projects_list->ensure_project_visible(index + 1);
                    _update_project_buttons();
                }

            } break;
            case KEY_F: {
                if (k->get_command()) {
                    projects_list->set_search_focus();
                } else {
                    scancode_handled = false;
                }
            } break;
            default: {
                scancode_handled = false;
            } break;
        }

        if (scancode_handled) {
            accept_event();
        }
    }
}

void ProjectsManager::_update_project_buttons() {
    Vector<ProjectsListItem*> selected_projects =
        projects_list->get_selected_projects();
    bool empty_selection = selected_projects.empty();

    bool is_missing_project_selected = false;
    for (int i = 0; i < selected_projects.size(); ++i) {
        if (selected_projects[i]->missing) {
            is_missing_project_selected = true;
            break;
        }
    }

    erase_btn->set_disabled(empty_selection);
    open_btn->set_disabled(empty_selection || is_missing_project_selected);
    rename_btn->set_disabled(empty_selection || is_missing_project_selected);
    run_btn->set_disabled(empty_selection || is_missing_project_selected);

    erase_missing_btn->set_disabled(!projects_list->is_any_project_missing());
}

void ProjectsManager::_version_button_pressed() {
    OS::get_singleton()->set_clipboard(version_btn->get_text());
}
