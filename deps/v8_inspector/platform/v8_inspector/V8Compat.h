// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8Compat_h
#define V8Compat_h

#include <v8.h>

#if V8_MAJOR_VERSION < 5 || (V8_MAJOR_VERSION == 5 && V8_MINOR_VERSION < 1)
namespace v8 {

// In standalone V8 inspector this is expected to be noop anyways...
class V8_EXPORT MicrotasksScope {
public:
    enum Type { kRunMicrotasks, kDoNotRunMicrotasks };

    MicrotasksScope(Isolate* isolate, Type type)
    {
        // No-op
    }
};

} // namespace v8

#endif // V8_MAJOR_VERSION < 5 || (V8_MAJOR_VERSION == 5 && V8_MINOR_VERSION < 2)

#endif // V8Compat_h
