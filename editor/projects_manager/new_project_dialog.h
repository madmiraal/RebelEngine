// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef NEW_PROJECT_DIALOG_H
#define NEW_PROJECT_DIALOG_H

#include "core/ustring.h"
#include "scene/gui/button.h"
#include "scene/gui/container.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/texture_rect.h"

class NewProjectDialog : public ConfirmationDialog {
    GDCLASS(NewProjectDialog, ConfirmationDialog);

public:
    NewProjectDialog();

    void show_dialog();

protected:
    static void _bind_methods();
    void _notification(int p_what);
    virtual void ok_pressed();

private:
    enum MessageType {
        MESSAGE_ERROR,
        MESSAGE_WARNING,
        MESSAGE_SUCCESS
    };

    enum InputType {
        PROJECT_PATH,
        INSTALL_PATH
    };

    String created_folder_path;
    String fav_dir;

    AcceptDialog* dialog_error;

    Button* browse;
    Button* create_dir;
    Button* install_browse;
    Ref<ButtonGroup> rasterizer_button_group;

    Container* install_path_container;
    Container* name_container;
    Container* path_container;
    Container* rasterizer_container;

    FileDialog* fdialog;
    FileDialog* fdialog_install;

    Label* msg;

    LineEdit* install_path;
    LineEdit* project_name;
    LineEdit* project_path;

    TextureRect* install_status_rect;
    TextureRect* status_rect;

    void _browse_path();
    void _browse_install_path();
    void _cancel_pressed();
    void _create_folder();
    void _file_selected(const String& p_path);
    void _install_path_selected(const String& p_path);
    void _path_selected(const String& p_path);
    void _path_text_changed(const String& p_path);
    void _remove_created_folder();
    void _set_message(
        const String& p_msg,
        MessageType p_type   = MESSAGE_SUCCESS,
        InputType input_type = PROJECT_PATH
    );
    String _test_path();
    void _text_changed(const String& p_text);
};

#endif // NEW_PROJECT_DIALOG_H