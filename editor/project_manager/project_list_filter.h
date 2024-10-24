// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef PROJECT_LIST_FILTER_H
#define PROJECT_LIST_FILTER_H

#include "scene/gui/box_container.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"

class ProjectListFilter : public HBoxContainer {
    GDCLASS(ProjectListFilter, HBoxContainer);

public:
    enum FilterOption {
        FILTER_NAME,
        FILTER_PATH,
        FILTER_MODIFIED,
    };

private:
    friend class ProjectManager;

    OptionButton* filter_option;
    LineEdit* search_box;
    bool has_search_box;
    FilterOption _current_filter;

    void _search_text_changed(const String& p_newtext);
    void _filter_option_selected(int p_idx);

protected:
    void _notification(int p_what);
    static void _bind_methods();

public:
    void _setup_filters(Vector<String> options);
    void add_filter_option();

    void add_search_box();
    // May return `nullptr` if the search box wasn't created yet, so check for
    // validity before using the returned value.
    LineEdit* get_search_box() const;

    void set_filter_size(int h_size);
    String get_search_term();
    FilterOption get_filter_option();
    void set_filter_option(FilterOption);
    ProjectListFilter();
    void clear();
};

#endif // PROJECT_LIST_FILTER_H
