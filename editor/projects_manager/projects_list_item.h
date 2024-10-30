// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef PROJECTS_LIST_ITEM_H
#define PROJECTS_LIST_ITEM_H

#include "core/ustring.h"
#include "scene/gui/box_container.h"
#include "scene/gui/texture_button.h"
#include "scene/gui/texture_rect.h"

class ProjectsListItemControl : public HBoxContainer {
    GDCLASS(ProjectsListItemControl, HBoxContainer)

public:
    TextureRect* icon;
    TextureButton* favorite_button;

    bool icon_needs_reload;
    bool hover;

    ProjectsListItemControl();

    void set_is_favorite(bool fav);
    void _notification(int p_what);
};

struct ProjectsListItem {
    enum class SortOrder {
        NAME,
        PATH,
        LAST_MODIFIED,
    };

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

    ProjectsListItemControl* control = nullptr;

    ProjectsListItem() {}

    ProjectsListItem(
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
    ) :
        project_key(p_project),
        project_name(p_name),
        description(p_description),
        path(p_path),
        icon(p_icon),
        main_scene(p_main_scene),
        last_modified(p_last_modified),
        favorite(p_favorite),
        grayed(p_grayed),
        missing(p_missing),
        version(p_version) {}

    _FORCE_INLINE_ bool operator==(const ProjectsListItem& l) const {
        return project_key == l.project_key;
    }
};

class ProjectsListItemComparator {
public:
    ProjectsListItem::SortOrder sort_order;

    // operator<
    bool operator()(const ProjectsListItem& a, const ProjectsListItem& b) const;
};

#endif // PROJECTS_LIST_ITEM_H
