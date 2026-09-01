// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef REF_PTR_H
#define REF_PTR_H

#include "core/rid.h"

class RefPtr {
    enum {
        DATASIZE = sizeof(void*) //*4 -ref was shrunk
    };

    mutable char data[DATASIZE]; // too much probably, virtual class + pointer

public:
    bool is_null() const;
    void operator=(const RefPtr& p_other);
    bool operator==(const RefPtr& p_other) const;
    bool operator!=(const RefPtr& p_other) const;
    RID get_rid() const;
    void unref();

    _FORCE_INLINE_ void* get_data() const {
        return data;
    }

    RefPtr(const RefPtr& p_other);
    RefPtr();
    ~RefPtr();
};

#endif // REF_PTR_H
