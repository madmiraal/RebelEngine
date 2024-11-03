// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef PROJECTS_LIST_H
#define PROJECTS_LIST_H

#include "core/set.h"
#include "core/ustring.h"
#include "core/vector.h"
#include "projects_list_item.h"
#include "scene/gui/box_container.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/scroll_container.h"

class ProjectsList : public VBoxContainer {
    GDCLASS(ProjectsList, VBoxContainer)

public:
    static const char* SIGNAL_SELECTION_CHANGED;
    static const char* SIGNAL_PROJECT_ASK_OPEN;

    enum MenuOptions {
        GLOBAL_NEW_WINDOW,
        GLOBAL_OPEN_PROJECT
    };

    ProjectsList();

    void ensure_project_visible(int p_index);
    void erase_missing_projects();
    void erase_selected_projects(bool p_delete_project_contents);
    int get_project_count() const;
    const Set<String>& get_selected_project_keys() const;
    Vector<ProjectsListItem*> get_selected_projects() const;
    int get_single_selected_index() const;
    bool is_any_project_missing() const;
    void load_projects();
    void project_created(const String& dir);
    int refresh_project(const String& dir_path);
    void select_project(int p_index);
    void set_search_focus();
    void set_loading();
    void sort_projects();
    void update_dock_menu();

protected:
    static void _bind_methods();
    void _notification(int p_what);

private:
    String first_selected_project_key;
    Set<String> selected_project_keys;

    Label* loading_label;

    LineEdit* search_box;
    OptionButton* sort_order_options;

    ProjectsListItem::SortOrder current_sort_order =
        ProjectsListItem::SortOrder::NAME;

    ScrollContainer* scroll_container;
    VBoxContainer* projects_container;

    Vector<ProjectsListItem*> projects;

    int icon_load_index = 0;

    void _clear_selection();
    void _create_project_item_control(int p_index);
    void _load_project_icon(int p_index);
    void _on_item_double_clicked();
    void _on_search_text_changed(const String& p_newtext);
    void _on_selection_changed(
        bool p_shift_pressed,
        bool p_control_pressed,
        Node* p_node
    );
    void _on_sort_order_selected(int p_index);
    void _on_item_updated(const Node* p_node);
    void _remove_project(int p_index, bool p_update_settings);
    void _select_item(ProjectsListItem* p_item);
    void _select_range(ProjectsListItem* p_to_item);
    void _toggle_select(int p_index);
    void _update_icons_async();
};

#endif // PROJECTS_LIST_H
