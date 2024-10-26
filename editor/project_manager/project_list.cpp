// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "project_list.h"

#include "core/io/config_file.h"
#include "core/os/os.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "scene/gui/label.h"

const char* ProjectList::SIGNAL_SELECTION_CHANGED = "selection_changed";
const char* ProjectList::SIGNAL_PROJECT_ASK_OPEN  = "project_ask_open";

ProjectList::ProjectList() {
    _order_option = ProjectListFilter::FILTER_MODIFIED;

    _scroll_children = memnew(VBoxContainer);
    _scroll_children->set_h_size_flags(SIZE_EXPAND_FILL);
    add_child(_scroll_children);

    _icon_load_index = 0;
}

void ProjectList::ensure_project_visible(int p_index) {
    const ProjectListItem& item = _projects[p_index];
    ensure_control_visible(item.control);
}

void ProjectList::erase_missing_projects() {
    if (_projects.empty()) {
        return;
    }

    int deleted_count   = 0;
    int remaining_count = 0;

    for (int i = 0; i < _projects.size(); ++i) {
        const ProjectListItem& item = _projects[i];

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

void ProjectList::erase_selected_projects(bool p_delete_project_contents) {
    if (_selected_project_keys.empty()) {
        return;
    }

    for (int i = 0; i < _projects.size(); ++i) {
        ProjectListItem& item = _projects.write[i];
        if (_selected_project_keys.has(item.project_key)
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
            _projects.remove(i);
            --i;
        }
    }

    EditorSettings::get_singleton()->save();

    _selected_project_keys.clear();
    _last_clicked = "";

    update_dock_menu();
}

int ProjectList::get_project_count() const {
    return _projects.size();
}

const Set<String>& ProjectList::get_selected_project_keys() const {
    // Faster if that's all you need
    return _selected_project_keys;
}

Vector<ProjectListItem> ProjectList::get_selected_projects() const {
    Vector<ProjectListItem> items;
    if (_selected_project_keys.empty()) {
        return items;
    }
    items.resize(_selected_project_keys.size());
    int j = 0;
    for (int i = 0; i < _projects.size(); ++i) {
        const ProjectListItem& item = _projects[i];
        if (_selected_project_keys.has(item.project_key)) {
            items.write[j++] = item;
        }
    }
    ERR_FAIL_COND_V(j != items.size(), items);
    return items;
}

int ProjectList::get_single_selected_index() const {
    if (_selected_project_keys.empty()) {
        // Default selection
        return 0;
    }
    String key;
    if (_selected_project_keys.size() == 1) {
        // Only one selected
        key = _selected_project_keys.front()->get();
    } else {
        // Multiple selected, consider the last clicked one as "main"
        key = _last_clicked;
    }
    for (int i = 0; i < _projects.size(); ++i) {
        if (_projects[i].project_key == key) {
            return i;
        }
    }
    return 0;
}

bool ProjectList::is_any_project_missing() const {
    for (int i = 0; i < _projects.size(); ++i) {
        if (_projects[i].missing) {
            return true;
        }
    }
    return false;
}

void ProjectList::load_projects() {
    // This is a full, hard reload of the list. Don't call this unless really
    // required, it's expensive. If you have 150 projects, it may read through
    // 150 files on your disk at once + load 150 icons.

    // Clear whole list
    for (int i = 0; i < _projects.size(); ++i) {
        ProjectListItem& project = _projects.write[i];
        CRASH_COND(project.control == nullptr);
        memdelete(project.control); // Why not queue_free()?
    }
    _projects.clear();
    _last_clicked = "";
    _selected_project_keys.clear();

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

        ProjectListItem item;
        _load_project_data(property_key, item, favorite);

        _projects.push_back(item);
    }

    // Create controls
    for (int i = 0; i < _projects.size(); ++i) {
        _create_project_item_control(i);
    }

    sort_projects();

    set_v_scroll(0);

    _update_icons_async();

    update_dock_menu();
}

int ProjectList::refresh_project(const String& dir_path) {
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

    bool was_selected = _selected_project_keys.has(project_key);

    // Remove item in any case
    for (int i = 0; i < _projects.size(); ++i) {
        const ProjectListItem& existing_item = _projects[i];
        if (existing_item.path == dir_path) {
            _remove_project(i, false);
            break;
        }
    }

    int index = -1;
    if (should_be_in_list) {
        // Recreate it with updated info

        ProjectListItem item;
        _load_project_data(property_key, item, is_favourite);

        _projects.push_back(item);
        _create_project_item_control(_projects.size() - 1);

        sort_projects();

        for (int i = 0; i < _projects.size(); ++i) {
            if (_projects[i].project_key == project_key) {
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

void ProjectList::select_project(int p_index) {
    Vector<ProjectListItem> previous_selected_items = get_selected_projects();
    _selected_project_keys.clear();

    for (int i = 0; i < previous_selected_items.size(); ++i) {
        previous_selected_items[i].control->update();
    }

    _toggle_select(p_index);
}

void ProjectList::set_order_option(ProjectListFilter::FilterOption p_option) {
    if (_order_option != p_option) {
        _order_option = p_option;
        EditorSettings::get_singleton()->set(
            "project_manager/sorting_order",
            (int)_order_option
        );
        EditorSettings::get_singleton()->save();
    }
}

void ProjectList::set_search_term(String p_search_term) {
    _search_term = p_search_term;
}

void ProjectList::sort_projects() {
    SortArray<ProjectListItem, ProjectListItemComparator> sorter;
    sorter.compare.order_option = _order_option;
    sorter.sort(_projects.ptrw(), _projects.size());

    for (int i = 0; i < _projects.size(); ++i) {
        ProjectListItem& item = _projects.write[i];

        bool visible = true;
        if (!_search_term.empty()) {
            String search_path;
            if (_search_term.find("/") != -1) {
                // Search path will match the whole path
                search_path = item.path;
            } else {
                // Search path will only match the last path component to make
                // searching more strict
                search_path = item.path.get_file();
            }

            // When searching, display projects whose name or path contain the
            // search term
            visible = item.project_name.findn(_search_term) != -1
                   || search_path.findn(_search_term) != -1;
        }

        item.control->set_visible(visible);
    }

    for (int i = 0; i < _projects.size(); ++i) {
        ProjectListItem& item = _projects.write[i];
        item.control->get_parent()->move_child(item.control, i);
    }

    // Rewind the coroutine because order of projects changed
    _update_icons_async();

    update_dock_menu();
}

void ProjectList::update_dock_menu() {
    OS::get_singleton()->global_menu_clear("_dock");

    int favs_added  = 0;
    int total_added = 0;
    for (int i = 0; i < _projects.size(); ++i) {
        if (!_projects[i].grayed && !_projects[i].missing) {
            if (_projects[i].favorite) {
                favs_added++;
            } else {
                if (favs_added != 0) {
                    OS::get_singleton()->global_menu_add_separator("_dock");
                }
                favs_added = 0;
            }
            OS::get_singleton()->global_menu_add_item(
                "_dock",
                _projects[i].project_name + " ( " + _projects[i].path + " )",
                GLOBAL_OPEN_PROJECT,
                Variant(_projects[i].path.plus_file("project.rebel"))
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

void ProjectList::_bind_methods() {
    ClassDB::bind_method("_panel_draw", &ProjectList::_panel_draw);
    ClassDB::bind_method("_panel_input", &ProjectList::_panel_input);
    ClassDB::bind_method("_favorite_pressed", &ProjectList::_favorite_pressed);
    ClassDB::bind_method("_show_project", &ProjectList::_show_project);

    ADD_SIGNAL(MethodInfo(SIGNAL_SELECTION_CHANGED));
    ADD_SIGNAL(MethodInfo(SIGNAL_PROJECT_ASK_OPEN));
}

void ProjectList::_load_project_data(
    const String& p_property_key,
    ProjectListItem& p_item,
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

    p_item = ProjectListItem(
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

void ProjectList::_create_project_item_control(int p_index) {
    // Will be added last in the list, so make sure indexes match
    ERR_FAIL_COND(p_index != _scroll_children->get_child_count());

    ProjectListItem& item = _projects.write[p_index];
    ERR_FAIL_COND(item.control != nullptr); // Already created

    Ref<Texture> favorite_icon = get_icon("Favorites", "EditorIcons");
    Color font_color           = get_color("font_color", "Tree");

    ProjectListItemControl* hb = memnew(ProjectListItemControl);
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

    _scroll_children->add_child(hb);
    item.control = hb;
}

void ProjectList::_favorite_pressed(Node* p_hb) {
    ProjectListItemControl* control =
        Object::cast_to<ProjectListItemControl>(p_hb);

    int index            = control->get_index();
    ProjectListItem item = _projects.write[index]; // Take copy

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

    _projects.write[index] = item;

    control->set_is_favorite(item.favorite);

    sort_projects();

    if (item.favorite) {
        for (int i = 0; i < _projects.size(); ++i) {
            if (_projects[i].project_key == item.project_key) {
                ensure_project_visible(i);
                break;
            }
        }
    }

    update_dock_menu();
}

void ProjectList::_load_project_icon(int p_index) {
    ProjectListItem& item = _projects.write[p_index];

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

void ProjectList::_notification(int p_what) {
    if (p_what == NOTIFICATION_PROCESS) {
        // Load icons as a coroutine to speed up launch when you have hundreds
        // of projects
        if (_icon_load_index < _projects.size()) {
            ProjectListItem& item = _projects.write[_icon_load_index];
            if (item.control->icon_needs_reload) {
                _load_project_icon(_icon_load_index);
            }
            _icon_load_index++;

        } else {
            set_process(false);
        }
    }
}

// Draws selected project highlight
void ProjectList::_panel_draw(Node* p_hb) {
    Control* hb = Object::cast_to<Control>(p_hb);

    hb->draw_line(
        Point2(0, hb->get_size().y + 1),
        Point2(hb->get_size().x - 10, hb->get_size().y + 1),
        get_color("guide_color", "Tree")
    );

    String key = _projects[p_hb->get_index()].project_key;

    if (_selected_project_keys.has(key)) {
        hb->draw_style_box(
            get_stylebox("selected", "Tree"),
            Rect2(Point2(), hb->get_size() - Size2(10, 0) * EDSCALE)
        );
    }
}

// Input for each item in the list
void ProjectList::_panel_input(const Ref<InputEvent>& p_ev, Node* p_hb) {
    Ref<InputEventMouseButton> mb          = p_ev;
    int clicked_index                      = p_hb->get_index();
    const ProjectListItem& clicked_project = _projects[clicked_index];

    if (mb.is_valid() && mb->is_pressed()
        && mb->get_button_index() == BUTTON_LEFT) {
        if (mb->get_shift() && !_selected_project_keys.empty()
            && !_last_clicked.empty()
            && clicked_project.project_key != _last_clicked) {
            int anchor_index = -1;
            for (int i = 0; i < _projects.size(); ++i) {
                const ProjectListItem& p = _projects[i];
                if (p.project_key == _last_clicked) {
                    anchor_index = p.control->get_index();
                    break;
                }
            }
            CRASH_COND(anchor_index == -1);
            _select_range(anchor_index, clicked_index);

        } else if (mb->get_control()) {
            _toggle_select(clicked_index);

        } else {
            _last_clicked = clicked_project.project_key;
            select_project(clicked_index);
        }

        emit_signal(SIGNAL_SELECTION_CHANGED);

        if (!mb->get_control() && mb->is_doubleclick()) {
            emit_signal(SIGNAL_PROJECT_ASK_OPEN);
        }
    }
}

void ProjectList::_remove_project(int p_index, bool p_update_settings) {
    const ProjectListItem item = _projects[p_index]; // Take a copy

    _selected_project_keys.erase(item.project_key);

    if (_last_clicked == item.project_key) {
        _last_clicked = "";
    }

    memdelete(item.control);
    _projects.remove(p_index);

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

void ProjectList::_select_range(int p_begin, int p_end) {
    int first = p_begin > p_end ? p_begin : p_end;
    int last  = p_begin > p_end ? p_end : p_begin;
    select_project(first);
    for (int i = first + 1; i <= last; ++i) {
        _toggle_select(i);
    }
}

void ProjectList::_show_project(const String& p_path) {
    OS::get_singleton()->shell_open(String("file://") + p_path);
}

void ProjectList::_toggle_select(int p_index) {
    ProjectListItem& item = _projects.write[p_index];
    if (_selected_project_keys.has(item.project_key)) {
        _selected_project_keys.erase(item.project_key);
    } else {
        _selected_project_keys.insert(item.project_key);
    }
    item.control->update();
}

void ProjectList::_update_icons_async() {
    _icon_load_index = 0;
    set_process(true);
}
