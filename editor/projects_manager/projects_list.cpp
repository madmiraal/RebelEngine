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
    last_selected_project_key = "";

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
        key = last_selected_project_key;
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

    // Clear whole list
    for (int i = 0; i < projects.size(); ++i) {
        ProjectsListItem* project = projects[i];
        memdelete(project);
    }
    projects.clear();
    last_selected_project_key = "";
    selected_project_keys.clear();

    // Load data
    // TODO Would be nice to change how projects and favourites are stored... it
    // complicates things a bit. Use a dictionary associating project path to
    // metadata (like is_favorite).

    List<PropertyInfo> properties;
    EditorSettings::get_singleton()->get_property_list(&properties);

    Set<String> favorites;
    // Find favourites...
    for (List<PropertyInfo>::Element* E = properties.front(); E;
         E                              = E->next()) {
        String property_key = E->get().name;
        if (property_key.begins_with("favorite_projects/")) {
            favorites.insert(property_key);
        }
    }

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

        ProjectsListItem* item =
            memnew(ProjectsListItem(property_key, favorite));
        projects.push_back(item);
        item->connect("item_updated", this, "item_updated", varray(item));
    }

    // Create controls
    for (int i = 0; i < projects.size(); ++i) {
        _create_project_item_control(i);
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

        ProjectsListItem* item =
            memnew(ProjectsListItem(property_key, is_favourite));
        projects.push_back(item);
        _create_project_item_control(projects.size() - 1);

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
        if (!projects[i]->grayed && !projects[i]->missing) {
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
    ClassDB::bind_method("item_updated", &ProjectsList::_on_item_updated);
    ClassDB::bind_method(
        "_on_sort_order_selected",
        &ProjectsList::_on_sort_order_selected
    );
    ClassDB::bind_method(
        "_on_search_text_changed",
        &ProjectsList::_on_search_text_changed
    );
    ClassDB::bind_method("_panel_draw", &ProjectsList::_panel_draw);
    ClassDB::bind_method("_panel_input", &ProjectsList::_panel_input);

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

void ProjectsList::_create_project_item_control(int p_index) {
    // Will be added last in the list, so make sure indexes match
    ERR_FAIL_COND(p_index != projects_container->get_child_count());

    ProjectsListItem* item = projects[p_index];

    item->connect("draw", this, "_panel_draw", varray(item));
    item->connect("gui_input", this, "_panel_input", varray(item));

    projects_container->add_child(item);
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

    item->icon_texture->set_texture(icon);
    item->icon_needs_reload = false;
}

void ProjectsList::_on_search_text_changed(const String& p_newtext) {
    sort_projects();
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

void ProjectsList::_on_item_updated(Node* p_node) {
    sort_projects();

    ProjectsListItem* item = Object::cast_to<ProjectsListItem>(p_node);
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

// Draws selected project highlight
void ProjectsList::_panel_draw(Node* p_hb) {
    Control* hb = Object::cast_to<Control>(p_hb);

    hb->draw_line(
        Point2(0, hb->get_size().y + 1),
        Point2(hb->get_size().x - 10, hb->get_size().y + 1),
        get_color("guide_color", "Tree")
    );

    String key = projects[p_hb->get_index()]->project_key;

    if (selected_project_keys.has(key)) {
        hb->draw_style_box(
            get_stylebox("selected", "Tree"),
            Rect2(Point2(), hb->get_size() - Size2(10, 0) * EDSCALE)
        );
    }
}

// Input for each item in the list
void ProjectsList::_panel_input(const Ref<InputEvent>& p_ev, Node* p_hb) {
    Ref<InputEventMouseButton> mb           = p_ev;
    int clicked_index                       = p_hb->get_index();
    const ProjectsListItem* clicked_project = projects[clicked_index];

    if (mb.is_valid() && mb->is_pressed()
        && mb->get_button_index() == BUTTON_LEFT) {
        if (mb->get_shift() && !selected_project_keys.empty()
            && !last_selected_project_key.empty()
            && clicked_project->project_key != last_selected_project_key) {
            int anchor_index = -1;
            for (int i = 0; i < projects.size(); ++i) {
                const ProjectsListItem* p = projects[i];
                if (p->project_key == last_selected_project_key) {
                    anchor_index = p->get_index();
                    break;
                }
            }
            CRASH_COND(anchor_index == -1);
            _select_range(anchor_index, clicked_index);

        } else if (mb->get_control()) {
            _toggle_select(clicked_index);

        } else {
            last_selected_project_key = clicked_project->project_key;
            select_project(clicked_index);
        }

        emit_signal(SIGNAL_SELECTION_CHANGED);

        if (!mb->get_control() && mb->is_doubleclick()) {
            emit_signal(SIGNAL_PROJECT_ASK_OPEN);
        }
    }
}

void ProjectsList::_remove_project(int p_index, bool p_update_settings) {
    ProjectsListItem* item = projects[p_index];

    selected_project_keys.erase(item->project_key);

    if (last_selected_project_key == item->project_key) {
        last_selected_project_key = "";
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

void ProjectsList::_select_range(int p_begin, int p_end) {
    int first = p_begin > p_end ? p_begin : p_end;
    int last  = p_begin > p_end ? p_end : p_begin;
    select_project(first);
    for (int i = first + 1; i <= last; ++i) {
        _toggle_select(i);
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
