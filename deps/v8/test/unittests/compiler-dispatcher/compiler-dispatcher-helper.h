// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_COMPILER_DISPATCHER_COMPILER_DISPATCHER_HELPER_H_
#define V8_UNITTESTS_COMPILER_DISPATCHER_COMPILER_DISPATCHER_HELPER_H_

namespace v8 {

class Isolate;

namespace internal {

class Object;
template <typename T>
class Handle;

Handle<Object> RunJS(v8::Isolate* isolate, const char* script);

}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_COMPILER_DISPATCHER_COMPILER_DISPATCHER_HELPER_H_
