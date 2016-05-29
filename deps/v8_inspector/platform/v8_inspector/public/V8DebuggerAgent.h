// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8DebuggerAgent_h
#define V8DebuggerAgent_h

#include "platform/PlatformExport.h"
#include "platform/inspector_protocol/Dispatcher.h"
#include "platform/v8_inspector/public/V8Debugger.h"

namespace blink {

class V8RuntimeAgent;

class PLATFORM_EXPORT V8DebuggerAgent : public protocol::Backend::Debugger, public V8Debugger::Agent<protocol::Frontend::Debugger> {
public:
    virtual ~V8DebuggerAgent() { }
};

} // namespace blink


#endif // V8DebuggerAgent_h
