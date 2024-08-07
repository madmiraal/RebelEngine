// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef MACOS_JOYPAD_H
#define MACOS_JOYPAD_H

#ifdef MACOS_10_0_4
#include <IOKit/hidsystem/IOHIDUsageTables.h>
#else
#include <Kernel/IOKit/hidsystem/IOHIDUsageTables.h>
#endif
#include "main/input_default.h"

#include <ForceFeedback/ForceFeedback.h>
#include <ForceFeedback/ForceFeedbackConstants.h>
#include <IOKit/hid/IOHIDLib.h>

struct rec_element {
    IOHIDElementRef ref;
    IOHIDElementCookie cookie;

    uint32_t usage;

    int min;
    int max;

    struct Comparator {
        bool operator()(const rec_element p_a, const rec_element p_b) const {
            return p_a.usage < p_b.usage;
        }
    };
};

struct joypad {
    IOHIDDeviceRef device_ref;

    Vector<rec_element> axis_elements;
    Vector<rec_element> button_elements;
    Vector<rec_element> hat_elements;

    int id          = 0;
    bool offset_hat = false;

    io_service_t ffservice = 0; /* Interface for force feedback, 0 = no ff */
    FFCONSTANTFORCE ff_constant_force;
    FFDeviceObjectReference ff_device = nullptr;
    FFEffectObjectReference ff_object = nullptr;
    uint64_t ff_timestamp             = 0;
    LONG* ff_directions               = nullptr;
    FFEFFECT ff_effect;
    DWORD* ff_axes = nullptr;

    void add_hid_elements(CFArrayRef p_array);
    void add_hid_element(IOHIDElementRef p_element);

    bool has_element(IOHIDElementCookie p_cookie, Vector<rec_element>* p_list)
        const;
    bool config_force_feedback(io_service_t p_service);
    bool check_ff_features();

    int get_hid_element_state(rec_element* p_element) const;

    void free();
    joypad();
};

class MacOSJoypad {
    enum {
        JOYPADS_MAX = 16,
    };

private:
    InputDefault* input;
    IOHIDManagerRef hid_manager;

    Vector<joypad> device_list;

    bool have_device(IOHIDDeviceRef p_device) const;
    bool configure_joypad(IOHIDDeviceRef p_device_ref, joypad* p_joy);

    int get_joy_index(int p_id) const;
    int get_joy_ref(IOHIDDeviceRef p_device) const;

    void poll_joypads() const;
    void setup_joypad_objects();
    void config_hid_manager(CFArrayRef p_matching_array) const;

    void joypad_vibration_start(
        int p_id,
        float p_magnitude,
        float p_duration,
        uint64_t p_timestamp
    );
    void joypad_vibration_stop(int p_id, uint64_t p_timestamp);

public:
    void process_joypads();

    void _device_added(IOReturn p_res, IOHIDDeviceRef p_device);
    void _device_removed(IOReturn p_res, IOHIDDeviceRef p_device);

    MacOSJoypad();
    ~MacOSJoypad();
};

#endif // MACOS_JOYPAD_H
