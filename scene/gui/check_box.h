// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef CHECK_BOX_H
#define CHECK_BOX_H

#include "scene/gui/button.h"

class CheckBox : public Button {
    REBEL_OBJECT(CheckBox, Button);

protected:
    Size2 get_icon_size() const;
    Size2 get_minimum_size() const override;
    void _notification(int p_what);

    bool is_radio();

public:
    CheckBox(const String& p_text = String());
    ~CheckBox() override;
};

#endif
