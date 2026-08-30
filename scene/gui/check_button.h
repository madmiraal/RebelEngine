// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef CHECK_BUTTON_H
#define CHECK_BUTTON_H

#include "scene/gui/button.h"

class CheckButton : public Button {
    REBEL_OBJECT(CheckButton, Button);

protected:
    Size2 get_icon_size() const;
    Size2 get_minimum_size() const override;
    void _notification(int p_what);

public:
    CheckButton();
    ~CheckButton() override;
};

#endif
