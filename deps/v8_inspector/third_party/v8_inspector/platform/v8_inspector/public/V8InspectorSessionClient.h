// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8InspectorSessionClient_h
#define V8InspectorSessionClient_h

#include "platform/inspector_protocol/Platform.h"

#include <v8.h>

namespace blink {

class PLATFORM_EXPORT V8InspectorSessionClient {
public:
    virtual ~V8InspectorSessionClient() { }
    virtual void runtimeEnabled() = 0;
    virtual void runtimeDisabled() = 0;
    virtual void resumeStartup() = 0;
    // TODO(dgozman): this was added to support service worker shadow page. We should not connect at all.
    virtual bool canExecuteScripts() = 0;
    virtual void profilingStarted() = 0;
    virtual void profilingStopped() = 0;
    virtual void consoleCleared() = 0;
};

} // namespace blink

#endif // V8InspectorSessionClient_h
