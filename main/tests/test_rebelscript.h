// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef TEST_REBELSCRIPT_H
#define TEST_REBELSCRIPT_H

#include "core/os/main_loop.h"

namespace TestRebelScript {

enum TestType {
    TEST_TOKENIZER,
    TEST_PARSER,
    TEST_COMPILER,
    TEST_BYTECODE,
};

MainLoop* test(TestType p_type);
} // namespace TestRebelScript

#endif // TEST_REBELSCRIPT_H
