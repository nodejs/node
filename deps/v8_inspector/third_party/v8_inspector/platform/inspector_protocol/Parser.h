// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Parser_h
#define Parser_h

#include "platform/inspector_protocol/Platform.h"
#include "platform/inspector_protocol/String16.h"

namespace blink {
namespace protocol {

class Value;

PLATFORM_EXPORT std::unique_ptr<Value> parseJSON(const String16& json);

} // namespace platform
} // namespace blink

#endif // !defined(Parser_h)
