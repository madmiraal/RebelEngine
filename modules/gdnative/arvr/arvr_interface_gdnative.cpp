// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#include "arvr_interface_gdnative.h"

#include "main/input_default.h"
#include "servers/arvr/arvr_positional_tracker.h"
#include "servers/visual/visual_server_globals.h"

void ARVRInterfaceGDNative::_bind_methods() {
    ADD_PROPERTY_DEFAULT("interface_is_initialized", false);
    ADD_PROPERTY_DEFAULT("ar_is_anchor_detection_enabled", false);
}

ARVRInterfaceGDNative::ARVRInterfaceGDNative() {
    print_verbose("Construct gdnative interface\n");

    // we won't have our data pointer until our library gets set
    data = nullptr;

    interface = nullptr;
}

ARVRInterfaceGDNative::~ARVRInterfaceGDNative() {
    print_verbose("Destruct gdnative interface\n");

    if (interface != nullptr && is_initialized()) {
        uninitialize();
    };

    // cleanup after ourselves
    cleanup();
}

void ARVRInterfaceGDNative::cleanup() {
    if (interface != nullptr) {
        interface->destructor(data);
        data      = nullptr;
        interface = nullptr;
    }
}

void ARVRInterfaceGDNative::set_interface(
    const rebel_arvr_interface_gdnative* p_interface
) {
    // this should only be called once, just being paranoid..
    if (interface) {
        cleanup();
    }

    // bind to our interface
    interface = p_interface;

    // Now we do our constructing...
    data = interface->constructor((rebel_object*)this);
}

StringName ARVRInterfaceGDNative::get_name() const {
    ERR_FAIL_COND_V(interface == nullptr, StringName());

    rebel_string result = interface->get_name(data);

    StringName name = *(String*)&result;

    rebel_string_destroy(&result);

    return name;
}

int ARVRInterfaceGDNative::get_capabilities() const {
    int capabilities;

    ERR_FAIL_COND_V(interface == nullptr, 0); // 0 = None

    capabilities = interface->get_capabilities(data);

    return capabilities;
}

bool ARVRInterfaceGDNative::get_anchor_detection_is_enabled() const {
    ERR_FAIL_COND_V(interface == nullptr, false);

    return interface->get_anchor_detection_is_enabled(data);
}

void ARVRInterfaceGDNative::set_anchor_detection_is_enabled(bool p_enable) {
    ERR_FAIL_COND(interface == nullptr);

    interface->set_anchor_detection_is_enabled(data, p_enable);
}

int ARVRInterfaceGDNative::get_camera_feed_id() {
    ERR_FAIL_COND_V(interface == nullptr, 0);

    if ((interface->version.major > 1)
        || ((interface->version.major) == 1 && (interface->version.minor >= 1)
        )) {
        return (unsigned int)interface->get_camera_feed_id(data);
    } else {
        return 0;
    }
}

bool ARVRInterfaceGDNative::is_stereo() {
    bool stereo;

    ERR_FAIL_COND_V(interface == nullptr, false);

    stereo = interface->is_stereo(data);

    return stereo;
}

bool ARVRInterfaceGDNative::is_initialized() const {
    ERR_FAIL_COND_V(interface == nullptr, false);

    return interface->is_initialized(data);
}

bool ARVRInterfaceGDNative::initialize() {
    ERR_FAIL_COND_V(interface == nullptr, false);

    bool initialized = interface->initialize(data);

    if (initialized) {
        // if we successfully initialize our interface and we don't have a
        // primary interface yet, this becomes our primary interface

        ARVRServer* arvr_server = ARVRServer::get_singleton();
        if ((arvr_server != nullptr)
            && (arvr_server->get_primary_interface() == nullptr)) {
            arvr_server->set_primary_interface(this);
        };
    };

    return initialized;
}

void ARVRInterfaceGDNative::uninitialize() {
    ERR_FAIL_COND(interface == nullptr);

    ARVRServer* arvr_server = ARVRServer::get_singleton();
    if (arvr_server != nullptr) {
        // Whatever happens, make sure this is no longer our primary interface
        arvr_server->clear_primary_interface_if(this);
    }

    interface->uninitialize(data);
}

Size2 ARVRInterfaceGDNative::get_render_targetsize() {
    ERR_FAIL_COND_V(interface == nullptr, Size2());

    rebel_vector2 result = interface->get_render_targetsize(data);
    Vector2* vec         = (Vector2*)&result;

    return *vec;
}

Transform ARVRInterfaceGDNative::get_transform_for_eye(
    ARVRInterface::Eyes p_eye,
    const Transform& p_cam_transform
) {
    Transform* ret;

    ERR_FAIL_COND_V(interface == nullptr, Transform());

    rebel_transform t = interface->get_transform_for_eye(
        data,
        (int)p_eye,
        (rebel_transform*)&p_cam_transform
    );

    ret = (Transform*)&t;

    return *ret;
}

CameraMatrix ARVRInterfaceGDNative::get_projection_for_eye(
    ARVRInterface::Eyes p_eye,
    real_t p_aspect,
    real_t p_z_near,
    real_t p_z_far
) {
    CameraMatrix cm;

    ERR_FAIL_COND_V(interface == nullptr, CameraMatrix());

    interface->fill_projection_for_eye(
        data,
        (rebel_real*)cm.matrix,
        (rebel_int)p_eye,
        p_aspect,
        p_z_near,
        p_z_far
    );

    return cm;
}

unsigned int ARVRInterfaceGDNative::get_external_texture_for_eye(
    ARVRInterface::Eyes p_eye
) {
    ERR_FAIL_COND_V(interface == nullptr, 0);

    if ((interface->version.major > 1)
        || ((interface->version.major) == 1 && (interface->version.minor >= 1)
        )) {
        return (unsigned int
        )interface->get_external_texture_for_eye(data, (rebel_int)p_eye);
    } else {
        return 0;
    }
}

unsigned int ARVRInterfaceGDNative::get_external_depth_for_eye(
    ARVRInterface::Eyes p_eye
) {
    ERR_FAIL_COND_V(interface == nullptr, 0);

    if ((interface->version.major > 1)
        || ((interface->version.major) == 1 && (interface->version.minor >= 2)
        )) {
        return (unsigned int
        )interface->get_external_depth_for_eye(data, (rebel_int)p_eye);
    } else {
        return 0;
    }
}

void ARVRInterfaceGDNative::commit_for_eye(
    ARVRInterface::Eyes p_eye,
    RID p_render_target,
    const Rect2& p_screen_rect
) {
    ERR_FAIL_COND(interface == nullptr);

    interface->commit_for_eye(
        data,
        (rebel_int)p_eye,
        (rebel_rid*)&p_render_target,
        (rebel_rect2*)&p_screen_rect
    );
}

void ARVRInterfaceGDNative::process() {
    ERR_FAIL_COND(interface == nullptr);

    interface->process(data);
}

void ARVRInterfaceGDNative::notification(int p_what) {
    ERR_FAIL_COND(interface == nullptr);

    // this is only available in interfaces that implement 1.1 or later
    if ((interface->version.major > 1)
        || ((interface->version.major == 1) && (interface->version.minor > 0)
        )) {
        interface->notification(data, p_what);
    }
}

/////////////////////////////////////////////////////////////////////////////////////
// some helper callbacks

extern "C" {

void GDAPI rebel_arvr_register_interface(
    const rebel_arvr_interface_gdnative* p_interface
) {
    Ref<ARVRInterfaceGDNative> new_interface;
    new_interface.instance();
    new_interface->set_interface(
        (const rebel_arvr_interface_gdnative*)p_interface
    );
    ARVRServer::get_singleton()->add_interface(new_interface);
}

rebel_real GDAPI rebel_arvr_get_worldscale() {
    ARVRServer* arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL_V(arvr_server, 1.0);

    return arvr_server->get_world_scale();
}

rebel_transform GDAPI rebel_arvr_get_reference_frame() {
    rebel_transform reference_frame;
    Transform* reference_frame_ptr = (Transform*)&reference_frame;

    ARVRServer* arvr_server = ARVRServer::get_singleton();
    if (arvr_server != nullptr) {
        *reference_frame_ptr = arvr_server->get_reference_frame();
    } else {
        rebel_transform_new_identity(&reference_frame);
    }

    return reference_frame;
}

void GDAPI rebel_arvr_blit(
    rebel_int p_eye,
    rebel_rid* p_render_target,
    rebel_rect2* p_rect
) {
    // blits out our texture as is, handy for preview display of one of the eyes
    // that is already rendered with lens distortion on an external HMD
    ARVRInterface::Eyes eye = (ARVRInterface::Eyes)p_eye;
    RID* render_target      = (RID*)p_render_target;
    Rect2 screen_rect       = *(Rect2*)p_rect;

    if (eye == ARVRInterface::EYE_LEFT) {
        screen_rect.size.x /= 2.0;
    } else if (p_eye == ARVRInterface::EYE_RIGHT) {
        screen_rect.size.x     /= 2.0;
        screen_rect.position.x += screen_rect.size.x;
    }

    VSG::rasterizer->set_current_render_target(RID());
    VSG::rasterizer
        ->blit_render_target_to_screen(*render_target, screen_rect, 0);
}

rebel_int GDAPI rebel_arvr_get_texid(rebel_rid* p_render_target) {
    // In order to send off our textures to display on our hardware we need the
    // opengl texture ID instead of the render target RID This is a handy
    // function to expose that.
    RID* render_target = (RID*)p_render_target;

    RID eye_texture = VSG::storage->render_target_get_texture(*render_target);
    uint32_t texid  = VS::get_singleton()->texture_get_texid(eye_texture);

    return texid;
}

rebel_int GDAPI rebel_arvr_add_controller(
    char* p_device_name,
    rebel_int p_hand,
    rebel_bool p_tracks_orientation,
    rebel_bool p_tracks_position
) {
    ARVRServer* arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL_V(arvr_server, 0);

    InputDefault* input = (InputDefault*)Input::get_singleton();
    ERR_FAIL_NULL_V(input, 0);

    Ref<ARVRPositionalTracker> new_tracker;
    new_tracker.instance();
    new_tracker->set_name(p_device_name);
    new_tracker->set_type(ARVRServer::TRACKER_CONTROLLER);
    if (p_hand == 1) {
        new_tracker->set_hand(ARVRPositionalTracker::TRACKER_LEFT_HAND);
    } else if (p_hand == 2) {
        new_tracker->set_hand(ARVRPositionalTracker::TRACKER_RIGHT_HAND);
    }

    // also register as joystick...
    int joyid = input->get_unused_joy_id();
    if (joyid != -1) {
        new_tracker->set_joy_id(joyid);
        input->joy_connection_changed(joyid, true, p_device_name, "");
    }

    if (p_tracks_orientation) {
        Basis orientation;
        new_tracker->set_orientation(orientation);
    }
    if (p_tracks_position) {
        Vector3 position;
        new_tracker->set_position(position);
    }

    // add our tracker to our server and remember its pointer
    arvr_server->add_tracker(new_tracker);

    // note, this ID is only unique within controllers!
    return new_tracker->get_tracker_id();
}

void GDAPI rebel_arvr_remove_controller(rebel_int p_controller_id) {
    ARVRServer* arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL(arvr_server);

    InputDefault* input = (InputDefault*)Input::get_singleton();
    ERR_FAIL_NULL(input);

    Ref<ARVRPositionalTracker> remove_tracker =
        arvr_server->find_by_type_and_id(
            ARVRServer::TRACKER_CONTROLLER,
            p_controller_id
        );
    if (remove_tracker.is_valid()) {
        // unset our joystick if applicable
        int joyid = remove_tracker->get_joy_id();
        if (joyid != -1) {
            input->joy_connection_changed(joyid, false, "", "");
            remove_tracker->set_joy_id(-1);
        }

        // remove our tracker from our server
        arvr_server->remove_tracker(remove_tracker);
        remove_tracker.unref();
    }
}

void GDAPI rebel_arvr_set_controller_transform(
    rebel_int p_controller_id,
    rebel_transform* p_transform,
    rebel_bool p_tracks_orientation,
    rebel_bool p_tracks_position
) {
    ARVRServer* arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL(arvr_server);

    Ref<ARVRPositionalTracker> tracker = arvr_server->find_by_type_and_id(
        ARVRServer::TRACKER_CONTROLLER,
        p_controller_id
    );
    if (tracker.is_valid()) {
        Transform* transform = (Transform*)p_transform;
        if (p_tracks_orientation) {
            tracker->set_orientation(transform->basis);
        }
        if (p_tracks_position) {
            tracker->set_rw_position(transform->origin);
        }
    }
}

void GDAPI rebel_arvr_set_controller_button(
    rebel_int p_controller_id,
    rebel_int p_button,
    rebel_bool p_is_pressed
) {
    ARVRServer* arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL(arvr_server);

    InputDefault* input = (InputDefault*)Input::get_singleton();
    ERR_FAIL_NULL(input);

    Ref<ARVRPositionalTracker> tracker = arvr_server->find_by_type_and_id(
        ARVRServer::TRACKER_CONTROLLER,
        p_controller_id
    );
    if (tracker.is_valid()) {
        int joyid = tracker->get_joy_id();
        if (joyid != -1) {
            input->joy_button(joyid, p_button, p_is_pressed);
        }
    }
}

void GDAPI rebel_arvr_set_controller_axis(
    rebel_int p_controller_id,
    rebel_int p_axis,
    rebel_real p_value,
    rebel_bool p_can_be_negative
) {
    ARVRServer* arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL(arvr_server);

    InputDefault* input = (InputDefault*)Input::get_singleton();
    ERR_FAIL_NULL(input);

    Ref<ARVRPositionalTracker> tracker = arvr_server->find_by_type_and_id(
        ARVRServer::TRACKER_CONTROLLER,
        p_controller_id
    );
    if (tracker.is_valid()) {
        int joyid = tracker->get_joy_id();
        if (joyid != -1) {
            InputDefault::JoyAxis jx;
            jx.min   = p_can_be_negative ? -1 : 0;
            jx.value = p_value;
            input->joy_axis(joyid, p_axis, jx);
        }
    }
}

rebel_real GDAPI rebel_arvr_get_controller_rumble(rebel_int p_controller_id) {
    ARVRServer* arvr_server = ARVRServer::get_singleton();
    ERR_FAIL_NULL_V(arvr_server, 0.0);

    Ref<ARVRPositionalTracker> tracker = arvr_server->find_by_type_and_id(
        ARVRServer::TRACKER_CONTROLLER,
        p_controller_id
    );
    if (tracker.is_valid()) {
        return tracker->get_rumble();
    }

    return 0.0;
}

void GDAPI rebel_arvr_set_interface(
    rebel_object* p_arvr_interface,
    const rebel_arvr_interface_gdnative* p_gdn_interface
) {
    ARVRInterfaceGDNative* interface = (ARVRInterfaceGDNative*)p_arvr_interface;
    interface->set_interface(
        (const rebel_arvr_interface_gdnative*)p_gdn_interface
    );
}

rebel_int GDAPI rebel_arvr_get_depthid(rebel_rid* p_render_target) {
    // We also need to access our depth texture for reprojection.
    RID* render_target = (RID*)p_render_target;

    uint32_t texid =
        VSG::storage->render_target_get_depth_texture_id(*render_target);

    return texid;
}
}
