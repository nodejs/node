// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/win32-headers.h"
#include "src/init/v8.h"
#include "test/cctest/cctest.h"

class UnwindingWinX64Callbacks {
 public:
  UnwindingWinX64Callbacks() = default;

  static void Getter(v8::Local<v8::String> name,
                     const v8::PropertyCallbackInfo<v8::Value>& info) {
    // Expects to find at least 15 stack frames in the call stack.
    // The stack walking should fail on stack frames for builtin functions if
    // stack unwinding data has not been correctly registered.
    int stack_frames = CountCallStackFrames(15);
    CHECK_GE(stack_frames, 15);
  }
  static void Setter(v8::Local<v8::String> name, v8::Local<v8::Value> value,
                     const v8::PropertyCallbackInfo<void>& info) {}

 private:
  // Windows-specific code to walk the stack starting from the current
  // instruction pointer.
  static int CountCallStackFrames(int max_frames) {
    CONTEXT context_record;
    ::RtlCaptureContext(&context_record);

    int iframe = 0;
    while (++iframe < max_frames) {
      uint64_t image_base;
      PRUNTIME_FUNCTION function_entry =
          ::RtlLookupFunctionEntry(context_record.Rip, &image_base, nullptr);
      if (!function_entry) break;

      void* handler_data;
      uint64_t establisher_frame;
      ::RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, context_record.Rip,
                         function_entry, &context_record, &handler_data,
                         &establisher_frame, NULL);
    }
    return iframe;
  }
};

// Verifies that stack unwinding data has been correctly registered on Win/x64.
UNINITIALIZED_TEST(StackUnwindingWinX64) {
#ifdef V8_WIN64_UNWINDING_INFO

  static const char* unwinding_win_x64_test_source =
      "function start(count) {\n"
      "  for (var i = 0; i < count; i++) {\n"
      "    var o = instance.foo;\n"
      "    instance.foo = o + 1;\n"
      "  }\n"
      "};\n"
      "%PrepareFunctionForOptimization(start);\n";

  // This test may fail on Windows 7
  if (!::IsWindows8OrGreater()) {
    return;
  }

  i::FLAG_allow_natives_syntax = true;
  i::FLAG_win64_unwinding_info = true;

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  isolate->Enter();
  {
    v8::HandleScope scope(isolate);
    LocalContext env(isolate);

    v8::Local<v8::FunctionTemplate> func_template =
        v8::FunctionTemplate::New(isolate);
    v8::Local<v8::ObjectTemplate> instance_template =
        func_template->InstanceTemplate();

    UnwindingWinX64Callbacks accessors;
    v8::Local<v8::External> data = v8::External::New(isolate, &accessors);
    instance_template->SetAccessor(v8_str("foo"),
                                   &UnwindingWinX64Callbacks::Getter,
                                   &UnwindingWinX64Callbacks::Setter, data);
    v8::Local<v8::Function> func =
        func_template->GetFunction(env.local()).ToLocalChecked();
    v8::Local<v8::Object> instance =
        func->NewInstance(env.local()).ToLocalChecked();
    env->Global()->Set(env.local(), v8_str("instance"), instance).FromJust();

    CompileRun(unwinding_win_x64_test_source);
    v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
        env->Global()->Get(env.local(), v8_str("start")).ToLocalChecked());

    CompileRun("%OptimizeFunctionOnNextCall(start);");

    int32_t repeat_count = 100;
    v8::Local<v8::Value> args[] = {v8::Integer::New(isolate, repeat_count)};
    function->Call(env.local(), env.local()->Global(), arraysize(args), args)
        .ToLocalChecked();
  }
  isolate->Exit();
  isolate->Dispose();

#endif  // V8_WIN64_UNWINDING_INFO
}
