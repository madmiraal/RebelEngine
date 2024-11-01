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

class ProjectsListItem : public HBoxContainer {
    GDCLASS(ProjectsListItem, HBoxContainer)

public:
    enum class SortOrder {
        NAME,
        PATH,
        LAST_MODIFIED,
    };

    String project_key;
    String project_name = TTR("Unnamed Project");
    String description;
    String path;
    String icon_path;
    String main_scene;
    int version            = 0;
    uint64_t last_modified = 0;
    bool favorite          = false;
    bool grayed            = false;
    bool missing           = false;
    bool icon_needs_reload = true;
    bool hover             = false;

    TextureRect* icon_texture;
    TextureButton* favorite_button;

    ProjectsListItem(const String& p_property_key, bool p_favorite);

    void _notification(int p_what);
    void set_is_favorite(bool fav);

    _FORCE_INLINE_ bool operator==(const ProjectsListItem l) const {
        return project_key == l.project_key;
    }
};

class ProjectsListItemComparator {
public:
    ProjectsListItem::SortOrder sort_order;

    // operator<
    bool operator()(const ProjectsListItem* a, const ProjectsListItem* b) const;
};

#endif // PROJECTS_LIST_ITEM_H
