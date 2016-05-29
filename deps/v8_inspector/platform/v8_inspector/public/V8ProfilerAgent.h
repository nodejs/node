// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ProfilerAgent_h
#define V8ProfilerAgent_h

#include "platform/PlatformExport.h"
#include "platform/inspector_protocol/Dispatcher.h"
#include "platform/v8_inspector/public/V8Debugger.h"

namespace blink {

class PLATFORM_EXPORT V8ProfilerAgent : public protocol::Backend::Profiler, public V8Debugger::Agent<protocol::Frontend::Profiler> {
public:
    virtual ~V8ProfilerAgent() { }
};

} // namespace blink


#endif // !defined(V8ProfilerAgent_h)
