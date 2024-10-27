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
#include "project_list_item.h"
#include "scene/gui/box_container.h"
#include "scene/gui/scroll_container.h"

class ProjectList : public ScrollContainer {
    GDCLASS(ProjectList, ScrollContainer)

public:
    static const char* SIGNAL_SELECTION_CHANGED;
    static const char* SIGNAL_PROJECT_ASK_OPEN;

    enum MenuOptions {
        GLOBAL_NEW_WINDOW,
        GLOBAL_OPEN_PROJECT
    };

    ProjectList();

    void ensure_project_visible(int p_index);
    void erase_missing_projects();
    void erase_selected_projects(bool p_delete_project_contents);
    int get_project_count() const;
    const Set<String>& get_selected_project_keys() const;
    Vector<ProjectListItem> get_selected_projects() const;
    int get_single_selected_index() const;
    bool is_any_project_missing() const;
    void load_projects();
    int refresh_project(const String& dir_path);
    void select_project(int p_index);
    void set_search_text(String p_search_text);
    void set_sort_order(ProjectListFilter::SortOrder p_new_sort_order);
    void sort_projects();
    void update_dock_menu();

private:
    ProjectListFilter::SortOrder sort_order;

    String search_text;
    String _last_clicked; // Project key

    Set<String> _selected_project_keys;

    VBoxContainer* _scroll_children;

    Vector<ProjectListItem> _projects;

    int _icon_load_index;

    static void _bind_methods();
    static void _load_project_data(
        const String& p_property_key,
        ProjectListItem& p_item,
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
};

#endif // PROJECT_LIST_H
