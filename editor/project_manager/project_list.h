// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef PROJECT_LIST_H
#define PROJECT_LIST_H

#include "core/set.h"
#include "core/ustring.h"
#include "core/vector.h"
#include "project_list_filter.h"
#include "scene/gui/box_container.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/texture_button.h"
#include "scene/gui/texture_rect.h"

class ProjectListItemControl : public HBoxContainer {
    GDCLASS(ProjectListItemControl, HBoxContainer)

public:
    TextureButton* favorite_button;
    TextureRect* icon;
    bool icon_needs_reload;
    bool hover;

    ProjectListItemControl();
    void set_is_favorite(bool fav);
    void _notification(int p_what);
};

class ProjectList : public ScrollContainer {
    GDCLASS(ProjectList, ScrollContainer)

public:
    static const char* SIGNAL_SELECTION_CHANGED;
    static const char* SIGNAL_PROJECT_ASK_OPEN;

    enum MenuOptions {
        GLOBAL_NEW_WINDOW,
        GLOBAL_OPEN_PROJECT
    };

    // Can often be passed by copy
    struct Item {
        String project_key;
        String project_name;
        String description;
        String path;
        String icon;
        String main_scene;
        uint64_t last_modified;
        bool favorite;
        bool grayed;
        bool missing;
        int version;

        ProjectListItemControl* control;

        Item() {}

        Item(
            const String& p_project,
            const String& p_name,
            const String& p_description,
            const String& p_path,
            const String& p_icon,
            const String& p_main_scene,
            uint64_t p_last_modified,
            bool p_favorite,
            bool p_grayed,
            bool p_missing,
            int p_version
        ) {
            project_key   = p_project;
            project_name  = p_name;
            description   = p_description;
            path          = p_path;
            icon          = p_icon;
            main_scene    = p_main_scene;
            last_modified = p_last_modified;
            favorite      = p_favorite;
            grayed        = p_grayed;
            missing       = p_missing;
            version       = p_version;
            control       = nullptr;
        }

        _FORCE_INLINE_ bool operator==(const Item& l) const {
            return project_key == l.project_key;
        }
    };

    ProjectList();
    ~ProjectList();

    void ensure_project_visible(int p_index);
    void erase_missing_projects();
    void erase_selected_projects(bool p_delete_project_contents);
    int get_project_count() const;
    Vector<Item> get_selected_projects() const;
    int get_single_selected_index() const;
    bool is_any_project_missing() const;
    void load_projects();
    int refresh_project(const String& dir_path);
    void select_project(int p_index);
    void set_order_option(ProjectListFilter::FilterOption p_option);
    void set_search_term(String p_search_term);
    const Set<String>& get_selected_project_keys() const;
    void sort_projects();
    void update_dock_menu();

private:
    static void _bind_methods();
    static void load_project_data(
        const String& p_property_key,
        Item& p_item,
        bool p_favorite
    );

    void _create_project_item_control(int p_index);
    void _favorite_pressed(Node* p_hb);
    void _load_project_icon(int p_index);
    void _notification(int p_what);
    void _panel_draw(Node* p_hb);
    void _panel_input(const Ref<InputEvent>& p_ev, Node* p_hb);
    void _remove_project(int p_index, bool p_update_settings);
    void _select_range(int p_begin, int p_end);
    void _show_project(const String& p_path);
    void _toggle_select(int p_index);
    void _update_icons_async();


    String _search_term;
    ProjectListFilter::FilterOption _order_option;
    Set<String> _selected_project_keys;
    String _last_clicked; // Project key
    VBoxContainer* _scroll_children;
    int _icon_load_index;
    Vector<Item> _projects;
};

class ProjectListComparator {
public:
    ProjectListFilter::FilterOption order_option;

    // operator<
    _FORCE_INLINE_ bool operator()(
        const ProjectList::Item& a,
        const ProjectList::Item& b
    ) const {
        if (a.favorite && !b.favorite) {
            return true;
        }
        if (b.favorite && !a.favorite) {
            return false;
        }
        switch (order_option) {
            case ProjectListFilter::FILTER_PATH:
                return a.project_key < b.project_key;
            case ProjectListFilter::FILTER_MODIFIED:
                return a.last_modified > b.last_modified;
            default:
                return a.project_name < b.project_name;
        }
    }
};

#endif // PROJECT_LIST_H
