// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8RuntimeAgent_h
#define V8RuntimeAgent_h

#include "platform/PlatformExport.h"
#include "platform/inspector_protocol/Dispatcher.h"
#include "platform/v8_inspector/public/V8Debugger.h"

namespace blink {

class InjectedScriptManager;

class PLATFORM_EXPORT V8RuntimeAgent : public protocol::Backend::Runtime, public V8Debugger::Agent<protocol::Frontend::Runtime> {
public:
    virtual ~V8RuntimeAgent() { }
};

} // namespace blink

#endif // V8RuntimeAgent_h
