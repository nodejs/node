// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "src/base/build_config.h"
#include "src/base/platform/platform.h"
#include "test/cctest/cctest-utils.h"
#include "test/cctest/cctest.h"

using OS = v8::base::OS;

namespace v8 {
namespace internal {

#ifdef V8_CC_GNU

static uintptr_t sp_addr = 0;

void GetStackPointerCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  GET_STACK_POINTER_TO(sp_addr);
  args.GetReturnValue().Set(v8::Integer::NewFromUnsigned(
      args.GetIsolate(), static_cast<uint32_t>(sp_addr)));
}

TEST(StackAlignment) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate);
  global_template->Set(
      isolate, "get_stack_pointer",
      v8::FunctionTemplate::New(isolate, GetStackPointerCallback));

  LocalContext env(nullptr, global_template);
  CompileRun(
      "function foo() {"
      "  return get_stack_pointer();"
      "}");

  v8::Local<v8::Object> global_object = env->Global();
  v8::Local<v8::Function> foo = v8::Local<v8::Function>::Cast(
      global_object->Get(isolate->GetCurrentContext(), v8_str("foo"))
          .ToLocalChecked());

  v8::Local<v8::Value> result =
      foo->Call(isolate->GetCurrentContext(), global_object, 0, nullptr)
          .ToLocalChecked();
  CHECK_EQ(0u, result->Uint32Value(isolate->GetCurrentContext()).FromJust() %
                   OS::ActivationFrameAlignment());
}
#endif  // V8_CC_GNU

}  // namespace internal
}  // namespace v8
