// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "project_list_filter.h"

#include "editor/editor_scale.h"
#include "project_list.h"

ProjectListFilter::ProjectListFilter() {
    _current_filter = FILTER_NAME;
    has_search_box  = false;
}

void ProjectListFilter::add_filter_option() {
    filter_option = memnew(OptionButton);
    filter_option->set_clip_text(true);
    filter_option->connect("item_selected", this, "_filter_option_selected");
    add_child(filter_option);
}

void ProjectListFilter::add_search_box() {
    search_box = memnew(LineEdit);
    search_box->set_placeholder(TTR("Filter projects"));
    search_box->set_tooltip(
        TTR("This field filters projects by name and last path component.\nTo "
            "filter projects by name and full path, the query must contain at "
            "least one `/` character.")
    );
    search_box->connect("text_changed", this, "_search_text_changed");
    search_box->set_h_size_flags(SIZE_EXPAND_FILL);
    add_child(search_box);

    has_search_box = true;
}

void ProjectListFilter::clear() {
    if (has_search_box) {
        search_box->clear();
    }
}

ProjectListFilter::FilterOption ProjectListFilter::get_filter_option() {
    return _current_filter;
}

LineEdit* ProjectListFilter::get_search_box() const {
    return search_box;
}

String ProjectListFilter::get_search_term() {
    return search_box->get_text().strip_edges();
}

void ProjectListFilter::set_filter_option(FilterOption option) {
    filter_option->select((int)option);
    _filter_option_selected(0);
}

void ProjectListFilter::set_filter_size(int h_size) {
    filter_option->set_custom_minimum_size(Size2(h_size * EDSCALE, 10 * EDSCALE)
    );
}

void ProjectListFilter::_bind_methods() {
    ClassDB::bind_method(
        D_METHOD("_search_text_changed"),
        &ProjectListFilter::_search_text_changed
    );
    ClassDB::bind_method(
        D_METHOD("_filter_option_selected"),
        &ProjectListFilter::_filter_option_selected
    );

    ADD_SIGNAL(MethodInfo("filter_changed"));
}

void ProjectListFilter::_notification(int p_what) {
    if (p_what == NOTIFICATION_ENTER_TREE && has_search_box) {
        search_box->set_right_icon(get_icon("Search", "EditorIcons"));
        search_box->set_clear_button_enabled(true);
    }
}

void ProjectListFilter::_filter_option_selected(int p_idx) {
    FilterOption selected = (FilterOption)(filter_option->get_selected());
    if (_current_filter != selected) {
        _current_filter = selected;
        emit_signal("filter_changed");
    }
}

void ProjectListFilter::_search_text_changed(const String& p_newtext) {
    emit_signal("filter_changed");
}

void ProjectListFilter::_setup_filters(const Vector<String>& options) {
    filter_option->clear();
    for (int i = 0; i < options.size(); i++) {
        filter_option->add_item(options[i]);
    }
}
