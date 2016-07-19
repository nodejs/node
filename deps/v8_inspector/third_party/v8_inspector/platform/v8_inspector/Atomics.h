// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Atomics_h
#define Atomics_h

#include <stdint.h>

#if defined(_MSC_VER)
#include <windows.h>
#endif

namespace blink {

#if defined(_MSC_VER)

inline int atomicIncrement(int volatile* addend) { return InterlockedIncrement(reinterpret_cast<long volatile*>(addend)); }

#else

inline int atomicAdd(int volatile* addend, int increment) { return __sync_add_and_fetch(addend, increment); }
inline int atomicIncrement(int volatile* addend) { return atomicAdd(addend, 1); }

#endif

} // namespace blink

#endif /* Atomics_h */
