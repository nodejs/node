// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8INTERNALVALUETYPE_H_
#define V8_INSPECTOR_V8INTERNALVALUETYPE_H_

#include "include/v8.h"

namespace v8_inspector {

enum class V8InternalValueType { kEntry, kLocation, kScope, kScopeList };

bool markAsInternal(v8::Local<v8::Context>, v8::Local<v8::Object>,
                    V8InternalValueType);
bool markArrayEntriesAsInternal(v8::Local<v8::Context>, v8::Local<v8::Array>,
                                V8InternalValueType);
v8::Local<v8::Value> v8InternalValueTypeFrom(v8::Local<v8::Context>,
                                             v8::Local<v8::Object>);

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8INTERNALVALUETYPE_H_
