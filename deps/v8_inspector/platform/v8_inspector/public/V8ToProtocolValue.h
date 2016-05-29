// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ToProtocolValue_h
#define V8ToProtocolValue_h

#include "platform/inspector_protocol/Values.h"
#include <v8.h>

namespace blink {

PLATFORM_EXPORT std::unique_ptr<protocol::Value> toProtocolValue(v8::Local<v8::Context>, v8::Local<v8::Value>, int maxDepth = protocol::Value::maxDepth);

} // namespace blink

#endif // V8ToProtocolValue_h
