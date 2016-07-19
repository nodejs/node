// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackendCallback_h
#define BackendCallback_h

#include "platform/inspector_protocol/ErrorSupport.h"
#include "platform/inspector_protocol/Platform.h"

namespace blink {
namespace protocol {

class PLATFORM_EXPORT BackendCallback {
public:
    virtual ~BackendCallback() { }
    virtual void sendFailure(const ErrorString&) = 0;
};

} // namespace platform
} // namespace blink

#endif // !defined(BackendCallback_h)
