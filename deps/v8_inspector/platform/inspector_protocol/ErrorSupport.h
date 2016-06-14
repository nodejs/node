// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ErrorSupport_h
#define ErrorSupport_h

#include "platform/inspector_protocol/Collections.h"
#include "platform/inspector_protocol/Platform.h"
#include "platform/inspector_protocol/String16.h"

namespace blink {
namespace protocol {

using ErrorString = String16;

class PLATFORM_EXPORT ErrorSupport {
public:
    ErrorSupport();
    ErrorSupport(String16* errorString);
    ~ErrorSupport();

    void push();
    void setName(const String16&);
    void pop();
    void addError(const String16&);
    bool hasErrors();
    String16 errors();

private:
    protocol::Vector<String16> m_path;
    protocol::Vector<String16> m_errors;
    String16* m_errorString;
};

} // namespace platform
} // namespace blink

using blink::protocol::ErrorString;

#endif // !defined(ErrorSupport_h)
