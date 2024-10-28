// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "projects_list_item.h"

#include "editor/editor_scale.h"

ProjectsListItemControl::ProjectsListItemControl() {
    favorite_button   = nullptr;
    icon              = nullptr;
    icon_needs_reload = true;
    hover             = false;

    set_focus_mode(FocusMode::FOCUS_ALL);
}

void ProjectsListItemControl::set_is_favorite(bool fav) {
    favorite_button->set_modulate(
        fav ? Color(1, 1, 1, 1) : Color(1, 1, 1, 0.2)
    );
}

void ProjectsListItemControl::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_MOUSE_ENTER: {
            hover = true;
            update();
        } break;
        case NOTIFICATION_MOUSE_EXIT: {
            hover = false;
            update();
        } break;
        case NOTIFICATION_DRAW: {
            if (hover) {
                draw_style_box(
                    get_stylebox("hover", "Tree"),
                    Rect2(Point2(), get_size() - Size2(10, 0) * EDSCALE)
                );
            }
        } break;
    }
}

bool ProjectsListItemComparator::operator()(
    const ProjectsListItem& a,
    const ProjectsListItem& b
) const {
    if (a.favorite && !b.favorite) {
        return true;
    }
    if (b.favorite && !a.favorite) {
        return false;
    }
    switch (sort_order) {
        case ProjectsListItem::SortOrder::NAME:
            return a.project_name < b.project_name;
        case ProjectsListItem::SortOrder::PATH:
            return a.project_key < b.project_key;
        case ProjectsListItem::SortOrder::LAST_MODIFIED:
            return a.last_modified > b.last_modified;
        default:
            ERR_FAIL_V_MSG(false, "Unrecognised sort order");
    }
}
