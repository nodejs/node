// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FrontendChannel_h
#define FrontendChannel_h

#include "platform/inspector_protocol/Values.h"

namespace blink {
namespace protocol {

class PLATFORM_EXPORT FrontendChannel {
public:
    virtual ~FrontendChannel() { }
    virtual void sendProtocolResponse(int callId, const String16& message) = 0;
    virtual void sendProtocolNotification(const String16& message) = 0;
    virtual void flushProtocolNotifications() = 0;
};

} // namespace protocol
} // namespace blink

#endif // !defined(FrontendChannel_h)
