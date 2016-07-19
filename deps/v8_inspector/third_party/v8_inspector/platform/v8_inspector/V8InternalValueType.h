// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8InternalValueType_h
#define V8InternalValueType_h

#include <v8.h>

namespace blink {

enum class V8InternalValueType { kEntry, kLocation, kScope, kScopeList };

bool markAsInternal(v8::Local<v8::Context>, v8::Local<v8::Object>, V8InternalValueType);
bool markArrayEntriesAsInternal(v8::Local<v8::Context>, v8::Local<v8::Array>, V8InternalValueType);
v8::Local<v8::Value> v8InternalValueTypeFrom(v8::Local<v8::Context>, v8::Local<v8::Object>);


} // namespace blink

#endif // V8InternalValueType_h
