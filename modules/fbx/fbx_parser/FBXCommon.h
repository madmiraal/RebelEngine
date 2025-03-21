// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

/*
Open Asset Import Library (assimp)
----------------------------------------------------------------------

Copyright (c) 2006-2019, assimp team

All rights reserved.

Redistribution and use of this software in source and binary forms,
with or without modification, are permitted provided that the
following conditions are met:

* Redistributions of source code must retain the above
copyright notice, this list of conditions and the
following disclaimer.

* Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the
following disclaimer in the documentation and/or other
materials provided with the distribution.

* Neither the name of the assimp team, nor the names of its
contributors may be used to endorse or promote products
derived from this software without specific prior
written permission of the assimp team.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

----------------------------------------------------------------------
*/

/** @file FBXCommon.h
 * Some useful constants and enums for dealing with FBX files.
 */
#ifndef FBX_COMMON_H
#define FBX_COMMON_H

#include <cstdint>
#include <string>

namespace FBXDocParser {
const std::string NULL_RECORD    = { // 13 null bytes
    '\0',
    '\0',
    '\0',
    '\0',
    '\0',
    '\0',
    '\0',
    '\0',
    '\0',
    '\0',
    '\0',
    '\0',
    '\0'
};                                                // who knows why
const std::string SEPARATOR      = {'\x00', '\x01'}; // for use inside strings
const std::string MAGIC_NODE_TAG = "_$AssimpFbx$";   // from import
const int64_t SECOND             = 46186158000;      // FBX's kTime unit

// rotation order. We'll probably use EulerXYZ for everything
enum RotOrder {
    RotOrder_EulerXYZ = 0,
    RotOrder_EulerXZY,
    RotOrder_EulerYZX,
    RotOrder_EulerYXZ,
    RotOrder_EulerZXY,
    RotOrder_EulerZYX,

    RotOrder_SphericXYZ,

    RotOrder_MAX // end-of-enum sentinel
};

enum TransformInheritance {
    Transform_RrSs = 0,
    Transform_RSrs = 1,
    Transform_Rrs  = 2,
    TransformInheritance_MAX // end-of-enum sentinel
};
} // namespace FBXDocParser

#endif // FBX_COMMON_H
