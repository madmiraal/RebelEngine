// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "baked_lightmap_editor_plugin.h"

void BakedLightmapEditorPlugin::_bake_select_file(const String& p_file) {
    if (lightmap) {
        BakedLightmap::BakeError err;
        if (get_tree()->get_edited_scene_root()
            && get_tree()->get_edited_scene_root() == lightmap) {
            err = lightmap->bake(lightmap, p_file);
        } else {
            err = lightmap->bake(lightmap->get_parent(), p_file);
        }

        switch (err) {
            case BakedLightmap::BAKE_ERROR_NO_SAVE_PATH: {
                String scene_path = lightmap->get_filename();
                if (scene_path == String()) {
                    scene_path = lightmap->get_owner()->get_filename();
                }
                if (scene_path == String()) {
                    EditorNode::get_singleton()->show_warning(
                        TTR("Can't determine a save path for lightmap "
                            "images.\nSave your scene and try again.")
                    );
                    break;
                }
                scene_path = scene_path.get_basename() + ".lmbake";

                file_dialog->set_current_path(scene_path);
                file_dialog->popup_centered_ratio();

            } break;
            case BakedLightmap::BAKE_ERROR_NO_MESHES:
                EditorNode::get_singleton()->show_warning(
                    TTR("No meshes to bake. Make sure they contain an UV2 "
                        "channel and that the 'Use In Baked Light' and "
                        "'Generate Lightmap' flags are on.")
                );
                break;
            case BakedLightmap::BAKE_ERROR_CANT_CREATE_IMAGE:
                EditorNode::get_singleton()->show_warning(
                    TTR("Failed creating lightmap images, make sure path is "
                        "writable.")
                );
                break;
            case BakedLightmap::BAKE_ERROR_LIGHTMAP_SIZE:
                EditorNode::get_singleton()->show_warning(
                    TTR("Failed determining lightmap size. Maximum lightmap "
                        "size too small?")
                );
                break;
            case BakedLightmap::BAKE_ERROR_INVALID_MESH:
                EditorNode::get_singleton()->show_warning(TTR(
                    "Some mesh is invalid. Make sure the UV2 channel values "
                    "are contained within the [0.0,1.0] square region."
                ));
                break;
            case BakedLightmap::BAKE_ERROR_NO_LIGHTMAPPER:
                EditorNode::get_singleton()->show_warning(
                    TTR("Rebel Editor was built without ray tracing support, "
                        "lightmaps can't be baked.")
                );
                break;
            default: {
            }
        }
    }
}

void BakedLightmapEditorPlugin::_bake() {
    _bake_select_file("");
}

void BakedLightmapEditorPlugin::edit(Object* p_object) {
    BakedLightmap* s = Object::cast_to<BakedLightmap>(p_object);
    if (!s) {
        return;
    }

    lightmap = s;
}

bool BakedLightmapEditorPlugin::handles(Object* p_object) const {
    return p_object->is_class("BakedLightmap");
}

void BakedLightmapEditorPlugin::make_visible(bool p_visible) {
    if (p_visible) {
        bake->show();
    } else {
        bake->hide();
    }
}

EditorProgress* BakedLightmapEditorPlugin::tmp_progress    = nullptr;
EditorProgress* BakedLightmapEditorPlugin::tmp_subprogress = nullptr;

bool BakedLightmapEditorPlugin::bake_func_step(
    float p_progress,
    const String& p_description,
    void*,
    bool p_force_refresh
) {
    if (!tmp_progress) {
        tmp_progress = memnew(
            EditorProgress("bake_lightmaps", TTR("Bake Lightmaps"), 1000, true)
        );
        ERR_FAIL_COND_V(tmp_progress == nullptr, false);
    }
    return tmp_progress
        ->step(p_description, p_progress * 1000, p_force_refresh);
}

bool BakedLightmapEditorPlugin::bake_func_substep(
    float p_progress,
    const String& p_description,
    void*,
    bool p_force_refresh
) {
    if (!tmp_subprogress) {
        tmp_subprogress =
            memnew(EditorProgress("bake_lightmaps_substep", "", 1000, true));
        ERR_FAIL_COND_V(tmp_subprogress == nullptr, false);
    }
    return tmp_subprogress
        ->step(p_description, p_progress * 1000, p_force_refresh);
}

void BakedLightmapEditorPlugin::bake_func_end(uint32_t p_time_started) {
    if (tmp_progress != nullptr) {
        memdelete(tmp_progress);
        tmp_progress = nullptr;
    }

    if (tmp_subprogress != nullptr) {
        memdelete(tmp_subprogress);
        tmp_subprogress = nullptr;
    }

    const int time_taken =
        (OS::get_singleton()->get_ticks_msec() - p_time_started) * 0.001;
    if (time_taken >= 1) {
        // Only print a message and request attention if baking lightmaps took
        // at least 1 second. Otherwise, attempting to bake in an erroneous
        // situation (e.g. no meshes to bake) would print the "done baking
        // lightmaps" message and request attention for no good reason.
        print_line(vformat(
            "Done baking lightmaps in %02d:%02d:%02d.",
            time_taken / 3600,
            (time_taken % 3600) / 60,
            time_taken % 60
        ));

        // Request attention in case the user was doing something else.
        // Baking lightmaps is likely the editor task that can take the most
        // time, so only request the attention for baking lightmaps.
        OS::get_singleton()->request_attention();
    }
}

void BakedLightmapEditorPlugin::_bind_methods() {
    ClassDB::bind_method("_bake", &BakedLightmapEditorPlugin::_bake);
    ClassDB::bind_method(
        "_bake_select_file",
        &BakedLightmapEditorPlugin::_bake_select_file
    );
}

BakedLightmapEditorPlugin::BakedLightmapEditorPlugin(EditorNode* p_node) {
    editor = p_node;
    bake   = memnew(ToolButton);
    bake->set_icon(editor->get_gui_base()->get_icon("Bake", "EditorIcons"));
    bake->set_text(TTR("Bake Lightmaps"));
    bake->hide();
    bake->connect("pressed", this, "_bake");

    file_dialog = memnew(EditorFileDialog);
    file_dialog->set_mode(EditorFileDialog::MODE_SAVE_FILE);
    file_dialog->add_filter("*.lmbake ; LightMap Bake");
    file_dialog->set_title(TTR("Select lightmap bake file:"));
    file_dialog->connect("file_selected", this, "_bake_select_file");
    bake->add_child(file_dialog);

    add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, bake);
    lightmap = nullptr;

    BakedLightmap::bake_step_function    = bake_func_step;
    BakedLightmap::bake_substep_function = bake_func_substep;
    BakedLightmap::bake_end_function     = bake_func_end;
}

BakedLightmapEditorPlugin::~BakedLightmapEditorPlugin() {}
