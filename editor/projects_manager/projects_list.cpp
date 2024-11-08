// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "projects_list.h"

#include "core/io/config_file.h"
#include "core/os/os.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "editor/editor_themes.h"
#include "scene/gui/label.h"
#include "scene/gui/panel_container.h"

const char* ProjectsList::SIGNAL_SELECTION_CHANGED = "selection_changed";
const char* ProjectsList::SIGNAL_PROJECT_ASK_OPEN  = "project_ask_open";

namespace {
Set<String> get_favorites(List<PropertyInfo>& properties) {
    Set<String> favorites;
    for (List<PropertyInfo>::Element* E = properties.front(); E;
         E                              = E->next()) {
        String property_key = E->get().name;
        if (property_key.begins_with("favorite_projects/")) {
            favorites.insert(property_key);
        }
    }
    return favorites;
}
} // namespace

ProjectsList::ProjectsList() {
    set_theme(create_custom_theme());
    set_h_size_flags(SIZE_EXPAND_FILL);

    // Projects List Tools
    HBoxContainer* projects_list_tools_container = memnew(HBoxContainer);
    add_child(projects_list_tools_container);

    loading_label = memnew(Label(TTR("Loading, please wait...")));
    loading_label->add_font_override("font", get_font("bold", "EditorFonts"));
    loading_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    // Hide the label until it's needed.
    loading_label->set_modulate(Color(0, 0, 0, 0));
    projects_list_tools_container->add_child(loading_label);

    Label* sort_label = memnew(Label);
    sort_label->set_text(TTR("Sort:"));
    projects_list_tools_container->add_child(sort_label);

    sort_order_options = memnew(OptionButton);
    sort_order_options->set_clip_text(true);
    sort_order_options
        ->connect("item_selected", this, "_on_sort_order_selected");
    sort_order_options->set_custom_minimum_size(Size2(180, 10) * EDSCALE);
    sort_order_options->add_item(TTR("Name"));
    sort_order_options->add_item(TTR("Path"));
    sort_order_options->add_item(TTR("Last Modified"));
    int previous_sort_order =
        EditorSettings::get_singleton()->get("project_manager/sorting_order");
    current_sort_order = (ProjectsListItem::SortOrder)previous_sort_order;
    sort_order_options->select(previous_sort_order);
    projects_list_tools_container->add_child(sort_order_options);

    search_box = memnew(LineEdit);
    search_box->set_placeholder(TTR("Filter projects"));
    search_box->set_tooltip(
        TTR("This field filters projects by name and last path component.\n"
            "To filter projects by name and full path, the query must contain "
            "at least one `/` character.")
    );
    search_box->connect("text_changed", this, "_on_search_text_changed");
    search_box->set_h_size_flags(SIZE_EXPAND_FILL);
    search_box->set_custom_minimum_size(Size2(280, 10) * EDSCALE);
    projects_list_tools_container->add_child(search_box);

    // Projects
    PanelContainer* panel_container = memnew(PanelContainer);
    panel_container->add_style_override("panel", get_stylebox("bg", "Tree"));
    panel_container->set_v_size_flags(SIZE_EXPAND_FILL);
    add_child(panel_container);

    scroll_container = memnew(ScrollContainer);
    scroll_container->set_enable_h_scroll(false);
    panel_container->add_child(scroll_container);

    projects_container = memnew(VBoxContainer);
    projects_container->set_h_size_flags(SIZE_EXPAND_FILL);
    scroll_container->add_child(projects_container);

    load_projects();
}

void ProjectsList::ensure_project_visible(int p_index) {
    scroll_container->ensure_control_visible(projects[p_index]);
}

void ProjectsList::erase_missing_projects() {
    if (projects.empty()) {
        return;
    }

    int deleted_count   = 0;
    int remaining_count = 0;

    for (int i = 0; i < projects.size(); ++i) {
        if (projects[i]->missing) {
            _remove_project(i, true);
            --i;
            ++deleted_count;

        } else {
            ++remaining_count;
        }
    }

    print_line(
        "Removed " + itos(deleted_count) + " projects from the list, remaining "
        + itos(remaining_count) + " projects"
    );

    EditorSettings::get_singleton()->save();
}

void ProjectsList::erase_selected_projects(bool p_delete_project_contents) {
    if (selected_project_keys.empty()) {
        return;
    }

    for (int i = 0; i < projects.size(); ++i) {
        ProjectsListItem* item = projects.write[i];
        if (selected_project_keys.has(item->project_key)
            && item->is_visible()) {
            EditorSettings::get_singleton()->erase(
                "projects/" + item->project_key
            );
            EditorSettings::get_singleton()->erase(
                "favorite_projects/" + item->project_key
            );

            if (p_delete_project_contents) {
                OS::get_singleton()->move_to_trash(item->project_folder);
            }

            projects.remove(i);
            memdelete(item);
            --i;
        }
    }

    EditorSettings::get_singleton()->save();

    selected_project_keys.clear();
    first_selected_project_key = "";

    update_dock_menu();
}

int ProjectsList::get_project_count() const {
    return projects.size();
}

const Set<String>& ProjectsList::get_selected_project_keys() const {
    // Faster if that's all you need
    return selected_project_keys;
}

Vector<ProjectsListItem*> ProjectsList::get_selected_projects() const {
    Vector<ProjectsListItem*> items;
    if (selected_project_keys.empty()) {
        return items;
    }

    for (int i = 0; i < projects.size(); ++i) {
        ProjectsListItem* item = projects[i];
        if (selected_project_keys.has(item->project_key)) {
            items.push_back(item);
        }
    }
    return items;
}

int ProjectsList::get_single_selected_index() const {
    if (selected_project_keys.empty()) {
        // Default selection
        return 0;
    }
    String key;
    if (selected_project_keys.size() == 1) {
        // Only one selected
        key = selected_project_keys.front()->get();
    } else {
        // Multiple selected, consider the last clicked one as "main"
        key = first_selected_project_key;
    }
    for (int i = 0; i < projects.size(); ++i) {
        if (projects[i]->project_key == key) {
            return i;
        }
    }
    return 0;
}

bool ProjectsList::is_any_project_missing() const {
    for (int i = 0; i < projects.size(); ++i) {
        if (projects[i]->missing) {
            return true;
        }
    }
    return false;
}

void ProjectsList::load_projects() {
    // This is a full, hard reload of the list. Don't call this unless really
    // required, it's expensive. If you have 150 projects, it may read through
    // 150 files on your disk at once + load 150 icons.
    _clear_projects();

    List<PropertyInfo> properties;
    EditorSettings::get_singleton()->get_property_list(&properties);

    Set<String> favorites = get_favorites(properties);

    for (List<PropertyInfo>::Element* E = properties.front(); E;
         E                              = E->next()) {
        // This is actually something like
        // "projects/C:::Documents::Projects::MyGame"
        String property_key = E->get().name;
        if (!property_key.begins_with("projects/")) {
            continue;
        }
        String project_key = property_key.get_slice("/", 1);
        bool favorite      = favorites.has("favorite_projects/" + project_key);
        _add_item(property_key, favorite);
    }

    sort_projects();

    scroll_container->set_v_scroll(0);

    _update_icons_async();

    update_dock_menu();
}

void ProjectsList::project_created(const String& dir) {
    search_box->clear();
    int i = refresh_project(dir);
    select_project(i);
    ensure_project_visible(i);
    update_dock_menu();
}

int ProjectsList::refresh_project(const String& dir_path) {
    // Reads editor settings and reloads information about a specific project.
    // If it wasn't loaded and should be in the list, it is added (i.e new
    // project). If it isn't in the list anymore, it is removed. If it is in the
    // list but doesn't exist anymore, it is marked as missing.

    String project_key = dir_path.replace("/", "::");

    // Read project manager settings
    bool is_favourite      = false;
    bool should_be_in_list = false;
    String property_key    = "projects/" + project_key;
    {
        List<PropertyInfo> properties;
        EditorSettings::get_singleton()->get_property_list(&properties);
        String favorite_property_key = "favorite_projects/" + project_key;

        bool found = false;
        for (List<PropertyInfo>::Element* E = properties.front(); E;
             E                              = E->next()) {
            String prop = E->get().name;
            if (!found && prop == property_key) {
                found = true;
            } else if (!is_favourite && prop == favorite_property_key) {
                is_favourite = true;
            }
        }

        should_be_in_list = found;
    }

    bool was_selected = selected_project_keys.has(project_key);

    // Remove item in any case
    for (int i = 0; i < projects.size(); ++i) {
        ProjectsListItem* existing_item = projects[i];
        if (existing_item->project_folder == dir_path) {
            _remove_project(i, false);
            break;
        }
    }

    int index = -1;
    if (should_be_in_list) {
        // Recreate it with updated info
        _add_item(property_key, is_favourite);

        sort_projects();

        for (int i = 0; i < projects.size(); ++i) {
            if (projects[i]->project_key == project_key) {
                if (was_selected) {
                    select_project(i);
                    ensure_project_visible(i);
                }
                _load_project_icon(i);

                index = i;
                break;
            }
        }
    }

    return index;
}

void ProjectsList::select_project(int p_index) {
    Vector<ProjectsListItem*> previous_selected_items = get_selected_projects();
    selected_project_keys.clear();

    for (int i = 0; i < previous_selected_items.size(); ++i) {
        previous_selected_items[i]->update();
    }

    _toggle_select(p_index);
}

void ProjectsList::set_search_focus() {
    search_box->grab_focus();
}

void ProjectsList::set_loading() {
    loading_label->set_modulate(Color(1, 1, 1));
}

void ProjectsList::sort_projects() {
    SortArray<ProjectsListItem*, ProjectsListItemComparator> sorter;
    sorter.compare.sort_order = current_sort_order;
    sorter.sort(projects.ptrw(), projects.size());

    String search_text = search_box->get_text().strip_edges();

    for (int i = 0; i < projects.size(); ++i) {
        ProjectsListItem* item = projects[i];

        bool visible = true;
        if (!search_text.empty()) {
            String search_path;
            if (search_text.find("/") != -1) {
                // Search path will match the whole path
                search_path = item->project_folder;
            } else {
                // Search path will only match the last path component to make
                // searching more strict
                search_path = item->project_folder.get_file();
            }

            // When searching, display projects whose name or path contain the
            // search term
            visible = item->project_name.findn(search_text) != -1
                   || search_path.findn(search_text) != -1;
        }

        item->set_visible(visible);
    }

    for (int i = 0; i < projects.size(); ++i) {
        ProjectsListItem* item = projects[i];
        item->get_parent()->move_child(item, i);
    }

    // Rewind the coroutine because order of projects changed
    _update_icons_async();

    update_dock_menu();
}

void ProjectsList::update_dock_menu() {
    OS::get_singleton()->global_menu_clear("_dock");

    int favs_added  = 0;
    int total_added = 0;
    for (int i = 0; i < projects.size(); ++i) {
        if (!projects[i]->disabled && !projects[i]->missing) {
            if (projects[i]->favorite) {
                favs_added++;
            } else {
                if (favs_added != 0) {
                    OS::get_singleton()->global_menu_add_separator("_dock");
                }
                favs_added = 0;
            }
            OS::get_singleton()->global_menu_add_item(
                "_dock",
                projects[i]->project_name + " ( " + projects[i]->project_folder
                    + " )",
                GLOBAL_OPEN_PROJECT,
                Variant(projects[i]->project_folder.plus_file("project.rebel"))
            );
            total_added++;
        }
    }
    if (total_added != 0) {
        OS::get_singleton()->global_menu_add_separator("_dock");
    }
    OS::get_singleton()->global_menu_add_item(
        "_dock",
        TTR("New Window"),
        GLOBAL_NEW_WINDOW,
        Variant()
    );
}

void ProjectsList::_bind_methods() {
    ClassDB::bind_method(
        "_on_item_double_clicked",
        &ProjectsList::_on_item_double_clicked
    );
    ClassDB::bind_method("_on_item_updated", &ProjectsList::_on_item_updated);
    ClassDB::bind_method(
        "_on_search_text_changed",
        &ProjectsList::_on_search_text_changed
    );
    ClassDB::bind_method(
        "_on_selection_changed",
        &ProjectsList::_on_selection_changed
    );
    ClassDB::bind_method(
        "_on_sort_order_selected",
        &ProjectsList::_on_sort_order_selected
    );

    ADD_SIGNAL(MethodInfo(SIGNAL_SELECTION_CHANGED));
    ADD_SIGNAL(MethodInfo(SIGNAL_PROJECT_ASK_OPEN));
}

void ProjectsList::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_PROCESS: {
            // Load icons as a co-routine to speed up launch
            // when there are many projects.
            if (icon_load_index < projects.size()) {
                const ProjectsListItem* item = projects[icon_load_index];
                if (item->icon_needs_reload) {
                    _load_project_icon(icon_load_index);
                }
                icon_load_index++;

            } else {
                set_process(false);
            }
        } break;

        case NOTIFICATION_READY: {
            if (get_project_count() >= 1) {
                // Focus on the search box immediately to allow the user
                // to search without having to reach for their mouse
                search_box->grab_focus();
            }
        } break;
    }
}

void ProjectsList::_add_item(const String& property_key, bool favorite) {
    ProjectsListItem* item = memnew(ProjectsListItem(property_key, favorite));
    item->connect("item_double_clicked", this, "_on_item_double_clicked");
    item->connect("item_updated", this, "_on_item_updated", varray(item));
    item->connect(
        "selection_changed",
        this,
        "_on_selection_changed",
        varray(item)
    );
    item->connect("gui_input", item, "_on_gui_input");
    projects_container->add_child(item);
    projects.push_back(item);
}

void ProjectsList::_clear_projects() {
    for (int i = 0; i < projects.size(); ++i) {
        ProjectsListItem* project = projects[i];
        memdelete(project);
    }
    projects.clear();
    selected_project_keys.clear();
    first_selected_project_key = "";
}

void ProjectsList::_clear_selection() {
    selected_project_keys.clear();
    for (int index = 0; index < projects.size(); index++) {
        projects[index]->selected = false;
        projects[index]->update();
    }
}

void ProjectsList::_load_project_icon(int p_index) {
    ProjectsListItem* item = projects[p_index];

    Ref<Texture> default_icon = get_icon("DefaultProjectIcon", "EditorIcons");
    Ref<Texture> icon;
    if (!item->icon_path.empty()) {
        Ref<Image> img;
        img.instance();
        Error err = img->load(
            item->icon_path.replace_first("res://", item->project_folder + "/")
        );
        if (err == OK) {
            img->resize(
                default_icon->get_width(),
                default_icon->get_height(),
                Image::INTERPOLATE_LANCZOS
            );
            Ref<ImageTexture> it = memnew(ImageTexture);
            it->create_from_image(img);
            icon = it;
        }
    }
    if (icon.is_null()) {
        icon = default_icon;
    }

    item->set_icon_texture(icon);
    item->icon_needs_reload = false;
}

void ProjectsList::_on_item_double_clicked() {
    emit_signal(SIGNAL_PROJECT_ASK_OPEN);
}

void ProjectsList::_on_search_text_changed(const String& p_newtext) {
    sort_projects();
}

void ProjectsList::_on_selection_changed(
    bool p_shift_pressed,
    bool p_control_pressed,
    Node* p_node
) {
    ProjectsListItem* item    = Object::cast_to<ProjectsListItem>(p_node);
    const String& project_key = item->project_key;

    if (p_shift_pressed) {
        _clear_selection();
        _select_range(item);
    } else if (p_control_pressed) {
        // Toggle item selection.
        item->selected = !item->selected;
        if (item->selected) {
            item->selected = true;
            selected_project_keys.insert(project_key);
        } else {
            item->selected = false;
            selected_project_keys.erase(project_key);
        }
        // We need to update the item, because we're not clearing the selection,
        // which calls update on all the items.
        item->update();
    } else {
        _clear_selection();
        _select_item(item);
        first_selected_project_key = project_key;
    }

    emit_signal(SIGNAL_SELECTION_CHANGED);
}

void ProjectsList::_on_sort_order_selected(int p_index) {
    ProjectsListItem::SortOrder selected_sort_order =
        (ProjectsListItem::SortOrder)(p_index);
    if (current_sort_order == selected_sort_order) {
        return;
    }
    EditorSettings* editor_settings = EditorSettings::get_singleton();
    editor_settings->set("project_manager/sorting_order", p_index);
    editor_settings->save();
    current_sort_order = selected_sort_order;
    sort_projects();
}

void ProjectsList::_on_item_updated(const Node* p_node) {
    sort_projects();

    const ProjectsListItem* item = Object::cast_to<ProjectsListItem>(p_node);
    if (item->favorite) {
        for (int i = 0; i < projects.size(); ++i) {
            if (projects[i]->project_key == item->project_key) {
                ensure_project_visible(i);
                break;
            }
        }
    }

    update_dock_menu();
}

void ProjectsList::_remove_project(int p_index, bool p_update_settings) {
    ProjectsListItem* item = projects[p_index];

    selected_project_keys.erase(item->project_key);

    if (first_selected_project_key == item->project_key) {
        first_selected_project_key = "";
    }

    if (p_update_settings) {
        EditorSettings::get_singleton()->erase("projects/" + item->project_key);
        EditorSettings::get_singleton()->erase(
            "favorite_projects/" + item->project_key
        );
        // Don't save the file, because there may be more changes.
    }

    projects.remove(p_index);
    memdelete(item);

    update_dock_menu();
}

void ProjectsList::_select_item(ProjectsListItem* p_item) {
    p_item->selected = true;
    selected_project_keys.insert(p_item->project_key);
}

void ProjectsList::_select_range(ProjectsListItem* p_to_item) {
    const String& last_selected_project_key = p_to_item->project_key;

    bool select = false;
    for (int index = 0; index < projects.size(); index++) {
        ProjectsListItem* item = projects[index];
        if (item->project_key == first_selected_project_key) {
            _select_item(item);
            select = !select;
        }
        if (item->project_key == last_selected_project_key) {
            _select_item(item);
            select = !select;
        }
        if (select) {
            _select_item(item);
        }
    }
}

void ProjectsList::_toggle_select(int p_index) {
    ProjectsListItem* item = projects[p_index];
    if (selected_project_keys.has(item->project_key)) {
        selected_project_keys.erase(item->project_key);
    } else {
        selected_project_keys.insert(item->project_key);
    }
    item->update();
}

void ProjectsList::_update_icons_async() {
    icon_load_index = 0;
    set_process(true);
}
