// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8HeapProfilerAgent_h
#define V8HeapProfilerAgent_h

#include "platform/PlatformExport.h"
#include "platform/inspector_protocol/Dispatcher.h"
#include "platform/v8_inspector/public/V8Debugger.h"

namespace blink {

class V8RuntimeAgent;

class PLATFORM_EXPORT V8HeapProfilerAgent : public protocol::Backend::HeapProfiler, public V8Debugger::Agent<protocol::Frontend::HeapProfiler> {
public:
    virtual ~V8HeapProfilerAgent() { }
};

} // namespace blink

#endif // !defined(V8HeapProfilerAgent_h)
