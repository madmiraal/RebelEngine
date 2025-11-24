// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef MUTEX_H
#define MUTEX_H

#include "core/error_list.h"
#include "core/typedefs.h"

#ifndef NO_THREADS

#include <mutex>

template <class MutexType>
class MutexImpl {
    mutable MutexType mutex;
    friend class MutexLock;

public:
    _ALWAYS_INLINE_ void lock() const {
        mutex.lock();
    }

    _ALWAYS_INLINE_ void unlock() const {
        mutex.unlock();
    }

    _ALWAYS_INLINE_ Error try_lock() const {
        return mutex.try_lock() ? OK : ERR_BUSY;
    }

    MutexImpl(const MutexImpl<MutexType>&)                        = delete;
    MutexImpl<MutexType>& operator=(const MutexImpl<MutexType>&)  = delete;
    MutexImpl(const MutexImpl<MutexType>&&)                       = delete;
    MutexImpl<MutexType>& operator=(const MutexImpl<MutexType>&&) = delete;
};

// This is written this way instead of being a template to overcome a limitation
// of C++ pre-17 that would require MutexLock to be used like this:
// MutexLock<Mutex> lock;
class MutexLock {
    union {
        std::recursive_mutex* recursive_mutex;
        std::mutex* mutex;
    };

    bool recursive;

public:
    _ALWAYS_INLINE_ explicit MutexLock(
        const MutexImpl<std::recursive_mutex>& p_mutex
    ) :
        recursive_mutex(&p_mutex.mutex),
        recursive(true) {
        recursive_mutex->lock();
    }

    _ALWAYS_INLINE_ explicit MutexLock(const MutexImpl<std::mutex>& p_mutex) :
        mutex(&p_mutex.mutex),
        recursive(false) {
        mutex->lock();
    }

    _ALWAYS_INLINE_ ~MutexLock() {
        if (recursive) {
            recursive_mutex->unlock();
        } else {
            mutex->unlock();
        }
    }

    MutexLock(const MutexLock&)             = delete;
    MutexLock& operator=(const MutexLock&)  = delete;
    MutexLock(const MutexLock&&)            = delete;
    MutexLock& operator=(const MutexLock&&) = delete;
};

using Mutex = MutexImpl<std::recursive_mutex>; // Recursive, for general use
using BinaryMutex = MutexImpl<std::mutex>; // Non-recursive, handle with care

extern template class MutexImpl<std::recursive_mutex>;
extern template class MutexImpl<std::mutex>;

#else // NO_THREADS

class FakeMutex {
    FakeMutex() {}
};

template <class MutexType>
class MutexImpl {
public:
    _ALWAYS_INLINE_ void lock() const {}

    _ALWAYS_INLINE_ void unlock() const {}

    _ALWAYS_INLINE_ Error try_lock() const {
        return OK;
    }

    MutexImpl(const MutexImpl<MutexType>&)                        = delete;
    MutexImpl<MutexType>& operator=(const MutexImpl<MutexType>&)  = delete;
    MutexImpl(const MutexImpl<MutexType>&&)                       = delete;
    MutexImpl<MutexType>& operator=(const MutexImpl<MutexType>&&) = delete;
};

class MutexLock {
public:
    explicit MutexLock(const MutexImpl<FakeMutex>& p_mutex) {}

    MutexLock(const MutexLock&)             = delete;
    MutexLock& operator=(const MutexLock&)  = delete;
    MutexLock(const MutexLock&&)            = delete;
    MutexLock& operator=(const MutexLock&&) = delete;
};

using Mutex       = MutexImpl<FakeMutex>;
using BinaryMutex = MutexImpl<FakeMutex>;

#endif // NO_THREADS

#endif // MUTEX_H
