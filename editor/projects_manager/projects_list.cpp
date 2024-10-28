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
    sort_order = ProjectsListFilter::SortOrder::LAST_MODIFIED;
    set_h_size_flags(SIZE_EXPAND_FILL);

    set_theme(create_custom_theme());

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

    Vector<String> sort_order_names;
    sort_order_names.push_back(TTR("Name"));
    sort_order_names.push_back(TTR("Path"));
    sort_order_names.push_back(TTR("Last Modified"));

    projects_list_filter = memnew(ProjectsListFilter);
    projects_list_filter->set_sort_order_names(sort_order_names);
    int previous_sort_order =
        EditorSettings::get_singleton()->get("project_manager/sorting_order");
    projects_list_filter->set_sort_order((ProjectsListFilter::SortOrder
    )previous_sort_order);
    projects_list_filter
        ->connect("sort_order_changed", this, "_on_sort_order_changed");
    projects_list_filter
        ->connect("search_text_changed", this, "_on_search_text_changed");
    projects_list_tools_container->add_child(projects_list_filter);

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

    load_recent_projects();
}

void ProjectsList::ensure_project_visible(int p_index) {
    const ProjectsListItem& item = projects[p_index];
    scroll_container->ensure_control_visible(item.control);
}

void ProjectsList::erase_missing_projects() {
    if (projects.empty()) {
        return;
    }

    int deleted_count   = 0;
    int remaining_count = 0;

    for (int i = 0; i < projects.size(); ++i) {
        const ProjectsListItem& item = projects[i];

        if (item.missing) {
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
        ProjectsListItem& item = projects.write[i];
        if (selected_project_keys.has(item.project_key)
            && item.control->is_visible()) {
            EditorSettings::get_singleton()->erase(
                "projects/" + item.project_key
            );
            EditorSettings::get_singleton()->erase(
                "favorite_projects/" + item.project_key
            );

            if (p_delete_project_contents) {
                OS::get_singleton()->move_to_trash(item.path);
            }

            memdelete(item.control);
            projects.remove(i);
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

Vector<ProjectsListItem> ProjectsList::get_selected_projects() const {
    Vector<ProjectsListItem> items;
    if (selected_project_keys.empty()) {
        return items;
    }
    items.resize(selected_project_keys.size());
    int j = 0;
    for (int i = 0; i < projects.size(); ++i) {
        const ProjectsListItem& item = projects[i];
        if (selected_project_keys.has(item.project_key)) {
            items.write[j++] = item;
        }
    }
    ERR_FAIL_COND_V(j != items.size(), items);
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
        if (projects[i].project_key == key) {
            return i;
        }
    }
    return 0;
}

bool ProjectsList::is_any_project_missing() const {
    for (int i = 0; i < projects.size(); ++i) {
        if (projects[i].missing) {
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
        ProjectsListItem& project = projects.write[i];
        CRASH_COND(project.control == nullptr);
        memdelete(project.control); // Why not queue_free()?
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

        ProjectsListItem item;
        _load_project_data(property_key, item, favorite);

        projects.push_back(item);
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

void ProjectsList::load_recent_projects() {
    set_sort_order(projects_list_filter->get_sort_order());
    set_search_text(projects_list_filter->get_search_text());
    load_projects();
}

void ProjectsList::project_created(const String& dir) {
    projects_list_filter->clear_search_text();
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
        const ProjectsListItem& existing_item = projects[i];
        if (existing_item.path == dir_path) {
            _remove_project(i, false);
            break;
        }
    }

    int index = -1;
    if (should_be_in_list) {
        // Recreate it with updated info

        ProjectsListItem item;
        _load_project_data(property_key, item, is_favourite);

        projects.push_back(item);
        _create_project_item_control(projects.size() - 1);

        sort_projects();

        for (int i = 0; i < projects.size(); ++i) {
            if (projects[i].project_key == project_key) {
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
    Vector<ProjectsListItem> previous_selected_items = get_selected_projects();
    selected_project_keys.clear();

    for (int i = 0; i < previous_selected_items.size(); ++i) {
        previous_selected_items[i].control->update();
    }

    _toggle_select(p_index);
}

void ProjectsList::set_search_focus() {
    projects_list_filter->get_search_box()->grab_focus();
}

void ProjectsList::set_loading() {
    loading_label->set_modulate(Color(1, 1, 1));
}

void ProjectsList::set_search_text(String p_search_text) {
    search_text = p_search_text;
}

void ProjectsList::set_sort_order(ProjectsListFilter::SortOrder new_sort_order
) {
    if (sort_order != new_sort_order) {
        sort_order = new_sort_order;
        EditorSettings::get_singleton()->set(
            "project_manager/sorting_order",
            (int)sort_order
        );
        EditorSettings::get_singleton()->save();
    }
}

void ProjectsList::sort_projects() {
    SortArray<ProjectsListItem, ProjectsListItemComparator> sorter;
    sorter.compare.sort_order = sort_order;
    sorter.sort(projects.ptrw(), projects.size());

    for (int i = 0; i < projects.size(); ++i) {
        ProjectsListItem& item = projects.write[i];

        bool visible = true;
        if (!search_text.empty()) {
            String search_path;
            if (search_text.find("/") != -1) {
                // Search path will match the whole path
                search_path = item.path;
            } else {
                // Search path will only match the last path component to make
                // searching more strict
                search_path = item.path.get_file();
            }

            // When searching, display projects whose name or path contain the
            // search term
            visible = item.project_name.findn(search_text) != -1
                   || search_path.findn(search_text) != -1;
        }

        item.control->set_visible(visible);
    }

    for (int i = 0; i < projects.size(); ++i) {
        ProjectsListItem& item = projects.write[i];
        item.control->get_parent()->move_child(item.control, i);
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
        if (!projects[i].grayed && !projects[i].missing) {
            if (projects[i].favorite) {
                favs_added++;
            } else {
                if (favs_added != 0) {
                    OS::get_singleton()->global_menu_add_separator("_dock");
                }
                favs_added = 0;
            }
            OS::get_singleton()->global_menu_add_item(
                "_dock",
                projects[i].project_name + " ( " + projects[i].path + " )",
                GLOBAL_OPEN_PROJECT,
                Variant(projects[i].path.plus_file("project.rebel"))
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
        "_on_sort_order_changed",
        &ProjectsList::_on_sort_order_changed
    );
    ClassDB::bind_method(
        "_on_search_text_changed",
        &ProjectsList::_on_search_text_changed
    );
    ClassDB::bind_method("_panel_draw", &ProjectsList::_panel_draw);
    ClassDB::bind_method("_panel_input", &ProjectsList::_panel_input);
    ClassDB::bind_method("_favorite_pressed", &ProjectsList::_favorite_pressed);
    ClassDB::bind_method("_show_project", &ProjectsList::_show_project);

    ADD_SIGNAL(MethodInfo(SIGNAL_SELECTION_CHANGED));
    ADD_SIGNAL(MethodInfo(SIGNAL_PROJECT_ASK_OPEN));
}

void ProjectsList::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_PROCESS: {
            // Load icons as a co-routine to speed up launch
            // when there are many projects.
            if (icon_load_index < projects.size()) {
                ProjectsListItem& item = projects.write[icon_load_index];
                if (item.control->icon_needs_reload) {
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
                projects_list_filter->get_search_box()->grab_focus();
            }
        } break;
    }
}

void ProjectsList::_load_project_data(
    const String& p_property_key,
    ProjectsListItem& p_item,
    bool p_favorite
) {
    String path  = EditorSettings::get_singleton()->get(p_property_key);
    String conf  = path.plus_file("project.rebel");
    bool grayed  = false;
    bool missing = false;

    Ref<ConfigFile> cf = memnew(ConfigFile);
    Error cf_err       = cf->load(conf);

    int config_version  = 0;
    String project_name = TTR("Unnamed Project");
    if (cf_err == OK) {
        String cf_project_name =
            static_cast<String>(cf->get_value("application", "config/name", "")
            );
        if (!cf_project_name.empty()) {
            project_name = cf_project_name.xml_unescape();
        }
        config_version = (int)cf->get_value("", "config_version", 0);
    }

    if (config_version > ProjectSettings::CONFIG_VERSION) {
        // It comes from an more recent, non-backward compatible version.
        grayed = true;
    }

    String description = cf->get_value("application", "config/description", "");
    String icon        = cf->get_value("application", "config/icon", "");
    String main_scene  = cf->get_value("application", "run/main_scene", "");

    uint64_t last_modified = 0;
    if (FileAccess::exists(conf)) {
        last_modified = FileAccess::get_modified_time(conf);

        String fscache = path.plus_file(".fscache");
        if (FileAccess::exists(fscache)) {
            uint64_t cache_modified = FileAccess::get_modified_time(fscache);
            if (cache_modified > last_modified) {
                last_modified = cache_modified;
            }
        }
    } else {
        grayed  = true;
        missing = true;
        print_line("Project is missing: " + conf);
    }

    String project_key = p_property_key.get_slice("/", 1);

    p_item = ProjectsListItem(
        project_key,
        project_name,
        description,
        path,
        icon,
        main_scene,
        last_modified,
        p_favorite,
        grayed,
        missing,
        config_version
    );
}

void ProjectsList::_create_project_item_control(int p_index) {
    // Will be added last in the list, so make sure indexes match
    ERR_FAIL_COND(p_index != projects_container->get_child_count());

    ProjectsListItem& item = projects.write[p_index];
    ERR_FAIL_COND(item.control != nullptr); // Already created

    Ref<Texture> favorite_icon = get_icon("Favorites", "EditorIcons");
    Color font_color           = get_color("font_color", "Tree");

    ProjectsListItemControl* hb = memnew(ProjectsListItemControl);
    hb->connect("draw", this, "_panel_draw", varray(hb));
    hb->connect("gui_input", this, "_panel_input", varray(hb));
    hb->add_constant_override("separation", 10 * EDSCALE);
    hb->set_tooltip(item.description);

    VBoxContainer* favorite_box = memnew(VBoxContainer);
    favorite_box->set_name("FavoriteBox");
    TextureButton* favorite = memnew(TextureButton);
    favorite->set_name("FavoriteButton");
    favorite->set_normal_texture(favorite_icon);
    // This makes the project's "hover" style display correctly when hovering
    // the favorite icon
    favorite->set_mouse_filter(MOUSE_FILTER_PASS);
    favorite->connect("pressed", this, "_favorite_pressed", varray(hb));
    favorite_box->add_child(favorite);
    favorite_box->set_alignment(BoxContainer::ALIGN_CENTER);
    hb->add_child(favorite_box);
    hb->favorite_button = favorite;
    hb->set_is_favorite(item.favorite);

    TextureRect* tf = memnew(TextureRect);
    // The project icon may not be loaded by the time the control is displayed,
    // so use a loading placeholder.
    tf->set_texture(get_icon("ProjectIconLoading", "EditorIcons"));
    tf->set_v_size_flags(SIZE_SHRINK_CENTER);
    if (item.missing) {
        tf->set_modulate(Color(1, 1, 1, 0.5));
    }
    hb->add_child(tf);
    hb->icon = tf;

    VBoxContainer* vb = memnew(VBoxContainer);
    if (item.grayed) {
        vb->set_modulate(Color(1, 1, 1, 0.5));
    }
    vb->set_h_size_flags(SIZE_EXPAND_FILL);
    hb->add_child(vb);
    Control* ec = memnew(Control);
    ec->set_custom_minimum_size(Size2(0, 1));
    ec->set_mouse_filter(MOUSE_FILTER_PASS);
    vb->add_child(ec);
    Label* title =
        memnew(Label(!item.missing ? item.project_name : TTR("Missing Project"))
        );
    title->add_font_override("font", get_font("title", "EditorFonts"));
    title->add_color_override("font_color", font_color);
    title->set_clip_text(true);
    vb->add_child(title);

    HBoxContainer* path_hb = memnew(HBoxContainer);
    path_hb->set_h_size_flags(SIZE_EXPAND_FILL);
    vb->add_child(path_hb);

    Button* show = memnew(Button);
    // Display a folder icon if the project directory can be opened, or a
    // "broken file" icon if it can't.
    show->set_icon(
        get_icon(!item.missing ? "Load" : "FileBroken", "EditorIcons")
    );
    if (!item.grayed) {
        // Don't make the icon less prominent if the parent is already grayed
        // out.
        show->set_modulate(Color(1, 1, 1, 0.5));
    }
    path_hb->add_child(show);

    if (!item.missing) {
        show->connect("pressed", this, "_show_project", varray(item.path));
        show->set_tooltip(TTR("Show in File Manager"));
    } else {
        show->set_tooltip(TTR("Error: Project is missing on the filesystem."));
    }

    Label* fpath = memnew(Label(item.path));
    path_hb->add_child(fpath);
    fpath->set_h_size_flags(SIZE_EXPAND_FILL);
    fpath->set_modulate(Color(1, 1, 1, 0.5));
    fpath->add_color_override("font_color", font_color);
    fpath->set_clip_text(true);

    projects_container->add_child(hb);
    item.control = hb;
}

void ProjectsList::_favorite_pressed(Node* p_hb) {
    ProjectsListItemControl* control =
        Object::cast_to<ProjectsListItemControl>(p_hb);

    int index             = control->get_index();
    ProjectsListItem item = projects.write[index]; // Take copy

    item.favorite = !item.favorite;

    if (item.favorite) {
        EditorSettings::get_singleton()->set(
            "favorite_projects/" + item.project_key,
            item.path
        );
    } else {
        EditorSettings::get_singleton()->erase(
            "favorite_projects/" + item.project_key
        );
    }
    EditorSettings::get_singleton()->save();

    projects.write[index] = item;

    control->set_is_favorite(item.favorite);

    sort_projects();

    if (item.favorite) {
        for (int i = 0; i < projects.size(); ++i) {
            if (projects[i].project_key == item.project_key) {
                ensure_project_visible(i);
                break;
            }
        }
    }

    update_dock_menu();
}

void ProjectsList::_load_project_icon(int p_index) {
    ProjectsListItem& item = projects.write[p_index];

    Ref<Texture> default_icon = get_icon("DefaultProjectIcon", "EditorIcons");
    Ref<Texture> icon;
    if (!item.icon.empty()) {
        Ref<Image> img;
        img.instance();
        Error err =
            img->load(item.icon.replace_first("res://", item.path + "/"));
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

    item.control->icon->set_texture(icon);
    item.control->icon_needs_reload = false;
}

void ProjectsList::_on_search_text_changed() {
    set_search_text(projects_list_filter->get_search_text());
    sort_projects();
}

void ProjectsList::_on_sort_order_changed() {
    set_sort_order(projects_list_filter->get_sort_order());
    sort_projects();
}

// Draws selected project highlight
void ProjectsList::_panel_draw(Node* p_hb) {
    Control* hb = Object::cast_to<Control>(p_hb);

    hb->draw_line(
        Point2(0, hb->get_size().y + 1),
        Point2(hb->get_size().x - 10, hb->get_size().y + 1),
        get_color("guide_color", "Tree")
    );

    String key = projects[p_hb->get_index()].project_key;

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
    const ProjectsListItem& clicked_project = projects[clicked_index];

    if (mb.is_valid() && mb->is_pressed()
        && mb->get_button_index() == BUTTON_LEFT) {
        if (mb->get_shift() && !selected_project_keys.empty()
            && !last_selected_project_key.empty()
            && clicked_project.project_key != last_selected_project_key) {
            int anchor_index = -1;
            for (int i = 0; i < projects.size(); ++i) {
                const ProjectsListItem& p = projects[i];
                if (p.project_key == last_selected_project_key) {
                    anchor_index = p.control->get_index();
                    break;
                }
            }
            CRASH_COND(anchor_index == -1);
            _select_range(anchor_index, clicked_index);

        } else if (mb->get_control()) {
            _toggle_select(clicked_index);

        } else {
            last_selected_project_key = clicked_project.project_key;
            select_project(clicked_index);
        }

        emit_signal(SIGNAL_SELECTION_CHANGED);

        if (!mb->get_control() && mb->is_doubleclick()) {
            emit_signal(SIGNAL_PROJECT_ASK_OPEN);
        }
    }
}

void ProjectsList::_remove_project(int p_index, bool p_update_settings) {
    const ProjectsListItem item = projects[p_index]; // Take a copy

    selected_project_keys.erase(item.project_key);

    if (last_selected_project_key == item.project_key) {
        last_selected_project_key = "";
    }

    memdelete(item.control);
    projects.remove(p_index);

    if (p_update_settings) {
        EditorSettings::get_singleton()->erase("projects/" + item.project_key);
        EditorSettings::get_singleton()->erase(
            "favorite_projects/" + item.project_key
        );
        // Not actually saving the file, in case you are doing more changes to
        // settings
    }

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

void ProjectsList::_show_project(const String& p_path) {
    OS::get_singleton()->shell_open(String("file://") + p_path);
}

void ProjectsList::_toggle_select(int p_index) {
    ProjectsListItem& item = projects.write[p_index];
    if (selected_project_keys.has(item.project_key)) {
        selected_project_keys.erase(item.project_key);
    } else {
        selected_project_keys.insert(item.project_key);
    }
    item.control->update();
}

void ProjectsList::_update_icons_async() {
    icon_load_index = 0;
    set_process(true);
}
