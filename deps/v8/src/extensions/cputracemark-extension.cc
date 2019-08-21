// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/extensions/cputracemark-extension.h"

namespace v8 {
namespace internal {

v8::Local<v8::FunctionTemplate>
CpuTraceMarkExtension::GetNativeFunctionTemplate(v8::Isolate* isolate,
                                                 v8::Local<v8::String> str) {
  return v8::FunctionTemplate::New(isolate, CpuTraceMarkExtension::Mark);
}

void CpuTraceMarkExtension::Mark(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() < 1 || !args[0]->IsUint32()) {
    args.GetIsolate()->ThrowException(
        v8::String::NewFromUtf8(
            args.GetIsolate(),
            "First parameter to cputracemark() must be a unsigned int32.",
            NewStringType::kNormal)
            .ToLocalChecked());
  }

#if V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64

#if !V8_LIBC_MSVCRT
  // for non msvc build
  uint32_t param =
      args[0]->Uint32Value(args.GetIsolate()->GetCurrentContext()).ToChecked();

  int magic_dummy;

#if defined(__i386__) && defined(__pic__)
  __asm__ __volatile__("push %%ebx; cpuid; pop %%ebx"
                       : "=a"(magic_dummy)
                       : "a"(0x4711 | ((unsigned)(param) << 16))
                       : "ecx", "edx");
#else
  __asm__ __volatile__("cpuid"
                       : "=a"(magic_dummy)
                       : "a"(0x4711 | ((unsigned)(param) << 16))
                       : "ecx", "edx", "ebx");
#endif  // defined(__i386__) && defined(__pic__)

#else
  // no msvc build support yet.
#endif  //! V8_LIBC_MSVCRT

#endif  // V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64
}

}  // namespace internal
}  // namespace v8
