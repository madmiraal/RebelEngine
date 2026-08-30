// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef CONTAINER_H
#define CONTAINER_H

#include "scene/gui/control.h"

class Container : public Control {
    GDCLASS(Container, Control);

    bool pending_sort;
    void _sort_children();
    void _child_minsize_changed();

protected:
    void queue_sort();
    void add_child_notify(Node* p_child) override;
    void move_child_notify(Node* p_child) override;
    void remove_child_notify(Node* p_child) override;

    void _notification(int p_what) override;
    static void _bind_methods();

public:
    enum {
        NOTIFICATION_SORT_CHILDREN = 50
    };

    void fit_child_in_rect(Control* p_child, const Rect2& p_rect);

    String get_configuration_warning() const override;

    Container();
};

#endif // CONTAINER_H
