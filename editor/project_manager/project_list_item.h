// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef PROJECT_LIST_ITEM_H
#define PROJECT_LIST_ITEM_H

#include "core/ustring.h"
#include "project_list_filter.h"
#include "scene/gui/box_container.h"
#include "scene/gui/texture_button.h"
#include "scene/gui/texture_rect.h"

class ProjectListItemControl : public HBoxContainer {
    GDCLASS(ProjectListItemControl, HBoxContainer)

public:
    TextureRect* icon;
    TextureButton* favorite_button;

    bool icon_needs_reload;
    bool hover;

    ProjectListItemControl();

    void set_is_favorite(bool fav);
    void _notification(int p_what);
};

struct ProjectListItem {
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

    ProjectListItem() {}

    ProjectListItem(
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

    _FORCE_INLINE_ bool operator==(const ProjectListItem& l) const {
        return project_key == l.project_key;
    }
};

class ProjectListItemComparator {
public:
    ProjectListFilter::SortOrder order_option;

    // operator<
    _FORCE_INLINE_ bool operator()(
        const ProjectListItem& a,
        const ProjectListItem& b
    ) const {
        if (a.favorite && !b.favorite) {
            return true;
        }
        if (b.favorite && !a.favorite) {
            return false;
        }
        switch (order_option) {
            case ProjectListFilter::SortOrder::NAME:
                return a.project_name < b.project_name;
            case ProjectListFilter::SortOrder::PATH:
                return a.project_key < b.project_key;
            case ProjectListFilter::SortOrder::LAST_MODIFIED:
                return a.last_modified > b.last_modified;
        }
    }
};

#endif // PROJECT_LIST_ITEM_H
