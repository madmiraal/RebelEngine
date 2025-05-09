// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef UTIL_MACROS_H
#define UTIL_MACROS_H

#define _GD_VARNAME_CONCAT_B_(m_ignore, m_name) m_name
#define _GD_VARNAME_CONCAT_A_(m_a, m_b, m_c)                                   \
    _GD_VARNAME_CONCAT_B_(hello there, m_a##m_b##m_c)
#define _GD_VARNAME_CONCAT_(m_a, m_b, m_c) _GD_VARNAME_CONCAT_A_(m_a, m_b, m_c)
#define GD_UNIQUE_NAME(m_name)             _GD_VARNAME_CONCAT_(m_name, _, __COUNTER__)

// static assert
// TODO: Get rid of this macro once we upgrade to C++11

#ifdef __cpp_static_assert
#define GD_STATIC_ASSERT(m_cond)                                               \
    static_assert((m_cond), "Condition '" #m_cond "' failed")
#else
#define GD_STATIC_ASSERT(m_cond)                                               \
    typedef int GD_UNIQUE_NAME(rebel_static_assert)[((m_cond) ? 1 : -1)]
#endif

// final
// TODO: Get rid of this macro once we upgrade to C++11

#if (__cplusplus >= 201103L)
#define GD_FINAL final
#else
#define GD_FINAL
#endif

// noreturn
// TODO: Get rid of this macro once we upgrade to C++11

#if (__cplusplus >= 201103L)
#define GD_NORETURN [[noreturn]]
#elif defined(__GNUC__)
#define GD_NORETURN __attribute__((noreturn))
#elif defined(_MSC_VER)
#define GD_NORETURN __declspec(noreturn)
#else
#define GD_NORETURN
#pragma message "Macro GD_NORETURN will have no effect"
#endif

// unreachable

#if defined(_MSC_VER)
#define GD_UNREACHABLE() __assume(0)
#elif defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__) >= 405
#define GD_UNREACHABLE() __builtin_unreachable()
#else
#define GD_UNREACHABLE()                                                       \
    CRASH_NOW();                                                               \
    do {                                                                       \
    } while (true);
#endif

namespace gdmono {

template <typename F>
struct ScopeExit {
    ScopeExit(F p_exit_func) : exit_func(p_exit_func) {}

    ~ScopeExit() {
        exit_func();
    }

    F exit_func;
};

class ScopeExitAux {
public:
    template <typename F>
    ScopeExit<F> operator+(F p_exit_func) {
        return ScopeExit<F>(p_exit_func);
    }
};

} // namespace gdmono

#define SCOPE_EXIT                                                             \
    auto GD_UNIQUE_NAME(gd_scope_exit) = gdmono::ScopeExitAux() + [=]() -> void

#endif // UTIL_MACROS_H
