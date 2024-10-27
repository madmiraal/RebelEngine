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
    enum class SortOrder {
        NAME,
        PATH,
        LAST_MODIFIED,
    };

    ProjectListFilter();

    void clear();
    SortOrder get_filter_option();
    LineEdit* get_search_box() const;
    String get_search_term();
    void set_filter_option(SortOrder);
    void set_sort_order_names(const Vector<String>& sort_order_names);

protected:
    static void _bind_methods();
    void _notification(int p_what);

private:
    SortOrder _current_filter = SortOrder::NAME;

    LineEdit* search_box;

    OptionButton* filter_option;

    void _filter_option_selected(int p_idx);
    void _search_text_changed(const String& p_newtext);
};

#endif // PROJECT_LIST_FILTER_H
