// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "projects_list_filter.h"

#include "editor/editor_scale.h"
#include "projects_list.h"

ProjectsListFilter::ProjectsListFilter() {
    sort_order_options = memnew(OptionButton);
    sort_order_options->set_clip_text(true);
    sort_order_options->connect("item_selected", this, "_sort_order_selected");
    sort_order_options->set_custom_minimum_size(Size2(180, 10) * EDSCALE);
    add_child(sort_order_options);

    search_box = memnew(LineEdit);
    search_box->set_placeholder(TTR("Filter projects"));
    search_box->set_tooltip(
        TTR("This field filters projects by name and last path component.\n"
            "To filter projects by name and full path, the query must contain "
            "at least one `/` character.")
    );
    search_box->connect("text_changed", this, "_search_text_changed");
    search_box->set_h_size_flags(SIZE_EXPAND_FILL);
    search_box->set_custom_minimum_size(Size2(280, 10) * EDSCALE);
    add_child(search_box);
}

void ProjectsListFilter::clear_search_text() {
    search_box->clear();
}

LineEdit* ProjectsListFilter::get_search_box() const {
    return search_box;
}

ProjectsListFilter::SortOrder ProjectsListFilter::get_sort_order() const {
    return current_sort_order;
}

String ProjectsListFilter::get_search_text() const {
    return search_box->get_text().strip_edges();
}

void ProjectsListFilter::set_sort_order(SortOrder new_sort_order) {
    sort_order_options->select((int)new_sort_order);
    _sort_order_selected(0);
}

void ProjectsListFilter::set_sort_order_names(
    const Vector<String>& sort_order_names
) {
    sort_order_options->clear();
    for (int i = 0; i < sort_order_names.size(); i++) {
        sort_order_options->add_item(sort_order_names[i]);
    }
}

void ProjectsListFilter::_bind_methods() {
    ClassDB::bind_method(
        D_METHOD("_search_text_changed"),
        &ProjectsListFilter::_search_text_changed
    );
    ClassDB::bind_method(
        D_METHOD("_sort_order_selected"),
        &ProjectsListFilter::_sort_order_selected
    );

    ADD_SIGNAL(MethodInfo("sort_order_changed"));
    ADD_SIGNAL(MethodInfo("search_text_changed"));
}

void ProjectsListFilter::_notification(int p_what) {
    if (p_what == NOTIFICATION_ENTER_TREE) {
        search_box->set_right_icon(get_icon("Search", "EditorIcons"));
        search_box->set_clear_button_enabled(true);
    }
}

void ProjectsListFilter::_sort_order_selected(int p_idx) {
    SortOrder selected = (SortOrder)(sort_order_options->get_selected());
    if (current_sort_order != selected) {
        current_sort_order = selected;
        emit_signal("sort_order_changed");
    }
}

void ProjectsListFilter::_search_text_changed(const String& p_newtext) {
    emit_signal("search_text_changed");
}
