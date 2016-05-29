// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8InspectorSessionClient_h
#define V8InspectorSessionClient_h

#include "platform/PlatformExport.h"

#include <v8.h>

namespace blink {

class PLATFORM_EXPORT V8InspectorSessionClient {
public:
    virtual ~V8InspectorSessionClient() { }
    virtual void startInstrumenting() = 0;
    virtual void stopInstrumenting() = 0;
    virtual void resumeStartup() = 0;
    virtual bool canExecuteScripts() = 0;
    virtual void profilingStarted() = 0;
    virtual void profilingStopped() = 0;
};

} // namespace blink


#endif // V8InspectorSessionClient_h
